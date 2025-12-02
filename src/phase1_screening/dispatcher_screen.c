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
};

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
    }

    int files_processed = 0;
    int current_file = 0;
    time_t last_status_print = time(NULL);

    printf("Testing using %d CPU cores\n", NUM_CORES);

    while(files_processed < MAX_FILES || current_file < MAX_FILES) {
        for(int w = 0; w < NUM_CORES; w++) {
            if(!workers[w].busy && current_file < MAX_FILES) {
                char input_filename[100];
                snprintf(input_filename, sizeof(input_filename), "results_A32/res%d.txt", current_file);
                
                if(access(input_filename, R_OK) != 0) {
                    printf("File %s Not exist!\n", input_filename);
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
                    
                    printf("Assign file res%d.txt to Core %d (PID: %d)\n", current_file, workers[w].core_id, pid);
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
                    printf("\n[Core %d]  Processing file res%d.txt timed out (>2h)，terminating process PID:%d\n", 
                           workers[w].core_id, workers[w].file_number, workers[w].pid);
                    
                    kill(workers[w].pid, SIGKILL);
                    waitpid(workers[w].pid, NULL, 0);
                    
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
                        printf("\n[Warning][Core %d] res%d.txt may be stuck(file %ld seconds not updating) PID:%d\n",
                               workers[w].core_id, workers[w].file_number, file_age, workers[w].pid);
                    }
                }

                int status;
                pid_t result = waitpid(workers[w].pid, &status, WNOHANG); // Don't block waiting.
                
                if(result > 0) {
                    // Chile process compeleted
                    printf("\n");
                    if(WIFEXITED(status)) {
                        int exit_code = WEXITSTATUS(status);
                        if(exit_code == 0) {
                            printf("[Core %d] compelete res%d.txt (Elapse: %ds)\n", 
                                   workers[w].core_id, workers[w].file_number, elapsed);
                        } else if(exit_code == 10) {
                            printf("[Core %d] ✗ res%d.txt consecutive SIGSEGV (Elapse: %ds)\n", 
                                   workers[w].core_id, workers[w].file_number, elapsed);
                        } else {
                            printf("[Core %d] ✗ res%d.txt failed (Exit:%d, Elapse: %ds)\n", 
                                   workers[w].core_id, workers[w].file_number, exit_code, elapsed);
                        }
                    } else {
                        printf("[Core %d] ✗ res%d.txt Terminated\n", 
                               workers[w].core_id, workers[w].file_number);
                    }
                    
                    workers[w].pid = -1;
                    workers[w].busy = 0;
                    workers[w].file_number = -1;
                    files_processed++;
                }
            }
        }

        usleep(500000); // 500ms
    }
    
    return 0;
}