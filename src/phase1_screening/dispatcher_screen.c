#include "core.h"
#include "cpu_affinity.h"

#define NUM_CORES 1 // Specify the number of cores to use by including the -d option in the compilation parameters.
#define MAX_FILES 256

struct Worker {
    pid_t pid; // child pid
    int core_id;
    int busy; //flag
    int file_number;
    time_t start_time; 
    char last_msg[64]; // Last missions
    int jobs_done;
};

void refresh_dashboard(struct Worker *workers, int processed, int max, int active) {
    printf("\033[H\033[2J"); // Refresh Screen

    printf("==================== (Dashboard) ====================\n");
    printf("    Overall progress:[");
    int width = 40;
    int pos = (processed * width) / max;
    for(int i = 0 ; i < width ; ++i) {
        if(i < pos) printf("#");
        else printf(" ");
    }
    printf("] %d/%d (Active Core: %d)\n", processed, max, active);
    printf("====================================================================\n");
    printf(" Core | PID   | Processing    | Elapsed  | Total | Status/Last Message \n");
    printf("------+-------+-------------+-------+------+------------------------\n");

    time_t now = time(NULL);

    for(int i = 0 ; i < NUM_CORES ; i++) {
        struct Worker *w = &workers[i];

        if(w->busy){
            int elapsed = now - w->start_time;

            char *color = "\033[0m";                     // White
            if (elapsed > 3600) color = "\033[31m";      // Red
            else if (elapsed > 60) color = "\033[33m";   // Yellow

            printf("  %-3d | %-5d | res%-5d.txt | %s%4ds\033[0m  | %-4d | \033[36mProccessing..\033[0m\n", 
                w->core_id, w->pid, w->file_number, color, elapsed, w->jobs_done);
        } else {
            // Idle state demonstrate last message
            printf("  %-3d | ----- | ----------- |  ---  | %-4d | %s\n", 
                   w->core_id, w->jobs_done, w->last_msg);
        }
    }
    printf("====================================================================\n");
    fflush(stdout);
}

int main() {
    if(access("./worker", X_OK) != 0) {
        fprintf(stderr, "Cannot execute ins_check\n");
        return 1;
    }

    struct Worker workers[NUM_CORES];
    for(int i = 0; i < NUM_CORES; i++) {
        workers[i].pid = -1;
        workers[i].core_id = i;
        workers[i].busy = 0;
        workers[i].file_number = -1;
        workers[i].jobs_done = 0;
        sprintf(workers[i].last_msg, "Starting...");
    }

    int files_processed = 0;
    int current_file = 0;

    while(files_processed < MAX_FILES || current_file < MAX_FILES) {
        for(int w = 0; w < NUM_CORES; w++) {
            if(!workers[w].busy && current_file < MAX_FILES) {
                char input_filename[100];
                snprintf(input_filename, sizeof(input_filename), "results_A32/res%d.txt", current_file);
                
                if(access(input_filename, R_OK) != 0) {
                    snprintf(workers[w].last_msg, 64, "\033[33mEscape(No File): %d\033[0m", current_file);
                    current_file++;
                    continue;
                }

                pid_t pid = fork();
                if(pid < 0) {
                    perror("fork failed");
                    current_file++;
                } else if(pid == 0) {
                    if(set_cpu_affinity(getpid(), workers[w].core_id) < 0) {
                        fprintf(stderr, "Cannot set child process %d to core %d\n", 
                                getpid(), workers[w].core_id);
                    }
                    
                    char file_num_str[20];
                    snprintf(file_num_str, sizeof(file_num_str), "%d", current_file);
                    
                    execl("./worker", "worker", file_num_str, NULL);
                    perror("./worker failed!");
                    _exit(1);
                } else {
                    workers[w].pid = pid;
                    workers[w].busy = 1;
                    workers[w].file_number = current_file;
                    workers[w].start_time = time(NULL);
                    
                    current_file++;
                }
            }
        }

        for(int w = 0; w < NUM_CORES; w++) {
            if(workers[w].busy) {
                time_t current_time = time(NULL);
                int elapsed = current_time - workers[w].start_time;
                
                // Detect timeout for 2 hours
                if(elapsed > 7200) {
                    kill(workers[w].pid, SIGKILL);
                    waitpid(workers[w].pid, NULL, 0);
                    
                    snprintf(workers[w].last_msg, 64, "\033[31mTimeOut Terminate res%d\033[0m", workers[w].file_number);
                    workers[w].pid = -1;
                    workers[w].busy = 0;
                    workers[w].file_number = -1;
                    files_processed++;
                    continue;
                }
                
                // Detect Result file update time
                char result_file[256];
                snprintf(result_file, sizeof(result_file), 
                        "bitmap_results/res%d_complete.bin", workers[w].file_number);
                struct stat st;
                if (stat(result_file, &st) == 0) {
                    time_t file_age = current_time - st.st_mtime;
                    // File hasn't been update for 10 min
                    if (file_age > 600 && elapsed > 60) {
                        // FIXME: delete this printf?
                        printf("\n[Warning][Core %d] res%d.txt may be stuck(file %ld seconds not updating) PID:%d\n",
                               workers[w].core_id, workers[w].file_number, file_age, workers[w].pid);
                    }
                }

                int status;
                pid_t result = waitpid(workers[w].pid, &status, WNOHANG); // Don't block waiting.
                
                if(result > 0) {
                    // Chile process compeleted
                    if(WIFEXITED(status)) {
                        int exit_code = WEXITSTATUS(status);
                        if(exit_code == 0) {
                            snprintf(workers[w].last_msg, 64, "\033[32mCompeleted res%d\033[0m", workers[w].file_number);
                        } else if(exit_code == 10) {
                            snprintf(workers[w].last_msg, 64, "\033[31mCrashOut(SEGV) res%d\033[0m", workers[w].file_number);
                        } else {
                            snprintf(workers[w].last_msg, 64, "\033[31mFailed(Exit:%d) res%d\033[0m", exit_code, workers[w].file_number);
                        }
                    } else {
                        snprintf(workers[w].last_msg, 64, "\033[31mTerminated res%d\033[0m", workers[w].file_number);
                    }
                    
                    workers[w].pid = -1;
                    workers[w].busy = 0;
                    workers[w].file_number = -1;
                    files_processed++;
                }
            }
        }

        int active_workers = 0;
        for(int i=0; i<NUM_CORES; i++) if(workers[i].busy) active_workers++;
        
        refresh_dashboard(workers, files_processed, MAX_FILES, active_workers);

        usleep(200000);

    }
    
    printf("\n\nAll File Process Done! Total: %d files\n", files_processed);

    return 0;
}