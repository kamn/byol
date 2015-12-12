#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

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

long eval_op(long x, char* op, long y) {
  if(strcmp(op, "+") == 0) { return x + y; }
  if(strcmp(op, "-") == 0) { return x - y; }
  if(strcmp(op, "*") == 0) { return x * y; }
  if(strcmp(op, "/") == 0) { return x / y; }
  if(strcmp(op, "%") == 0) { return x % y; }
  if(strcmp(op, "^") == 0) { 
    int val = x;
    for(int i = 1; i < y; i++){
      val = val * x;
    }
    return val;
  }
  if(strcmp(op, "min") == 0) { return x > y ? y : x; }
  if(strcmp(op, "max") == 0) { return x < y ? y : x; }
  return 0;
}

long eval(mpc_ast_t* t) {

  //If it is a number return
  if(strstr(t->tag, "number")) {
    return atoi(t->contents);
  }

  if(strstr(t->children[1]->tag, "expr")) {
    return eval(t->children[1]);
  }

  //Operator is second child
  char* op = t->children[1]->contents;
  
  //store thrid and future children in x
  long x = eval(t->children[2]);

  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}


int main(int argc, char** argv) {
  //Create Parsers
  mpc_parser_t* Number    = mpc_new("number");
  mpc_parser_t* Operator  = mpc_new("operator");
  mpc_parser_t* Expr      = mpc_new("expr");
  mpc_parser_t* Risky     = mpc_new("risky");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number    : /-?[0-9]+/ ;                            \
      operator  : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" ;                 \
      expr      : <number> | '(' <operator> <expr>+ ')' ; \
      risky     : /^/ <expr> /$/ ;                        \
    ", Number, Operator, Expr, Risky); 


  //Version and Exit info
  puts("Risky version 0.0.0.0.2");
  printf("Running on %s\n", SYS_NAME);
  puts("Press Ctrl-c to Exit\n");

  while(1) {

    //Output prompt and get input
    char* input = readline("risky> ");

    //Add input history
    add_history(input);

    mpc_result_t r;
    if(mpc_parse("<stdin>", input, Risky, &r)) {
      //On success
      long result = eval(r.output);
      printf("%li\n", result);
      mpc_ast_delete(r.output);
    } else {
      //On error
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    //free memory
    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expr, Risky);

  return 0;
}
