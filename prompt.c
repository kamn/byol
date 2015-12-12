#include <stdio.h>
#include <stdlib.h>


#ifdef _WIN32
#define SYS_NAME "Windows"
#elif __linux__
#define SYS_NAME "Linux"
#elif __unix__
#define SYS_NAME "Unix"
#else
#define SYS_NAME "Unknown"
#endif


//Read history hack for windows
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

//Fake readline
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

//Fake add history
void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

int main(int argc, char** argv) {
  
  //Version and Exit info
  puts("Risky verion 0.0.0.0.1");
  printf("Running on %s\n", SYS_NAME);
  puts("Press Ctrl-c to Exit\n");

  while(1) {

    //Output prompt and get input
    char* input = readline("risky> ");

    //Add input history
    add_history(input);

    printf("%s\n", input);

    //free memory
    free(input);
  }

  return 0;
}
