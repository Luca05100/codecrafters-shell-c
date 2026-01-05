#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
  char command[1024];
  setbuf(stdout, NULL);
  
  while(1) {
    // TODO: Uncomment the code below to pass the first stage
    printf("$ ");
    fflush(stdout);
    if (fgets(command, sizeof(command), stdin) == NULL) {
      break;
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

    for(int i = 0; i < len; i++) { 
      char c = command[i];

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
        current_token[token_index++] = c;
      }
    }
    
    //Save last token
    if(token_index > 0) {
        current_token[token_index] = '\0';
        args[arg_count++] = strdup(current_token);
    }
    args[arg_count] = NULL; 

    if (arg_count == 0) continue;

    //exit command
    if (strcmp(args[0], "exit") == 0) {
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
    }

    //type command
    else if(strcmp(args[0], "type") == 0) {
      if (arg_count < 2) continue; // Safety check
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

   //PWD command (Modificat să folosească args[0])
    else if (strcmp(args[0], "pwd") == 0) {
      char cwd[1024];
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
      } else {
        perror("getcwd() error");
      }
    }

    // CD command (Modificat să folosească args[0] și args[1])
    else if (strcmp(args[0], "cd") == 0) {
      // Dacă există args[1] îl folosim, altfel mergem la "~"
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

        printf("%s: command not found\n", args[0]);
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
   
   for(int i=0; i<arg_count; i++) free(args[i]);

  }
  return 0;
}