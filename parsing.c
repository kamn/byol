#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#define LASSERT(args, cond, fmt, ...) \
  if (!(cond)) { \
  lval* err = lval_err(fmt, ##__VA_ARGS__); \
  lval_del(args); return err; }

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

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

enum { LERR_DIV_ZERO, LERR_MOD_ZERO, LERR_BAD_OP, LERR_BAD_NUM};

typedef lval*(*lbuiltin)(lenv*, lval*);

typedef struct lval {
  int type;
  long num;
  
  char* err;
  char* sym;
  lbuiltin fun;

  int count;
  struct lval** cell;
} lval;

struct lenv {
  int count;
  char** syms;
  lval** vals;
};

lval* lenv_get(lenv* e, lval* k);
void lval_del(lval* v); 
lval* lval_eval_sexpr(lenv* e, lval* v);

int REPL = 1;

char* ltype_name(int t) {
  switch(t) {
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";

  }
}

lval* lval_eval(lenv* e, lval* v) {
  if(v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  
  if(v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
  return v;
}

lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
  return v;
}

//Number type for lval
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

//Error type for lval
lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start (va, fmt);

  v->err = malloc(512);

  vsnprintf(v->err, 511, fmt, va);

  v->err = realloc(v->err, strlen(v->err) + 1);

  va_end(va);

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

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_copy(lval* v) {

  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch(v->type) {
    
    case LVAL_FUN: x->fun = v->fun; break;
    case LVAL_NUM: x->num = v->num; break;

    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err); break;

    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym); break;

    case LVAL_SEXPR:
    case LVAL_QEXPR:
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);
      for(int i = 0; i < x->count; i++) {
        x->cell[i] = lval_copy(v->cell[i]);
      }
      break;
  }

  return x;
}

void lval_del(lval* v) {
  switch(v->type) {
    case LVAL_NUM: break;
    case LVAL_FUN: break;

    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;

    case LVAL_QEXPR:
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

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

lval* lenv_get(lenv* e, lval* k) {
  for(int i = 0; i < e->count; i++) {
    if(strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  return lval_err("Unbound symbol");
}

void lenv_put(lenv* e, lval* k, lval* v) {
  for(int i = 0; i < e->count; i++) {
    if(strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  e->vals[e->count-1] = lval_copy(v);
  e->syms[e->count-1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count-1], k->sym);
}

void lenv_del(lenv* e) {
  for(int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
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
  if(strcmp(t->tag, ">") == 0 &&
    (strstr(t->children[1]->tag, "sexpr") || strstr(t->children[1]->tag, "qexpr"))) {
    return lval_read(t->children[1]);
  }

  if(strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if(strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }
  
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

lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];

  memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));
  
  v->count--;

  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}



lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
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
    case LVAL_FUN:    printf("<function>"); break;
    case LVAL_ERR:    printf("Error: %s", v->err); break;
    case LVAL_SYM:    printf("%s", v->sym); break;
    case LVAL_SEXPR:  lval_expr_print(v, '(', ')'); break;
    case LVAL_QEXPR:  lval_expr_print(v, '{', '}'); break;
    default:          printf("Unknown"); break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* builtin_def(lenv* e, lval* a) {
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'def' passed incorrect type. "
    "Got %s, Expected %s.",
    ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  lval* syms = a->cell[0];

  for(int i = 0; i< syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
      "Function 'def' cannot define non-symbol. "
      "Got %s, Expected %s.",
      ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
  }

  LASSERT(a, syms->count == a->count-1,
    "Function 'def' canot define incorrect number of values to symbols. "
    "%d Symbols and %d Vales.",
    syms->count, a->count-1);

  //assign
  for(int i = 0; i < syms->count; i++) {
    lenv_put(e, syms->cell[i], a->cell[i+1]);
  }

  lval_del(a);
  return lval_sexpr();
}

lval* builtin_head(lenv* e, lval *a) {
  LASSERT(a, a->count == 1,
    "Function 'head' passed too many arguments. "
    "Got %i, Expected %i.",
    a->count, 1);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'head' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0,
    "Function 'head' passed {}");

  lval* v = lval_take(a, 0);
  while(v->count > 1) { lval_del(lval_pop(v, 1)); }
  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  LASSERT(a, a->count == 1,
    "Function 'tail' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'tail' passed incorrect type");
  LASSERT(a, a->cell[0]->count != 0,
    "Function 'tail' passed {}!");

  lval* v = lval_take(a, 0);
  lval_del(lval_pop(v, 0));
  return v;
}

lval* lval_eval(lenv* e, lval* v);

lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lenv* e, lval* a) {
  LASSERT(a, a->count == 1,
    "Function 'eval' passed too many arguments");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
    "Function 'eval' passed incorrect type");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* lval_join(lenv* e, lval* x, lval* y) {
  
  while(y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  lval_del(y);
  return x;
}

lval* builtin_join(lenv* e, lval* a) {
  for(int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
      "Function 'join' passed incorrect type");
  }
  
  lval* x = lval_pop(a, 0);
  
  while(a->count) {
    x = lval_join(e, x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin_op(lenv* e, lval* a, char* op) {
  
  //Confirm all are numbers
  for (int i = 0; i < a->count; i++) {
    if(a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number");
    }
  }

  lval* x = lval_pop(a, 0);

  if((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  while(a->count > 0) {

    lval* y = lval_pop(a, 0);
    
    if(strcmp(op, "+") == 0) { x->num += y->num; }
    if(strcmp(op, "-") == 0) { x->num -= y->num; }
    if(strcmp(op, "*") == 0) { x->num *= y->num; }
    if(strcmp(op, "/") == 0) {
      if(y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division By Zero");
      } else {
        x->num /= y->num;
      }
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

    lval_del(y);
  }
  lval_del(a);
  return x;
}

lval* builtin_add(lenv* e, lval* a) {
  return builtin_op(e, a, "+");
}

lval* builtin_minus(lenv* e, lval* a) {
  return builtin_op(e, a, "-");
}

lval* builtin_multi(lenv* e, lval* a) {
  return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
  return builtin_op(e, a, "/");
}

lval* builtin_exit(lenv* e, lval* a){
  REPL = 0;
  return a;
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_fun(func);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

void lenv_add_builtins(lenv* e) {
  //Repl functions
  lenv_add_builtin(e, "exit", builtin_exit);

  //Lang functions
  lenv_add_builtin(e, "def", builtin_def);

  //List fn
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);

  // Math fn
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_minus);
  lenv_add_builtin(e, "*", builtin_multi);
  lenv_add_builtin(e, "/", builtin_div);
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
  
  for(int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  //Erro checking
  for(int i = 0; i< v->count; i++) {
    if(v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  //empty
  if(v->count == 0) { return v; }
  //If single
  if(v->count == 1 && v->cell[0]->type != LVAL_FUN) { return lval_take(v, 0); }

  //ensure first is a symbol
  lval* f = lval_pop(v, 0);

  if(f->type != LVAL_FUN) {
    lval_del(f);
    lval_del(v);
    return lval_err("First element is not a function");
  }

  lval* result = f->fun(e, v);
  lval_del(f);
  return result;
}

int main(int argc, char** argv) {
  //Create Parsers
  mpc_parser_t* Number    = mpc_new("number");
  mpc_parser_t* Symbol    = mpc_new("symbol");
  mpc_parser_t* Sexpr     = mpc_new("sexpr");
  mpc_parser_t* Qexpr     = mpc_new("qexpr");
  mpc_parser_t* Expr      = mpc_new("expr");
  mpc_parser_t* Risky     = mpc_new("risky");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number    : /-?[0-9]+/ ;                            \
      symbol    : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;      \
      sexpr     : '(' <expr>* ')' ;                       \
      qexpr     : '{' <expr>* '}' ;                       \
      expr      : <number> | <symbol> | <sexpr>           \
                | <qexpr> ;                               \
      risky     : /^/ <sexpr> /$/ | /^/ <qexpr> /$/ ;     \
    ", Number, Symbol, Sexpr, Qexpr, Expr, Risky); 


  lenv* e = lenv_new();
  lenv_add_builtins(e);

  //Version and Exit info
  puts("Risky version 0.0.0.0.2");
  printf("Running on %s\n", SYS_NAME);
  puts("Press Ctrl-c to Exit\n");

  while(REPL) {
    //Output prompt and get input()
    char* input = readline("risky> ");

    //Add input history
    add_history(input);

    mpc_result_t r;
    if(mpc_parse("<stdin>", input, Risky, &r)) {
      //On success
      lval* x = lval_eval(e, lval_read(r.output));
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {
      //On error
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    //free memory
    free(input);
  }

  lenv_del(e);
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Risky);

  return 0;
}
