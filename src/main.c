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
    }

    //verify command

    if(strlen(command) == 0){
      continue;
    }

    //exit command
    if (strcmp(command, "exit 0") == 0 || strcmp(command, "exit") == 0) {
      exit(0);
      break;
    }

    //echo command
   else if(strncmp(command, "echo ", 5) == 0) {
      printf("%s\n", command + 5);
    }
    else if (strcmp(command, "echo") == 0) {
      printf("\n");
    }

    //type command
    else if(strncmp(command, "type ", 5) == 0) {
      char *arg = command + 5;
      if(strcmp(arg, "exit") == 0 ||
      strcmp(arg, "echo") == 0 ||
      strcmp(arg, "type") == 0 ||
      strcmp(arg, "pwd") == 0) {
        printf("%s is a shell builtin\n", arg);
      }
    

    //Path command
    else {
      char *env = getenv("PATH");
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
   //PWD command
    else if (strcmp(command, "pwd") == 0) {
      char cwd[1024];
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
      } else {
        perror("getcwd() error");
      }
    }

    //RUN command
    else {
      char *args[100];
      int i = 0;

      char *token = strtok(command, " ");
      while(token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
      }
      args[i] = NULL;

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

  }
  return 0;
}
