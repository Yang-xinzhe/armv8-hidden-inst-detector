#include "core.h"
#include "cpu_affinity.h"
#include "config.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// #define NUM_CORES 4 // Removed: determined by args
#define DEFAULT_CONFIG_PATH "config/project.conf"

struct Worker {
    pid_t pid; // child pid
    int core_id;
    int busy; //flag
    int file_number;
    time_t start_time; 
    char last_msg[64]; // Last status message
    int jobs_done;
};

// Update 1: Add input_dir to dashboard arguments for better status display
void refresh_dashboard(struct Worker *workers, int num_cores, int processed, int max, int active, char *input_dir) {
    printf("\033[H\033[2J"); // Refresh Screen

    printf("==================== Dashboard ======================\n");
    // Update 2: Show config info
    printf("    Target: %s | Cores: %d\n", input_dir, num_cores);
    printf("    Overall progress: [");
    int width = 40;
    int pos = (processed * width) / max;
    for(int i = 0 ; i < width ; ++i) {
        if(i < pos) printf("#");
        else printf(" ");
    }
    printf("] %d/%d (Active: %d)\n", processed, max, active);
    printf("====================================================================\n");
    printf(" Core | PID   | Input         | Elapsed  | Total | Status/Last message \n");
    printf("------+-------+-------------+-------+------+------------------------\n");

    time_t now = time(NULL);

    for(int i = 0 ; i < num_cores ; i++) { // Use num_cores variable
        struct Worker *w = &workers[i];

        if(w->busy){
            int elapsed = now - w->start_time;

            char *color = "\033[0m";                     // White
            if (elapsed > 3600) color = "\033[31m";      // Red
            else if (elapsed > 60) color = "\033[33m";   // Yellow

            printf("  %-3d | %-5d | res%-5d.txt | %s%4ds\033[0m  | %-4d | \033[36mProcessing...\033[0m\n", 
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
    
    // Path Context
    char *platform = NULL;
    char *core = NULL;
    char *isa = NULL;
    int stage = 0;

    char *cli_input_dir = NULL;
    char *cli_output_dir = NULL;
    const char *config_path = DEFAULT_CONFIG_PATH;

    ProjectConfig cfg;
    project_config_init(&cfg);

    int opt;
    // Updated optstring: -o (old offset) -> -x; -o is free but we use -r for output override in dispatcher convention
    // But let's stick to consistent CLI for user: 
    // Dispatcher: -c cores -x offset -e worker -P plat -C core -I isa -S stage [-d input] [-r output]
    while ((opt = getopt(argc, argv, "c:x:e:P:C:I:S:d:r:f:")) != -1) {
        switch (opt) {
            case 'c': num_cores = atoi(optarg); break;
            case 'x': core_offset = atoi(optarg); break;
            case 'e': worker_path = optarg; break;
            case 'P': platform = optarg; break;
            case 'C': core = optarg; break;
            case 'I': isa = optarg; break;
            case 'S': stage = atoi(optarg); break;
            case 'd': cli_input_dir = optarg; break;
            case 'r': cli_output_dir = optarg; break;
            case 'f': config_path = optarg; break;
            default:
                fprintf(stderr, "Usage: %s -c <cores> -x <offset> -e <worker> [-P <plat> -C <core> -I <isa> -S <stage>] [-d <input>] [-r <output>]\n", argv[0]);
                return 1;
        }
    }

    /* Load config if available (CLI still takes precedence) */
    (void)project_config_load(&cfg, config_path);

    // Path Derivation Logic
    char derived_input[512] = {0};
    char derived_output[512] = {0};

    // Input Dir Resolution
    if (cli_input_dir) {
        strncpy(derived_input, cli_input_dir, sizeof(derived_input)-1);
    } else if (platform && core && isa && stage > 0) {
        if (stage == 1) {
            // Stage 1 Input: Default Seeds
            snprintf(derived_input, sizeof(derived_input), "experiments/inputs/%s_undef_seeds", isa);
        } else if (stage == 2) {
            // Stage 2 Input: Output of Stage 1
            snprintf(derived_input, sizeof(derived_input), "experiments/targets/%s/%s/%s/01_screening", platform, core, isa);
        }
    } else {
         // Fallback to legacy config if available
         if (stage == 1 && cfg.phase1_input_dir[0] != '\0') strncpy(derived_input, cfg.phase1_input_dir, sizeof(derived_input)-1);
         else if (stage == 2 && cfg.phase2_input_dir[0] != '\0') strncpy(derived_input, cfg.phase2_input_dir, sizeof(derived_input)-1);
    }

    // Output Dir Resolution
    if (cli_output_dir) {
        strncpy(derived_output, cli_output_dir, sizeof(derived_output)-1);
    } else if (platform && core && isa && stage > 0) {
        if (stage == 1) {
             snprintf(derived_output, sizeof(derived_output), "experiments/targets/%s/%s/%s/01_screening", platform, core, isa);
        } else if (stage == 2) {
             // Assuming Arithmetic by default for Stage 2, or need finer grain?
             // For now: 02_fuzzing/arithmetic. User can override with -r if doing memory.
             snprintf(derived_output, sizeof(derived_output), "experiments/targets/%s/%s/%s/02_fuzzing/arithmetic", platform, core, isa);
        }
    } else {
         if (stage == 1 && cfg.phase1_output_dir[0] != '\0') strncpy(derived_output, cfg.phase1_output_dir, sizeof(derived_output)-1);
         else if (stage == 2 && cfg.phase2_output_dir[0] != '\0') strncpy(derived_output, cfg.phase2_output_dir, sizeof(derived_output)-1);
    }

    char *input_dir = derived_input[0] ? derived_input : NULL;
    char *output_dir = derived_output[0] ? derived_output : NULL;

    // Validation: Ensure all arguments are provided
    if (num_cores <= 0 || core_offset < 0 || worker_path == NULL || input_dir == NULL) {
        fprintf(stderr, "Error: Missing required arguments or unable to derive paths.\n");
        fprintf(stderr, "Usage: %s -c <cores> -x <offset> -e <worker> -P <plat> -C <core> -I <isa> -S <stage>\n", argv[0]);
        fprintf(stderr, "Derived Input: %s\n", input_dir ? input_dir : "(null)");
        fprintf(stderr, "Derived Output: %s\n", output_dir ? output_dir : "(null)");
        return 1;
    }
    
    // Auto-create output directory
    if (output_dir) {
        char mkdir_cmd[1024];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", output_dir);
        system(mkdir_cmd);
    }

    if(access(worker_path, X_OK) != 0) {
        fprintf(stderr, "Cannot execute %s\n", worker_path);
        return 1;
    }

    int max_files = cfg.phase1_max_files > 0 ? cfg.phase1_max_files : 256;
    int timeout_seconds = cfg.phase1_timeout_seconds > 0 ? cfg.phase1_timeout_seconds : 7200;

    // VLA or malloc for workers
    struct Worker workers[num_cores];
    for(int i = 0; i < num_cores; i++) {
        workers[i].pid = -1;
        workers[i].core_id = i + core_offset;
        workers[i].busy = 0;
        workers[i].file_number = -1;
        workers[i].jobs_done = 0;
        snprintf(workers[i].last_msg, sizeof(workers[i].last_msg), "Starting...");
    }

    int files_processed = 0;
    int current_file = 0;

    while(files_processed < max_files || current_file < max_files) {
        for(int w = 0; w < num_cores; w++) {
            if(!workers[w].busy && current_file < max_files) {
                char input_filename[512];
                // For Stage 1 (Screening), input is res%d.txt
                // For Stage 2 (Fuzzing), input is candidates_%d_complete.bin
                // But we check existence to be safe.
                // The worker expects specific filenames, dispatcher just checks existence.
                
                // Heuristic: Check both possible names or just trust worker?
                // Dispatcher check is useful to skip missing files fast.
                if (stage == 1) {
                    snprintf(input_filename, sizeof(input_filename), "%s/res%d.txt", input_dir, current_file);
                } else {
                    snprintf(input_filename, sizeof(input_filename), "%s/candidates_%d_complete.bin", input_dir, current_file);
                }
                
                if(access(input_filename, R_OK) != 0) {
                    snprintf(workers[w].last_msg, 64, "\033[33mSkip (missing): %d\033[0m", current_file);
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

                    // Unified Worker Invocation: worker -i <in> -o <out> <file_num>
                    char *worker_argv[10];
                    int arg_idx = 0;
                    worker_argv[arg_idx++] = worker_path;
                    
                    if (input_dir) {
                        worker_argv[arg_idx++] = "-i";
                        worker_argv[arg_idx++] = input_dir;
                    }

                    if (output_dir) {
                        worker_argv[arg_idx++] = "-o";
                        worker_argv[arg_idx++] = output_dir;
                    }
                    
                    worker_argv[arg_idx++] = file_num_str;
                    worker_argv[arg_idx] = NULL;
                    
                    execv(worker_path, worker_argv);

                    perror("Worker exec failed");
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
                if(elapsed > timeout_seconds) {
                    kill(workers[w].pid, SIGKILL);
                    waitpid(workers[w].pid, NULL, 0);
                    
                    snprintf(workers[w].last_msg, 64, "\033[31mTimeout: killed res%d\033[0m", workers[w].file_number);
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
                            snprintf(workers[w].last_msg, 64, "\033[32mCompleted res%d\033[0m", workers[w].file_number);
                        } else if(exit_code == 10) {
                            snprintf(workers[w].last_msg, 64, "\033[31mCrashed (SIGSEGV) res%d\033[0m", workers[w].file_number);
                        } else {
                            snprintf(workers[w].last_msg, 64, "\033[31mFailed(Exit:%d) res%d\033[0m", exit_code, workers[w].file_number);
                        }
                    } else {
                        int sig = WTERMSIG(status);
                        snprintf(workers[w].last_msg, 64, "\033[31mTerminated (sig:%d) res%d\033[0m", sig, workers[w].file_number);
                        
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
        refresh_dashboard(workers, num_cores, files_processed, max_files, active_workers, input_dir);

        // Update 5: Fix infinite loop when files are skipped
        if (current_file >= max_files && active_workers == 0) {
            break;
        }

        usleep(200000);

    }
    
    printf("\n\nAll File Process Done! Total: %d files\n", files_processed);

    return 0;
}