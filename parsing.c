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

enum { LVAL_NUM, LVAL_ERR};

enum { LERR_DIV_ZERO, LERR_MOD_ZERO, LERR_BAD_OP, LERR_BAD_NUM};

typedef struct {
  int type;
  long num;
  int err;
} lval;

//Number type for lval
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

//Error type for lval
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

void lval_print(lval v) {
  switch(v.type) {
    case LVAL_NUM: printf("%li", v.num); break;

    case LVAL_ERR:
      if(v.err == LERR_DIV_ZERO) {
        printf("Error: Division By Zero!");
      }
      if(v.err == LERR_BAD_OP) {
        printf("Error: Invalid Operator!");
      }
      if(v.err == LERR_BAD_NUM) {
        printf("Error: Invalid Number!");
      }
      if(v.err == LERR_MOD_ZERO) {
        printf("Error: Modulo By Zero!");
      }
    break;
  }
}

void lval_println(lval v) { lval_print(v); putchar('\n'); }

lval eval_op(lval x, char* op, lval y) {
  
  if(x.type == LVAL_ERR) { return x; }
  if(y.type == LVAL_ERR) { return y; }

  if(strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
  if(strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
  if(strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
  if(strcmp(op, "/") == 0) {
    return y.num == 0
      ? lval_err(LERR_DIV_ZERO)
      : lval_num(x.num / y.num); 
  }

  //TODO: Add lval stuff
  if(strcmp(op, "%") == 0) { 
    return y.num == 0
      ? lval_err(LERR_MOD_ZERO)
      : lval_num(x.num % y.num);
  }
  if(strcmp(op, "^") == 0) { 
    long val = x.num;
    for(int i = 1; i < y.num; i++){
      val = val * x.num;
    }
    return lval_num(val);
  }
  if(strcmp(op, "min") == 0) { return lval_num(x.num > y.num ? y.num : x.num); }
  if(strcmp(op, "max") == 0) { return lval_num(x.num < y.num ? y.num : x.num); }

  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {

  //If it is a number return
  if(strstr(t->tag, "number")) {

    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  if(strstr(t->children[1]->tag, "sexpr")) {
    return eval(t->children[1]);
  }

  //Operator is second child
  char* op = t->children[1]->contents;
  
  //store thrid and future children in x
  lval x = eval(t->children[2]);

  //If only (- 1) then return -1
  //TODO: Fix with errors
  if(strstr(op, "-") && t->children_num == 4) {
    return lval_num(-x.num);
  }

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
  mpc_parser_t* Symbol    = mpc_new("symbol");
  mpc_parser_t* Sexpr     = mpc_new("sexpr");
  mpc_parser_t* Expr      = mpc_new("expr");
  mpc_parser_t* Risky     = mpc_new("risky");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number    : /-?[0-9]+/ ;                            \
      symbol    : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" ;                 \
      sexpr     : '(' <expr>* ')' ;                       \
      expr      : <number> | <symbol> | <sexpr> ;         \
      risky     : /^/ <expr> /$/ ;                        \
    ", Number, Symbol, Sexpr, Expr, Risky); 


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
      lval result = eval(r.output);
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      //On error
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    //free memory
    free(input);
  }

  mpc_cleanup(4, Number, Symbol, Sexpr, Expr, Risky);

  return 0;
}
