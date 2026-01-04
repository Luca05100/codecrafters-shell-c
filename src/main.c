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

    if (strcmp(command, "exit 0") == 0) {
      exit(0);
    }
    else {
      printf("%s: command not found\n", command);
    }

  }


  return 0;
}
