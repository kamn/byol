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

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR};

enum { LERR_DIV_ZERO, LERR_MOD_ZERO, LERR_BAD_OP, LERR_BAD_NUM};

typedef struct lval {
  int type;
  long num;
  
  char* err;
  char* sym;

  int count;
  struct lval** cell;
} lval;

//Number type for lval
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

//Error type for lval
lval* lval_err(char* m) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval* v) {
  switch(v->type) {
    case LVAL_NUM: break;
    
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;

    case LVAL_SEXPR:
      for(int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }

      free(v->cell);
    break;
  }

  free(v);
}


lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ?
    lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
  
  if(strstr(t->tag, "number")) { return lval_read_num(t); }
  if(strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  lval* x = NULL;
  //if root
  if(strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if(strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  
  for(int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }
  return x;
}


void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);

    if(i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch(v->type) {
    case LVAL_NUM:    printf("%li", v->num); break;
    case LVAL_ERR:    printf("Error: %s", v->err); break;
    case LVAL_SYM:    printf("%s", v->sym); break;
    case LVAL_SEXPR:  lval_expr_print(v, '(', ')'); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* eval_op(lval* x, char* op, lval* y) {
  
  if(x->type == LVAL_ERR) { return x; }
  if(y->type == LVAL_ERR) { return y; }

  if(strcmp(op, "+") == 0) { return lval_num(x->num + y->num); }
  if(strcmp(op, "-") == 0) { return lval_num(x->num - y->num); }
  if(strcmp(op, "*") == 0) { return lval_num(x->num * y->num); }
  if(strcmp(op, "/") == 0) {
    return y->num == 0
      ? lval_err("Divide By Zero")
      : lval_num(x->num / y->num); 
  }

  //TODO: Add lval stuff
  if(strcmp(op, "%") == 0) { 
    return y->num == 0
      ? lval_err("Modulo By Zero")
      : lval_num(x->num % y->num);
  }
  if(strcmp(op, "^") == 0) { 
    long val = x->num;
    for(int i = 1; i < y->num; i++){
      val = val * x->num;
    }
    return lval_num(val);
  }
  if(strcmp(op, "min") == 0) { return lval_num(x->num > y->num ? y->num : x->num); }
  if(strcmp(op, "max") == 0) { return lval_num(x->num < y->num ? y->num : x->num); }

  return lval_err("Bad Symbol");
}

lval* eval(mpc_ast_t* t) {

  //If it is a number return
  if(strstr(t->tag, "number")) {

    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("Bad Number");
  }

  if(strstr(t->children[1]->tag, "sexpr")) {
    return eval(t->children[1]);
  }

  //Operator is second child
  char* op = t->children[1]->contents;
  
  //store thrid and future children in x
  lval* x = eval(t->children[2]);

  //If only (- 1) then return -1
  //TODO: Fix with errors
  if(strstr(op, "-") && t->children_num == 4) {
    return lval_num(-x->num);
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
      lval* x = lval_read(r.output);
      lval_println(x);
      lval_del(x);
      //lval result = eval(r.output);
      //lval_println(result);
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
