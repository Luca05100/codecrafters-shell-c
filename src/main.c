#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h> 

// List of builtin commands for autocompletion
const char *builtins[] = {"echo", "exit", "type", "pwd", "cd"};
const int num_builtins = 5;

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

// Function to replace fgets, build the line, and support TAB
int read_command_with_autocomplete(char *buffer, int max_len) {
    struct termios orig_termios;
    enable_raw_mode(&orig_termios);

    int pos = 0;
    char c;

    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == '\n') {
            // ENTER pressed
            buffer[pos] = '\n'; 
            buffer[pos + 1] = '\0';
            write(STDOUT_FILENO, "\n", 1);
            disable_raw_mode(&orig_termios);
            return 1;
        } 
        else if (c == 127 || c == 8) { 
            // BACKSPACE pressed
            if (pos > 0) {
                pos--;
                write(STDOUT_FILENO, "\b \b", 3);
            }
        } 
        else if (c == '\t') {
            // TAB pressed - Search builtins and PATH
            buffer[pos] = '\0';
            char match_name[1024] = "";
            int matches = 0;

            // 1. Search in builtins
            for (int i = 0; i < num_builtins; i++) {
                if (strncmp(builtins[i], buffer, pos) == 0) {
                    if (matches == 0) {
                        strcpy(match_name, builtins[i]);
                        matches++;
                    } else if (strcmp(match_name, builtins[i]) != 0) {
                        matches++;
                    }
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
                                if (matches == 0) {
                                    strcpy(match_name, entry->d_name);
                                    matches++;
                                } 
                                else if (strcmp(match_name, entry->d_name) != 0) {
                                    matches++;
                                }
                            }
                        }
                        closedir(dir);
                    }
                    dir_path = strtok(NULL, ":");
                }
                free(path_copy);
            }

            if (matches == 1) {
                int remaining_len = strlen(match_name) - pos;
                strncpy(&buffer[pos], match_name + pos, remaining_len);
                pos += remaining_len;
                
                buffer[pos] = ' ';
                pos++;
                
                write(STDOUT_FILENO, match_name + (pos - remaining_len - 1), remaining_len);
                write(STDOUT_FILENO, " ", 1);
            } else {
                write(STDOUT_FILENO, "\a", 1);
            }
        } 
        else {
            // Normal character
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
    // TODO: Uncomment the code below to pass the first stage
    printf("$ ");
    fflush(stdout);
    
    // Replace fgets() with our new custom read function
    if (!read_command_with_autocomplete(command, sizeof(command))) {
        break; // If it returns 0, EOF was read
    }

    // Remove newline \n
    int len = strlen(command);
    if (len > 0 && command[len - 1] == '\n') {
      command[len - 1] = '\0';
      len--; 
    }

    //verify command
    if(len == 0){
      continue;
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

    //Detect redirection
    char *output_file = NULL;
    char *error_file = NULL;
    int append_stdout = 0; 
    int append_stderr = 0; // Added for 2>>
    
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
                append_stderr = 0; // overwrite
            }
            break;
        }
        else if (strcmp(args[i], "2>>") == 0) { // Added block for 2>>
            if (i + 1 < arg_count) {
                error_file = args[i + 1];
                args[i] = NULL;
                arg_count = i;
                append_stderr = 1; // append
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
        int flags_err = O_WRONLY | O_CREAT | (append_stderr ? O_APPEND : O_TRUNC); // Modified to support append
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
      break;
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

    //type command
    else if(strcmp(args[0], "type") == 0) {
      if (arg_count < 2) {
          if (saved_stdout != -1) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
          if (saved_stderr != -1) { dup2(saved_stderr, STDERR_FILENO); close(saved_stderr); }
          continue; 
      }
      char *arg = args[1];
      
      if(strcmp(arg, "exit") == 0 ||
         strcmp(arg, "echo") == 0 ||
         strcmp(arg, "type") == 0 ||
         strcmp(arg, "pwd") == 0 ||
         strcmp(arg, "cd") == 0) {
        printf("%s is a shell builtin\n", arg);
      }
      //Path search
      else {
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

   //PWD command (Modified to use args[0])
    else if (strcmp(args[0], "pwd") == 0) {
      char cwd[1024];
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
      } else {
        perror("getcwd() error");
      }
    }

    // CD command (modified to use args[0] and args[1])
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
  
    //RUN command 
    else {

      pid_t pid = fork();
      
      if (pid == 0) {
        execvp(args[0], args);

        //if we reach here, there was an error executing the command
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

   //Cleanup memory
   for(int i=0; i < original_arg_count; i++) {
       if (args[i] != NULL) free(args[i]);
   }

  }
  return 0;
}