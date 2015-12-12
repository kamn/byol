#include <stdio.h>

//a buffer for input 
static char input[2048];

int main(int argc, char** argv) {
  
  //Version and Exit info
  puts("Lispy verion 0.0.0.0.1");
  puts("Press Ctrl-c to Exit\n");

  while(1) {
    //Out put prompt
    fputs("lispy> ", stdout);

    //Get user input
    fgets(input, 2048, stdin);

    printf("No you're a %s", input);
  }

  return 0;
}
