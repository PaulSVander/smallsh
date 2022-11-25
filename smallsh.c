#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include <fcntl.h>

void user_in(char* [], int, int*, char[], char[]);
void other_cmd(char* [], int*, int*, struct sigaction, char[], char[]);
void SIGTSTP_handler(int);
int sig_bg = 1;

int main() {
  int checkexit = 1;
  char* user_input[512] = {NULL};
  int child_exit_method = 0;
  int pid = getpid();
  char input_file[512] = "";
  char output_file[512] = "";
  int is_bg = 0;

  struct sigaction SIGINT_action = {0};
  struct sigaction SIGTSTP_action = {0};
  // Ignore SIGINT
  SIGINT_action.sa_handler = SIG_IGN;
  // Call SIGSTP_handler when SIGSTP is entered
  SIGTSTP_action.sa_handler = SIGTSTP_handler;
  // Ignore ctrl C and ctrl Z
  sigfillset(&SIGINT_action.sa_mask);
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGINT_action.sa_flags = 0;
  SIGTSTP_action.sa_flags = 0;
  sigaction(SIGINT, &SIGINT_action, NULL);
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

  while(checkexit) {
    user_in(user_input, pid, &is_bg, input_file, output_file);

    // exit handling
    if (!strcmp(user_input[0], "exit")) {
      exit(0);
    // comments, blanks handling
    } else if (user_input[0][0] == '#' || user_input[0][0] == '\0') {
      continue;
    // CD handling
    } else if (!strcmp(user_input[0], "cd")) {
      if (user_input[1]) {
        chdir(user_input[1]);
      } else {
        chdir(getenv("HOME"));
      }
    // STATUS handling
    } else if (!strcmp(user_input[0], "status")) {
      if (WIFEXITED(child_exit_method)) {
        printf("Exit value %d\n", WEXITSTATUS(child_exit_method));
      } else {
        printf("terminated by signal %d\n", WTERMSIG(child_exit_method));
      }
    } else {
      other_cmd(user_input, &child_exit_method, &is_bg, SIGINT_action, input_file, output_file);
    }

    for (int i = 0; user_input[i]; i++) {
      user_input[i] = NULL;
    }

    is_bg = 0;
    input_file[0] = '\0';
    output_file[0] = '\0';
  }
  return 0;
}

void user_in(char* input_arr[], int pid, int* is_bg, char input_file[], char output_file[]) {
  char input[2048];
  printf(": ");
  fflush(stdout);
  fgets(input, 2048, stdin);
  
  // remove newline
  input[strcspn(input, "\n")] = 0;

  // handles blank
  if (!strcmp(input, "")) {
      input_arr[0] = strdup("");
      return;
  }
  
  char* token = strtok(input, " ");

  for (int i = 0; token; i++) {

        //handles input
        if (!strcmp(token, "<")) {
            token = strtok(NULL, " ");
            strcpy(input_file, token);
                    
        //handles output
        } else if (!strcmp(token, ">")) {
            token = strtok(NULL, " ");
            strcpy(output_file, token);
                  
        //handles checking for background
        } else if (!strcmp(token, "&")) {

            *is_bg = 1;

        } else { 
            input_arr[i] = strdup(token);
            //handles expansion of variable $$
            for (int j = 0; input_arr[i][j]; j++) {
              if (input_arr[i][j] == '$' && input_arr[i][j+1] == '$') {
                input_arr[i][j] = '\0';
                snprintf(input_arr[i], 256, "%s%d", input_arr[i], pid);
              }
            }
        }
        
        token = strtok(NULL, " ");
    }

}

void other_cmd(char *arg_arr[], int* child_exit_status, int* is_bg, struct sigaction sigint_action, char input_file_name[], char output_file_name[]) {
    
    int input, output;
    pid_t spawn_pid = -5;
    
    // *****if successful child spawn_pid = 0, parent spawn_pid = child pid*****
    spawn_pid = fork();
    alarm(300);
    switch (spawn_pid) {
        
        case -1:
            perror("Error!\n");
            exit(1);
            break;
        
        // ********CODE EXECUTED BY CHILD********
        case 0:
            // Ctrl-C will have default behavior now
            sigint_action.sa_handler = SIG_DFL;
            sigaction(SIGINT, &sigint_action, NULL);
            //input handling
            if (strcmp(input_file_name, "")) {
                input = open(input_file_name, O_RDONLY);
                if (input == -1) {
                    perror("Error opening input file!\n");
                    exit(1);
                }
                //redirect via stdin
                dup2(input, 0);
                fcntl(input, F_SETFD, FD_CLOEXEC);
            }
            //output handling
            if (strcmp(output_file_name, "")) {
                output = open(output_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (output == -1) {
                    perror("Error opening output file!\n");
                }
                //rediret via stdout
                dup2(output, 1);
                fcntl(output, F_SETFD, FD_CLOEXEC);
            }

            if (execvp(arg_arr[0], arg_arr)) {
                printf("Failure in execvp.\n%s", arg_arr[0]);
                fflush(stdout);
                exit(2);
            }
            break;
        
        // ********CODE EXECUTED BY PARENT********
        default:
            // Runs process in background if is_bg set to true and not in foreground only mode

            if (*is_bg && sig_bg) {
                waitpid(spawn_pid, child_exit_status, WNOHANG);
                printf("background pid is %d\n", spawn_pid);
                fflush(stdout);
            } else {
                waitpid(spawn_pid, child_exit_status, 0);
            }
        
        waitpid(-1, child_exit_status, WNOHANG);
    }
}

void SIGTSTP_handler(int signo) {

    // display message when entering/exiting foreground only moded and flips background bool
    if (sig_bg) {
        char* message = "Entering foreground-only mode (& is now ignored)";
        write(1, message, 49);
        fflush(stdout);
        sig_bg = 0;
    } else {
        char* message = "Exiting foreground-only mode\n";
        write(1, message, 29);
        fflush(stdout);
        sig_bg = 1;
    }
}
