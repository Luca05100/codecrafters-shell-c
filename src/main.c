#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    else {
      printf("%s: command not found\n", command);
    }

  }


  return 0;
}
