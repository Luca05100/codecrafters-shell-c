#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h> 

#define MAX_HISTORY 1000
char *shell_history[MAX_HISTORY];
int history_count = 0;

// List of builtin commands for autocompletion
const char *builtins[] = {"echo", "exit", "type", "pwd", "cd", "history"};
const int num_builtins = 6;

// Function to check if a command is a builtin
int is_builtin(char *cmd) {
    for (int i = 0; i < num_builtins; i++) {
        if (strcmp(cmd, builtins[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Function to load history from a specific path
void load_history_from_file(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return;
    char line[1024];
    while (fgets(line, sizeof(line), file) && history_count < MAX_HISTORY) {
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (strlen(line) > 0) {
            shell_history[history_count++] = strdup(line);
        }
    }
    fclose(file);
}

// Function to execute builtin commands in a child process (for pipelines)
void execute_builtin_in_child(char **args, int arg_count) {
    if (strcmp(args[0], "echo") == 0) {
        for(int i = 1; i < arg_count; i++) {
            printf("%s", args[i]);
            if (i < arg_count - 1) {
                printf(" ");
            }
        }
        printf("\n");
        fflush(stdout);
    }
    else if (strcmp(args[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
            fflush(stdout);
        } else {
            perror("getcwd() error");
        }
    }
    else if (strcmp(args[0], "history") == 0) {
        if (arg_count >= 3 && strcmp(args[1], "-r") == 0) {
            load_history_from_file(args[2]);
        } else {
            // Handle optional limit argument
            int start_idx = 0;
            if (arg_count > 1) {
                int limit = atoi(args[1]);
                if (limit > 0 && limit < history_count) {
                    start_idx = history_count - limit;
                }
            }
            for (int i = start_idx; i < history_count; i++) {
                printf("%5d  %s\n", i + 1, shell_history[i]);
            }
            fflush(stdout);
        }
    }
    else if (strcmp(args[0], "type") == 0) {
        if (arg_count < 2) {
            return;
        }
        char *arg = args[1];

        if(is_builtin(arg)) {
            printf("%s is a shell builtin\n", arg);
            fflush(stdout);
        }
        else {
            char *env = getenv("PATH");
            if (!env) {
                printf("%s: not found\n", arg);
                fflush(stdout);
            } else {
                char *path_copy = strdup(env);
                char *dir = strtok(path_copy, ":");
                int found = 0;

                while(dir != NULL) {
                    char full_path[1024];
                    snprintf(full_path, sizeof(full_path), "%s/%s", dir, arg);

                    if (access(full_path, X_OK) == 0) {
                        printf("%s is %s\n", arg, full_path);
                        fflush(stdout);
                        found = 1;
                        break;
                    }
                    dir = strtok(NULL, ":");
                }
                free(path_copy);
                if (!found) {
                    printf("%s: not found\n", arg);
                    fflush(stdout);
                }
            }
        }
    }
}

void enable_raw_mode(struct termios *orig_termios) {
    tcgetattr(STDIN_FILENO, orig_termios);
    struct termios raw = *orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); 
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Restore terminal to normal mode
void disable_raw_mode(struct termios *orig_termios) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios);
}

// Helper for qsort
int cmpstringp(const void *p1, const void *p2) {
   return strcmp(* (char * const *) p1, * (char * const *) p2);
}

// Helper to find the common prefix of an array of strings
void get_common_prefix(char **matches, int count, char *out_prefix) {
    if (count == 0) {
        out_prefix[0] = '\0';
        return;
    }
    strcpy(out_prefix, matches[0]);
    for (int i = 1; i < count; i++) {
        int j = 0;
        while (out_prefix[j] && matches[i][j] && out_prefix[j] == matches[i][j]) {
            j++;
        }
        out_prefix[j] = '\0';
    }
}

// Function to replace fgets, build the line, and support TAB
int read_command_with_autocomplete(char *buffer, int max_len) {
    struct termios orig_termios;
    enable_raw_mode(&orig_termios);

    int pos = 0;
    char c;
    int tab_pressed = 0; // Track consecutive tabs
    int history_index = history_count; // Start at the end of history

    while (read(STDIN_FILENO, &c, 1) == 1) {
        // Handle escape sequences (like Arrow Keys)
        if (c == '\033') {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) == 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) == 0) continue;

            if (seq[0] == '[') {
                int changed = 0;
                if (seq[1] == 'A') { // Up arrow
                    if (history_index > 0) {
                        history_index--;
                        changed = 1;
                    } else {
                        write(STDOUT_FILENO, "\a", 1); // Bell
                    }
                } else if (seq[1] == 'B') { // Down arrow
                    if (history_index < history_count) {
                        history_index++;
                        changed = 1;
                    } else {
                        write(STDOUT_FILENO, "\a", 1); // Bell
                    }
                }

                if (changed) {
                    // Clear current line visually
                    while (pos > 0) {
                        write(STDOUT_FILENO, "\b \b", 3);
                        pos--;
                    }
                    
                    // Load line from history
                    if (history_index < history_count) {
                        strcpy(buffer, shell_history[history_index]);
                        pos = strlen(buffer);
                    } else {
                        buffer[0] = '\0';
                        pos = 0;
                    }
                    write(STDOUT_FILENO, buffer, pos);
                }
            }
            continue;
        }

        if (c == '\n') {
            // ENTER pressed
            buffer[pos] = '\0'; 
            write(STDOUT_FILENO, "\n", 1);
            disable_raw_mode(&orig_termios);
            return 1;
        } 
        else if (c == 127 || c == 8) { 
            // BACKSPACE pressed
            tab_pressed = 0;
            if (pos > 0) {
                pos--;
                write(STDOUT_FILENO, "\b \b", 3);
            }
        } 
        else if (c == '\t') {
            buffer[pos] = '\0';
            
            char *matches[1024];
            int match_count = 0;

            // 1. Search in builtins
            for (int i = 0; i < num_builtins; i++) {
                if (strncmp(builtins[i], buffer, pos) == 0) {
                    matches[match_count++] = strdup(builtins[i]);
                }
            }

            // 2. Search in PATH for external executables
            char *path_env = getenv("PATH");
            if (path_env != NULL) {
                char *path_copy = strdup(path_env);
                char *dir_path = strtok(path_copy, ":");
                
                while (dir_path != NULL) {
                    DIR *dir = opendir(dir_path);
                    if (dir != NULL) {
                        struct dirent *entry;
                        while ((entry = readdir(dir)) != NULL) {
                            if (strncmp(entry->d_name, buffer, pos) == 0) {
                                // Prevent duplicates
                                int is_duplicate = 0;
                                for (int j = 0; j < match_count; j++) {
                                    if (strcmp(matches[j], entry->d_name) == 0) {
                                        is_duplicate = 1;
                                        break;
                                    }
                                }
                                if (!is_duplicate && match_count < 1024) {
                                    matches[match_count++] = strdup(entry->d_name);
                                }
                            }
                        }
                        closedir(dir);
                    }
                    dir_path = strtok(NULL, ":");
                }
                free(path_copy);
            }

            if (match_count == 1) {
                int remaining_len = strlen(matches[0]) - pos;
                strncpy(&buffer[pos], matches[0] + pos, remaining_len);
                pos += remaining_len;
                
                buffer[pos] = ' ';
                pos++;
                
                write(STDOUT_FILENO, matches[0] + (pos - remaining_len - 1), remaining_len);
                write(STDOUT_FILENO, " ", 1);
                tab_pressed = 0;
            } 
            else if (match_count > 1) {
                char common_prefix[1024];
                get_common_prefix(matches, match_count, common_prefix);
                int prefix_len = strlen(common_prefix);

                // If we can partially complete the word
                if (prefix_len > pos) {
                    int add_len = prefix_len - pos;
                    strncpy(&buffer[pos], common_prefix + pos, add_len);
                    write(STDOUT_FILENO, common_prefix + pos, add_len);
                    pos += add_len;
                    tab_pressed = 0; 
                } 
                else {
                    // No partial completion possible, handle double tab logic
                    if (!tab_pressed) {
                        // First tab: bell
                        write(STDOUT_FILENO, "\a", 1);
                        tab_pressed = 1;
                    } else {
                        // Second tab: sort and print matches
                        qsort(matches, match_count, sizeof(char *), cmpstringp);
                        write(STDOUT_FILENO, "\n", 1);
                        for (int i = 0; i < match_count; i++) {
                            write(STDOUT_FILENO, matches[i], strlen(matches[i]));
                            write(STDOUT_FILENO, "  ", 2);
                        }
                        write(STDOUT_FILENO, "\n$ ", 3);
                        write(STDOUT_FILENO, buffer, pos); 
                        tab_pressed = 0;
                    }
                }
            } else {
                // No matches
                write(STDOUT_FILENO, "\a", 1);
                tab_pressed = 0;
            }
            
            // Cleanup matches
            for(int i = 0; i < match_count; i++) {
                free(matches[i]);
            }
        } 
        else {
            // Normal character
            tab_pressed = 0;
            if (pos < max_len - 2) {
                buffer[pos++] = c;
                write(STDOUT_FILENO, &c, 1); 
            }
        }
    }

    disable_raw_mode(&orig_termios);
    return 0; 
}

int main(int argc, char *argv[]) {
  char command[1024];
  setbuf(stdout, NULL);
  
  while(1) {
    printf("$ ");
    fflush(stdout);
    
    // Replace fgets() with our new custom read function
    if (!read_command_with_autocomplete(command, sizeof(command))) {
        break; // If it returns 0, EOF was read
    }

    //verify command
    int len = strlen(command);
    if(len == 0){
      continue;
    }

    // Add command to history with shifting if full
    if (history_count < MAX_HISTORY) {
        shell_history[history_count++] = strdup(command);
    } else {
        free(shell_history[0]);
        for (int i = 1; i < MAX_HISTORY; i++) {
            shell_history[i - 1] = shell_history[i];
        }
        shell_history[MAX_HISTORY - 1] = strdup(command);
    }

    //parse commands
    char *args[100];
    int arg_count = 0;

    char current_token[1024];
    int token_index = 0;
    int in_quotes_single = 0;
    int in_quotes_double = 0;
    int is_escaped = 0;

    for(int i = 0; i < len; i++) { 
      char c = command[i];
      if(is_escaped) {
        current_token[token_index++] = c;
        is_escaped = 0;
        continue;
      }
      if(c == '\\' && !in_quotes_single && !in_quotes_double) {
        is_escaped = 1;
        continue;
      }

      if(c == '\'') {
        if(in_quotes_double) {
          current_token[token_index++] = c;
        } 
        else {
          in_quotes_single = !in_quotes_single;
        }
      }

      else if(c == '"') {
        if(in_quotes_single) {
          current_token[token_index++] = c;
        }
        else {
          in_quotes_double = !in_quotes_double;
        }
      }
      else if(c == ' ' && !in_quotes_single && !in_quotes_double) {
        if(token_index > 0) {
          current_token[token_index] = '\0';
          args[arg_count++] = strdup(current_token);
          token_index = 0;
        }
      }
      else if(c == '|' && !in_quotes_single && !in_quotes_double) {
        // Handle pipe character
        if(token_index > 0) {
          current_token[token_index] = '\0';
          args[arg_count++] = strdup(current_token);
          token_index = 0;
        }
        args[arg_count++] = strdup("|");
      }
      else {
        if (in_quotes_double && c == '\\') {
          char next = command[i + 1];
          if (next == '"' || next == '\\') {
            current_token[token_index++] = next;
            i++; 
          } else {
            current_token[token_index++] = c; 
          }
        } 
        else {
          current_token[token_index++] = c;
        }
      }
    }
    
    //Save last token
    if(token_index > 0) {
        current_token[token_index] = '\0';
        args[arg_count++] = strdup(current_token);
    }
    args[arg_count] = NULL; 

    if (arg_count == 0) continue;

    // Save the original argument count before any modifications for redirection, so we can free memory correctly later
    int original_arg_count = arg_count;
    char *original_args[100];
    for (int i = 0; i < arg_count; i++) {
        original_args[i] = args[i];
    }

    // --- PIPELINE DETECTION ---
    // Moved above built-in execution so pipes intercept commands first
    int pipe_indices[50];
    int pipe_count = 0;
    for (int i = 0; i < arg_count; i++) {
        if (args[i] != NULL && strcmp(args[i], "|") == 0) {
            pipe_indices[pipe_count++] = i;
        }
    }

    if (pipe_count > 0) {
        // --- PIPELINE EXECUTION ---
        int cmd_indices[pipe_count + 2];
        cmd_indices[0] = 0;
        for (int i = 0; i < pipe_count; i++) {
            cmd_indices[i + 1] = pipe_indices[i];
        }
        cmd_indices[pipe_count + 1] = arg_count;

        int pipes[50][2];
        for (int i = 0; i < pipe_count; i++) {
            if (pipe(pipes[i]) == -1) {
                perror("pipe");
                exit(1);
            }
        }

        pid_t pids[50];
        for (int i = 0; i <= pipe_count; i++) {
            int cmd_start = cmd_indices[i];
            int cmd_end = cmd_indices[i + 1];
            
            if (cmd_start < cmd_end && args[cmd_start] != NULL && strcmp(args[cmd_start], "|") == 0) {
                cmd_start++;
            }
            
            char *cmd_args[50];
            int cmd_arg_count = 0;
            for (int j = cmd_start; j < cmd_end; j++) {
                if (args[j] != NULL && strcmp(args[j], "|") != 0) {
                    cmd_args[cmd_arg_count++] = args[j];
                }
            }
            cmd_args[cmd_arg_count] = NULL;

            if (cmd_arg_count == 0) continue;

            pids[i] = fork();
            if (pids[i] == 0) {
                if (i > 0) {
                    dup2(pipes[i - 1][0], STDIN_FILENO);
                }
                if (i < pipe_count) {
                    dup2(pipes[i][1], STDOUT_FILENO);
                }

                for (int j = 0; j < pipe_count; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                if (is_builtin(cmd_args[0])) {
                    execute_builtin_in_child(cmd_args, cmd_arg_count);
                    exit(0);
                } else {
                    execvp(cmd_args[0], cmd_args);
                    fprintf(stderr, "%s: command not found\n", cmd_args[0]);
                    exit(1);
                }
            } else if (pids[i] < 0) {
                perror("fork");
                exit(1);
            }
        }

        for (int i = 0; i < pipe_count; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }

        for (int i = 0; i <= pipe_count; i++) {
            int status;
            waitpid(pids[i], &status, 0);
        }
    } 
    else {
        // --- SINGLE COMMAND EXECUTION ---

        //Detect redirection for single commands
        char *output_file = NULL;
        char *error_file = NULL;
        int append_stdout = 0; 
        int append_stderr = 0; 
        
        for (int i = 0; i < arg_count; i++) {
            if (strcmp(args[i], ">") == 0 || strcmp(args[i], "1>") == 0) {
                if (i + 1 < arg_count) {
                    output_file = args[i + 1];
                    args[i] = NULL; 
                    arg_count = i;  
                    append_stdout = 0; 
                }
                break;
            }
            else if (strcmp(args[i], ">>") == 0 || strcmp(args[i], "1>>") == 0) {
                if (i + 1 < arg_count) {
                    output_file = args[i + 1];
                    args[i] = NULL; 
                    arg_count = i;  
                    append_stdout = 1; 
                }
                break;
            }
            else if (strcmp(args[i], "2>") == 0) {
                if (i + 1 < arg_count) {
                    error_file = args[i + 1];
                    args[i] = NULL;
                    arg_count = i;
                    append_stderr = 0; 
                }
                break;
            }
            else if (strcmp(args[i], "2>>") == 0) { 
                if (i + 1 < arg_count) {
                    error_file = args[i + 1];
                    args[i] = NULL;
                    arg_count = i;
                    append_stderr = 1; 
                }
                break;
            }
        }

        // Prepare redirection files
        int saved_stdout = -1;
        int saved_stderr = -1;

        if (output_file != NULL) {
            int flags = O_WRONLY | O_CREAT | (append_stdout ? O_APPEND : O_TRUNC);
            int fd = open(output_file, flags, 0644);
            if (fd != -1) {
                saved_stdout = dup(STDOUT_FILENO); 
                dup2(fd, STDOUT_FILENO);           
                close(fd);                         
            } else {
                perror("open error");
            }
        }

        if (error_file != NULL) {
            int flags_err = O_WRONLY | O_CREAT | (append_stderr ? O_APPEND : O_TRUNC); 
            int fd_err = open(error_file, flags_err, 0644);
            if (fd_err != -1) {
                saved_stderr = dup(STDERR_FILENO);
                dup2(fd_err, STDERR_FILENO);
                close(fd_err);
            } else {
                perror("open error");
            }
        }

        //exit command
        if (strcmp(args[0], "exit") == 0) {
          if (saved_stdout != -1) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
          if (saved_stderr != -1) { dup2(saved_stderr, STDERR_FILENO); close(saved_stderr); }
          exit(0);
        }
        //echo command
        else if (strcmp(args[0], "echo") == 0) {
          for(int i = 1; i < arg_count; i++) {
            printf("%s", args[i]);
            if (i < arg_count - 1) {
              printf(" ");
            }
          }
          printf("\n");
          fflush(stdout); 
        }
        //history command
        else if (strcmp(args[0], "history") == 0) {
          if (arg_count >= 3 && strcmp(args[1], "-r") == 0) {
              load_history_from_file(args[2]);
          } else {
              int start_idx = 0;
              if (arg_count > 1) {
                  int limit = atoi(args[1]);
                  if (limit > 0 && limit < history_count) {
                      start_idx = history_count - limit;
                  }
              }
              for (int i = start_idx; i < history_count; i++) {
                  printf("%5d  %s\n", i + 1, shell_history[i]);
              }
              fflush(stdout);
          }
        }
        //type command
        else if(strcmp(args[0], "type") == 0) {
          if (arg_count >= 2) {
              char *arg = args[1];
              if(is_builtin(arg)) {
                printf("%s is a shell builtin\n", arg);
              } else {
                char *env = getenv("PATH");
                if (!env) {
                     printf("%s: not found\n", arg);
                } else {
                    char *path_copy = strdup(env);
                    char *dir = strtok(path_copy, ":");
                    int found = 0;

                    while(dir != NULL) {
                        char full_path[1024];
                        snprintf(full_path, sizeof(full_path), "%s/%s", dir, arg);

                        if (access(full_path, X_OK) == 0) {
                            printf("%s is %s\n", arg, full_path);
                            found = 1;
                            break;
                        }
                        dir = strtok(NULL, ":");
                    }
                    free(path_copy);
                    if (!found) {
                        printf("%s: not found\n", arg);
                    }
                }
              }
          }
        }
        //PWD command
        else if (strcmp(args[0], "pwd") == 0) {
          char cwd[1024];
          if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
          } else {
            perror("getcwd() error");
          }
        }
        // CD command
        else if (strcmp(args[0], "cd") == 0) {
          char *path = (arg_count > 1) ? args[1] : "~"; 
          char expanded_path[1024];
          char *home = getenv("HOME");

          if(strcmp(path, "~") == 0) {
            strcpy(expanded_path, home);
          }
          else if (strncmp(path, "~/", 2) == 0) {
            sprintf(expanded_path, "%s%s", home, path + 1);
          } 
          else {
            strcpy(expanded_path, path);
          }
          
          if (chdir(expanded_path) != 0) {
            printf("cd: %s: No such file or directory\n", expanded_path);
          }
        }
        //RUN command (External)
        else {
          pid_t pid = fork();
          if (pid == 0) {
            execvp(args[0], args);
            fprintf(stderr, "%s: command not found\n", args[0]);
            exit(1);
          }
          else if (pid > 0 ) {
            int status;
            wait(&status);
          }
          else {
            perror("Fork failed");
          }
        }

        //restore terminal
        if (saved_stdout != -1) {
            dup2(saved_stdout, STDOUT_FILENO); 
            close(saved_stdout);              
        }
        if (saved_stderr != -1) {
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stderr);
        }
    }
   
    //Cleanup memory
    for(int i=0; i < original_arg_count; i++) {
        if (original_args[i] != NULL) free(original_args[i]);
    }

  }
  return 0;
}