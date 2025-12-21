#include "core.h"
#include "cpu_affinity.h"
#include <unistd.h>
#include <stdlib.h>

// #define NUM_CORES 4 // Removed: determined by args
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

// Update 1: Add input_dir to dashboard arguments for better status display
void refresh_dashboard(struct Worker *workers, int num_cores, int processed, int max, int active, char *input_dir) {
    printf("\033[H\033[2J"); // Refresh Screen

    printf("==================== (Dashboard) ====================\n");
    // Update 2: Show config info
    printf("    Target: %s | Cores: %d\n", input_dir, num_cores);
    printf("    Overall progress:[");
    int width = 40;
    int pos = (processed * width) / max;
    for(int i = 0 ; i < width ; ++i) {
        if(i < pos) printf("#");
        else printf(" ");
    }
    printf("] %d/%d (Active: %d)\n", processed, max, active);
    printf("====================================================================\n");
    printf(" Core | PID   | Processing    | Elapsed  | Total | Status/Last Message \n");
    printf("------+-------+-------------+-------+------+------------------------\n");

    time_t now = time(NULL);

    for(int i = 0 ; i < num_cores ; i++) { // Use num_cores variable
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

int main(int argc, char *argv[]) {
    // Initialize with invalid values to force specification
    int num_cores = -1;
    int core_offset = -1;
    char *worker_path = NULL;
    char *input_dir = NULL;
    char *output_dir = "bitmap_results";

    int opt;
    while ((opt = getopt(argc, argv, "c:o:e:d:r:")) != -1) {
        switch (opt) {
            case 'c': num_cores = atoi(optarg); break;
            case 'o': core_offset = atoi(optarg); break;
            case 'e': worker_path = optarg; break;
            case 'd': input_dir = optarg; break;
            case 'r': output_dir = optarg; break;
            default:
                fprintf(stderr, "Usage: %s -c <num_cores> -o <core_offset> -e <worker_path> -d <input_dir> [-r <output_dir>]\n", argv[0]);
                return 1;
        }
    }

    // Validation: Ensure all arguments are provided
    if (num_cores <= 0 || core_offset < 0 || worker_path == NULL || input_dir == NULL) {
        fprintf(stderr, "Error: Missing required arguments.\n");
        fprintf(stderr, "Usage: %s -c <num_cores> -o <core_offset> -e <worker_path> -d <input_dir> [-r <output_dir>]\n", argv[0]);
        fprintf(stderr, "Example (A53): %s -c 4 -o 0 -e ./worker -d results_A32 -r bitmap_results_A53\n", argv[0]);
        fprintf(stderr, "Example (A72): %s -c 2 -o 4 -e ./worker -d results_A32\n", argv[0]);
        return 1;
    }

    if(access(worker_path, X_OK) != 0) {
        fprintf(stderr, "Cannot execute %s\n", worker_path);
        return 1;
    }

    // VLA or malloc for workers
    struct Worker workers[num_cores];
    for(int i = 0; i < num_cores; i++) {
        workers[i].pid = -1;
        workers[i].core_id = i + core_offset;
        workers[i].busy = 0;
        workers[i].file_number = -1;
        workers[i].jobs_done = 0;
        sprintf(workers[i].last_msg, "Starting...");
    }

    int files_processed = 0;
    int current_file = 0;

    while(files_processed < MAX_FILES || current_file < MAX_FILES) {
        for(int w = 0; w < num_cores; w++) {
            if(!workers[w].busy && current_file < MAX_FILES) {
                char input_filename[100];
                snprintf(input_filename, sizeof(input_filename), "%s/res%d.txt", input_dir, current_file);
                
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
                    
                    execl(worker_path, worker_path, file_num_str, output_dir, NULL);
                    perror("Worker execution failed!"); // Use generic error msg
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

        for(int w = 0; w < num_cores; w++) {
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
                // Note: assuming output is still in bitmap_results? 
                // If output dir depends on task, might need adjustment, but standard worker logic uses bitmap_results
                snprintf(result_file, sizeof(result_file), 
                        "%s/res%d_complete.bin", output_dir, workers[w].file_number);
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
                        int sig = WTERMSIG(status);
                        snprintf(workers[w].last_msg, 64, "\033[31mTerminated(Sig:%d) res%d\033[0m", sig, workers[w].file_number);
                        
                        fprintf(stderr, "\n[ERROR] Worker for res%d terminated by signal %d (PID %d)\n", 
                                workers[w].file_number, sig, workers[w].pid);
                        // Optional: Pause briefly to let user see it if stderr is mixed
                        usleep(500000); 
                    }
                    
                    // Update 3: Fix Total counter
                    workers[w].jobs_done++;

                    workers[w].pid = -1;
                    workers[w].busy = 0;
                    workers[w].file_number = -1;
                    files_processed++;
                }
            }
        }

        int active_workers = 0;
        for(int i=0; i<num_cores; i++) if(workers[i].busy) active_workers++;
        
        // Update 4: Pass input_dir
        refresh_dashboard(workers, num_cores, files_processed, MAX_FILES, active_workers, input_dir);

        // Update 5: Fix infinite loop when files are skipped
        if (current_file >= MAX_FILES && active_workers == 0) {
            break;
        }

        usleep(200000);

    }
    
    printf("\n\nAll File Process Done! Total: %d files\n", files_processed);

    return 0;
}