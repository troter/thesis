/*===========================================================================
 * scheme.h - scheme header
 *
 * $Id$
===========================================================================*/
#ifndef SCHEME_H
#define SCHEME_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>

#include <ctype.h>
#include <error.h>
#include <assert.h>

/* test support */
#ifdef GC
#include <gc.h>
#define GC_malloc_atomic malloc
#define GC_calloc calloc
#define GC_realloc realloc
#endif

#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif

EXTERN_C_BEGIN

/*===========================================================================
  TINY UTILITY MACRO DEFINITIONS
===========================================================================*/
/* Bool Utility */
#ifndef FALSE
#  define FALSE 0
#endif
#ifndef TRUE
#  define TRUE (!FALSE)
#endif

/* concat macro */
#define CPP_CONCAT(x, y) x##y

/* conversion macro */
#define AS_UINT(x) ((uintptr_t) (x))
#define AS_SCM(x)  ((SCM) (AS_UINT(x)))

/*===========================================================================
  TYPE DEFINITIONS
===========================================================================*/

/* scheme cell object interface */
typedef struct _Cell* SCM;

/* Internal representation of SCM Object.
 *
 *   ........|00|       pointer on an object
 *   ........|11|       constant (#t #f '() ...)
 */

/* internal type */
#define SCM_INTERNAL_REPRESENTATION_TYPE_POINTER     0 /* pointer  */
/* reserved type                                     1             */
/* reserved type                                     2             */
#define SCM_INTERNAL_REPRESENTATION_TYPE_CONSTANT    3 /* constant */

/* internal type mask */
#define SCM_INTERNAL_REPRESENTATION_MASK 0x3

/* pointer */
#define SCM_POINTER_P(o) (                           \
  (AS_UINT(o) & SCM_INTERNAL_REPRESENTATION_MASK) == \
   SCM_INTERNAL_REPRESENTATION_TYPE_POINTER)

/* constant */
#define MAKE_SCM_CONSTANT(n) (AS_SCM(n << 2 | SCM_INTERNAL_REPRESENTATION_MASK))
#define SCM_CONSTANT_P(o) (                          \
  (AS_UINT(o) & SCM_INTERNAL_REPRESENTATION_MASK) == \
   SCM_INTERNAL_REPRESENTATION_TYPE_CONSTANT)

/*==================================================
  Scheme Constant Object 
==================================================*/
/* constant definition */
#define SCM_NULL        MAKE_SCM_CONSTANT(0) /* '()          */
#define SCM_FALSE       MAKE_SCM_CONSTANT(1) /* #f           */
#define SCM_TRUE        MAKE_SCM_CONSTANT(2) /* #t           */
#define SCM_EOF         MAKE_SCM_CONSTANT(3) /* eof-object   */
#define SCM_UNDEFINED   MAKE_SCM_CONSTANT(4) /* #<undef>     */
#define SCM_UNBOUND     MAKE_SCM_CONSTANT(5) /* internal use */

/* boolean converter */
#define C_TO_SCM_BOOLEAN(condition) ((condition) ? SCM_TRUE : SCM_FALSE)
#define BOOLEAN_P(o) ((o) == SCM_TRUE || (o) == SCM_FALSE)

/* predicate */
#define EQ_P(a,b)  ((a) == (b))
#define NULL_P(obj)        (EQ_P((obj), SCM_NULL))
#define FALSE_P(obj)       (EQ_P((obj), SCM_FALSE))
#define TRUE_P(obj)        (EQ_P((obj), SCM_TRUE))
#define EOF_P(obj)         (EQ_P((obj), SCM_EOF))
#define UNDEFINED_P(obj)   (EQ_P((obj), SCM_UNDEFINED))
#define UNBOUND_P(obj)     (EQ_P((obj), SCM_UNBOUND))

/* toplevel environment */
#define TOPLEVEL_ENVIRONMENT SCM_NULL
#define TOPLEVEL_ENVIRONMENT_P(env) (EQ_P((env), TOPLEVEL_ENVIRONMENT))

/*==================================================
  Scheme Cell Object 
==================================================*/
/* scheme cell object type */
enum SchemeCellType {
    CELL_TYPE_FREE = 0,
    CELL_TYPE_CONS,
    CELL_TYPE_INTEGER,
    CELL_TYPE_SYMBOL,
    CELL_TYPE_STRING,
    CELL_TYPE_PRIMITIVE,
    CELL_TYPE_CLOSURE,
    CELL_TYPE_MACRO,
    CELL_TYPE_PORT,
};

/* scheme cell gc flag */
enum GCFlag {
    GC_FLAG_WHITE,
    GC_FLAG_GRAY,
    GC_FLAG_BLACK,
    GC_FLAG_BLACK_BLACK,
};

/* scheme built-in function type */
enum PrimitiveType {
    PRIMITIVE_TYPE_SPECIAL_FORM,
    PRIMITIVE_TYPE_LIST_EXPR,
    PRIMITIVE_TYPE_EXPR_0,
    PRIMITIVE_TYPE_EXPR_1,
    PRIMITIVE_TYPE_EXPR_2,
    PRIMITIVE_TYPE_EXPR_3,
    PRIMITIVE_TYPE_EXPR_4,
    PRIMITIVE_TYPE_EXPR_5,
};

struct _Cell {
    /* cell header */
    struct _Header {
        enum SchemeCellType type;
        short gc_flag;
    } header;

    /* cell object */
    union {
        struct _Cons {
            SCM car;
            SCM cdr;
        } cons;
        struct _Integer {
            int value;
        } integer;
        struct _Symbol {
            char *name;
            int length;
            SCM value;
        } symbol;
        struct _String {
            char *value;
            char length;
        } string;
        struct _Primitive {
            enum PrimitiveType type;
            char *name;
            SCM (*proc)();
        } primitive;
        struct _Closure {
            SCM args;
            SCM body;
            SCM env;            
        } closure;
        struct _Macro {
            SCM closure;
        } macro;
        struct _Port {
            FILE *file;
        } port;
    } object;
};

/* accessor of cell header */
#define HEADER_TYPE(obj) (((SCM) (obj))->header.type)
#define HEADER_GC_FLAG(obj) (((SCM) (obj))->header.gc_flag)

/* accessor of gc header */
#define GC_MARK(obj)           (HEADER_GC_FLAG(obj) = GC_FLAG_BLACK)
#define GC_FOREVER_MARK(obj)   (HEADER_GC_FLAG(obj) = GC_FLAG_BLACK_BLACK)
#define GC_MARK_P(obj)         (HEADER_GC_FLAG(obj) == GC_FLAG_BLACK || HEADER_GC_FLAG(obj) == GC_FLAG_BLACK_BLACK)
#define GC_FOREVER_MARK_P(obj) (HEADER_GC_FLAG(obj) == GC_FLAG_BLACK_BLACK)
#define GC_UNMARK(obj)         (HEADER_GC_FLAG(obj) = GC_FLAG_WHITE)

/* accessor of free cell */
#define FREE_CELL_P(obj) (SCM_POINTER_P(obj) && (HEADER_TYPE(obj) == CELL_TYPE_FREE))

/* accessor of cell object cons */
#define CONS_P(obj) (SCM_POINTER_P(obj) && (HEADER_TYPE(obj) == CELL_TYPE_CONS))
#define CONS_CAR(obj) (((SCM) (obj))->object.cons.car)
#define CONS_CDR(obj) (((SCM) (obj))->object.cons.cdr)
#define CONS_CAR_REF(obj) (&(CONS_CAR(obj)))
#define CONS_CDR_REF(obj) (&(CONS_CDR(obj)))
/* #define CONS_CAR_REF(obj) (&(((SCM) (obj))->object.cons.car)) */
/* #define CONS_CDR_REF(obj) (&(((SCM) (obj))->object.cons.cdr)) */

/* accessor of cell object integer */
#define INTEGER_P(obj) (SCM_POINTER_P(obj) && (HEADER_TYPE(obj) == CELL_TYPE_INTEGER))
#define INTEGER_VALUE(obj) (((SCM) (obj))->object.integer.value)

/* accessor of cell object symbol */
#define SYMBOL_P(obj) (SCM_POINTER_P(obj) && (HEADER_TYPE(obj) == CELL_TYPE_SYMBOL))
#define SYMBOL_NAME(obj)  (((SCM) (obj))->object.symbol.name)
#define SYMBOL_VCELL(obj) (((SCM) (obj))->object.symbol.value)

/* accessor of cell object string */
#define STRING_P(obj) (SCM_POINTER_P(obj) && (HEADER_TYPE(obj) == CELL_TYPE_STRING))
#define STRING_VALUE(obj)  (((SCM) (obj))->object.string.value)
#define STRING_LENGTH(obj) (((SCM) (obj))->object.string.length)

/* accessor of cell object primitive*/
#define PRIMITIVE_P(obj) (SCM_POINTER_P(obj) && (HEADER_TYPE(obj) == CELL_TYPE_PRIMITIVE))
#define PRIMITIVE_TYPE(obj) (((SCM) (obj))->object.primitive.type)
#define PRIMITIVE_NAME(obj) (((SCM) (obj))->object.primitive.name)
#define PRIMITIVE_PROC(obj) (((SCM) (obj))->object.primitive.proc)
/* predicate of primitive type */
#define PRIMITIVE_TYPE_SPECIAL_FORM_P(obj) (PRIMITIVE_TYPE(obj) == PRIMITIVE_TYPE_SPECIAL_FORM)
#define PRIMITIVE_TYPE_LIST_EXPR_P(obj) (PRIMITIVE_TYPE(obj) == PRIMITIVE_TYPE_LIST_EXPR)
#define PRIMITIVE_TYPE_EXPR_0_P(obj) (PRIMITIVE_TYPE(obj) == PRIMITIVE_TYPE_EXPR_0)
#define PRIMITIVE_TYPE_EXPR_1_P(obj) (PRIMITIVE_TYPE(obj) == PRIMITIVE_TYPE_EXPR_1)
#define PRIMITIVE_TYPE_EXPR_2_P(obj) (PRIMITIVE_TYPE(obj) == PRIMITIVE_TYPE_EXPR_2)
#define PRIMITIVE_TYPE_EXPR_3_P(obj) (PRIMITIVE_TYPE(obj) == PRIMITIVE_TYPE_EXPR_3)
#define PRIMITIVE_TYPE_EXPR_4_P(obj) (PRIMITIVE_TYPE(obj) == PRIMITIVE_TYPE_EXPR_4)
#define PRIMITIVE_TYPE_EXPR_5_P(obj) (PRIMITIVE_TYPE(obj) == PRIMITIVE_TYPE_EXPR_5)

/* accessor of cell object closure */
#define CLOSURE_P(obj) (SCM_POINTER_P(obj) && (HEADER_TYPE(obj) == CELL_TYPE_CLOSURE))
#define CLOSURE_ARGS(obj) (((SCM) (obj))->object.closure.args)
#define CLOSURE_BODY(obj) (((SCM) (obj))->object.closure.body)
#define CLOSURE_ENV(obj)  (((SCM) (obj))->object.closure.env)

/* accessor of cell object macro */
#define MACRO_P(obj) (SCM_POINTER_P(obj) && (HEADER_TYPE(obj) == CELL_TYPE_MACRO))
#define MACRO_CLOSURE(obj)  (((SCM) (obj))->object.macro.closure)

/* accessor of cell object port */
#define PORT_P(obj)  (SCM_POINTER_P(obj) && (HEADER_TYPE(obj) == CELL_TYPE_PORT))
#define PORT_FILE(obj)  (((SCM) (obj))->object.port.file)

/*==================================================
  Scheme Global Object 
==================================================*/
/* read.c */
extern SCM _scm_symbol_quote;
/* eval.c */
extern SCM _scm_symbol_else;
extern SCM _scm_symbol_double_arrow;

/* accessor of global scheme ojbect */
#define SCM_SYMBOL_QUOTE (_scm_symbol_quote)
#define SCM_SYMBOL_ELSE (_scm_symbol_else)
#define SCM_SYMBOL_DOUBLE_ARROW (_scm_symbol_double_arrow)


/*==================================================
  Eval/Apply State 
==================================================*/
enum ApplyStatus {
    APPLY_STATUS_NEED_ARGUMENT_EVAL,
    APPLY_STATUS_DONT_NEED_ARGUMENT_EVAL
};
/* From sigscheme
 * 現在の環境、evalフラグをラップする構造体。
 * これを利用することによって、グローバル変数を利用しない形で
 * 末尾再帰の最適化を行える。
 */
enum EvalStatus {
    EVAL_STATUS_NEED_EVAL,
    EVAL_STATUS_RETURN_VALUE
};

struct EvalState {
    SCM env;
    enum EvalStatus status;
};

/*==================================================
  Subroutine Definition Macro's
==================================================*/
#define EXTERN_PRIMITIVE(_scheme_name, _c_name, _c_args, _type) \
  extern struct _Cell CPP_CONCAT(Scheme_data_p_, _c_name);      \
  SCM CPP_CONCAT(Scheme_p_, _c_name) _c_args

#define DEFINE_PRIMITIVE(_scheme_name, _c_name, _c_args, _type) \
  struct _Cell CPP_CONCAT(Scheme_data_p_, _c_name);             \
  SCM CPP_CONCAT(Scheme_, _c_name) _c_args

#define ADD_PRIMITIVE(_scheme_name, _c_name, _c_args, _type)                                  \
  do {                                                                                      \
    HEADER_TYPE(&CPP_CONCAT(Scheme_data_p_, _c_name)) = CELL_TYPE_PRIMITIVE;                  \
    HEADER_GC_FLAG(&CPP_CONCAT(Scheme_data_p_, _c_name)) = 0;                                 \
    PRIMITIVE_TYPE(&CPP_CONCAT(Scheme_data_p_, _c_name)) = CPP_CONCAT(PRIMITIVE_TYPE_, _type); \
    PRIMITIVE_NAME(&CPP_CONCAT(Scheme_data_p_, _c_name)) = _scheme_name;                        \
    PRIMITIVE_PROC(&CPP_CONCAT(Scheme_data_p_, _c_name)) = CPP_CONCAT(Scheme_, _c_name);        \
    SYMBOL_VCELL(intern(_scheme_name)) = &CPP_CONCAT(Scheme_data_p_, _c_name);                  \
  } while (0)


/*===========================================================================
  TYPE UTILITY MACRO DEFINITIONS
===========================================================================*/

/*==================================================
  Short Cut Accessor
==================================================*/
/* short cut accessor of cell cons*/
#define CAR CONS_CAR
#define CDR CONS_CDR
#define CAR_REF CONS_CAR_REF
#define CDR_REF CONS_CDR_REF

/* short cut accessor of predicate of primitive type */
#define SPECIAL_FORM_P PRIMITIVE_TYPE_SPECIAL_FORM_P
#define LIST_EXPR_P PRIMITIVE_TYPE_LIST_EXPR_P
#define EXPR_0_P PRIMITIVE_TYPE_EXPR_0_P
#define EXPR_1_P PRIMITIVE_TYPE_EXPR_1_P
#define EXPR_2_P PRIMITIVE_TYPE_EXPR_2_P
#define EXPR_3_P PRIMITIVE_TYPE_EXPR_3_P
#define EXPR_4_P PRIMITIVE_TYPE_EXPR_4_P
#define EXPR_5_P PRIMITIVE_TYPE_EXPR_5_P

/*==================================================
  List Utility
==================================================*/
#define CAAR(obj) (CAR(CAR(obj)))
#define CADR(obj) (CAR(CDR(obj)))
#define CDAR(obj) (CDR(CAR(obj)))
#define CDDR(obj) (CDR(CDR(obj)))

#define CAAAR(obj) (CAR(CAR(CAR(obj))))
#define CAADR(obj) (CAR(CAR(CDR(obj))))
#define CADAR(obj) (CAR(CDR(CAR(obj))))
#define CADDR(obj) (CAR(CDR(CDR(obj))))
#define CDAAR(obj) (CDR(CAR(CAR(obj))))
#define CDADR(obj) (CDR(CAR(CDR(obj))))
#define CDDAR(obj) (CDR(CDR(CAR(obj))))
#define CDDDR(obj) (CDR(CDR(CDR(obj))))

#define LIST_1_P(obj) (CONS_P(obj) && NULL_P(CDR(obj)))
#define LIST_2_P(obj) (CONS_P(obj) && LIST_1_P(CDR(obj)))
#define LIST_3_P(obj) (CONS_P(obj) && LIST_2_P(CDR(obj)))
#define LIST_4_P(obj) (CONS_P(obj) && LIST_3_P(CDR(obj)))

#define FOR_EACH(lst, kar)           \
  if (! NULL_P(lst) && CONS_P(lst))  \
    for (; ! NULL_P(lst) && CONS_P(lst) && (kar = CAR(lst), TRUE); lst = CDR(lst))



/*===========================================================================
  SCHEME BUILT-IN FUNCTION DECLARE
===========================================================================*/
/* special form */
EXTERN_PRIMITIVE("quote",  quote,  (SCM sexp, struct EvalState *state), SPECIAL_FORM);
EXTERN_PRIMITIVE("set!",   setq,   (SCM sexp, struct EvalState *state), SPECIAL_FORM);

EXTERN_PRIMITIVE("if",     if,   (SCM sexp, struct EvalState *state), SPECIAL_FORM);
EXTERN_PRIMITIVE("cond",   cond,   (SCM sexp, struct EvalState *state), SPECIAL_FORM);
EXTERN_PRIMITIVE("lambda", lambda, (SCM sexp, struct EvalState *state), SPECIAL_FORM);
EXTERN_PRIMITIVE("macro",  macro,  (SCM sexp, struct EvalState *state), SPECIAL_FORM);
EXTERN_PRIMITIVE("begin",  begin,   (SCM sexp, struct EvalState *state), SPECIAL_FORM);

/* proc */
EXTERN_PRIMITIVE("exit", exit, (SCM l),              EXPR_1);

EXTERN_PRIMITIVE("car",  car,  (SCM l),              EXPR_1);
EXTERN_PRIMITIVE("cdr",  cdr,  (SCM l),              EXPR_1);
EXTERN_PRIMITIVE("cons", cons, (SCM o1, SCM o2), EXPR_2);
EXTERN_PRIMITIVE("assq", assq, (SCM o,  SCM l),  EXPR_2);
EXTERN_PRIMITIVE("map",  map,  (SCM l),              LIST_EXPR);

EXTERN_PRIMITIVE("atom?", atom, (SCM o),              EXPR_1);
EXTERN_PRIMITIVE("eq?",   eq,   (SCM o1, SCM o2), EXPR_2);

EXTERN_PRIMITIVE("write",   write,   (SCM o), EXPR_1);
EXTERN_PRIMITIVE("display", display, (SCM o), EXPR_1);
EXTERN_PRIMITIVE("newline", newline, (),          EXPR_0);

EXTERN_PRIMITIVE("eval",   eval,   (SCM sexp),              EXPR_1);
EXTERN_PRIMITIVE("apply",  apply,  (SCM subr, SCM arg), EXPR_2);
EXTERN_PRIMITIVE("load",   load,   (SCM filename),          EXPR_1);

EXTERN_PRIMITIVE("intern",         intern,        (SCM string), EXPR_1);
EXTERN_PRIMITIVE("symbol->string", symbol2string, (SCM symbol), EXPR_1);

EXTERN_PRIMITIVE("set-car!", set_carq, (SCM lvar, SCM rvar), EXPR_2);
EXTERN_PRIMITIVE("set-cdr!", set_cdrq, (SCM lvar, SCM rvar), EXPR_2);

EXTERN_PRIMITIVE("<", less_than, (SCM l), LIST_EXPR);
EXTERN_PRIMITIVE("+", num_plus,  (SCM l), LIST_EXPR);
EXTERN_PRIMITIVE("-", num_minus, (SCM l), LIST_EXPR);
EXTERN_PRIMITIVE("*", num_mul,   (SCM l), LIST_EXPR);
EXTERN_PRIMITIVE("/", num_div,   (SCM l), LIST_EXPR);


/*======================================================================
 * scheme.c
 */
void scheme_initialize(void);
void scheme_finalize(void);

/*======================================================================
 * alloc.c
 */

/* stack */
void stack_initialize(void *start);
#define SCHEME_STACK_INITIALIZE                        \
  SCM __dummy_scheme_stack_start_object;           \
  stack_initialize(&__dummy_scheme_stack_start_object)

/* cell allocator */
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
#define SCHEME_MALLOC(size) xmalloc((size))
void allocator_initialize(void);
void allocator_finalize(void);
void scm_gc_protect(SCM obj);
SCM new_cons(SCM car, SCM cdr);
SCM new_integer(int i);
SCM new_symbol(char *pname, SCM value);
SCM new_string(char *string);
SCM new_closure(SCM sexp, SCM env);
SCM new_macro(SCM sexp, SCM env);

/*======================================================================
 * env.c
 */
SCM extend_environment(SCM parameters, SCM arguments, SCM env);
SCM* lookup_frame(SCM var, SCM frame);
SCM* lookup_environment(SCM var, SCM env);
SCM symbol_value(SCM var, SCM env);
void symbols_of_env_initialize(void);

void dump_environment(SCM env);

/*======================================================================
 * read.c
 */
SCM Scheme_read(FILE *file);
void symbols_of_read_initialize(void);

/*======================================================================
 * print.c
 */
SCM print(SCM sexp, FILE *file);

/*======================================================================
 * eval.c
 */
SCM eval(SCM sexp, SCM env);
void symbols_of_eval_initialize(void);

/*======================================================================
 * error.c
 */
SCM internal_catch(SCM (*func)(), SCM arg);
void scheme_error(char *msg);
char *get_error_message();
void set_error_message(char *msg);
SCM get_current_sexp(void);
void set_current_sexp(SCM sexp);

/*======================================================================
 * symbol.c
 */
#define SYMBOL_TABLE_SIZE 211
extern SCM _symbol_table[SYMBOL_TABLE_SIZE];
#define SYMBOL_TABLE _symbol_table

SCM intern(char *name);
void symbol_table_initialize(void);
void symbols_of_symbol_initialize(void);


EXTERN_C_END

#endif /* ifndef SCHEME_H */
