/*===========================================================================
 * error.c - scheme error handling
 *
 * $Id$
===========================================================================*/
#include <setjmp.h>

#include "scheme.h"


/*==================================================
  File Local Type Definitions
==================================================*/
struct Trap {
    struct Trap* owner;
    jmp_buf jmp;
    char *message;
};

/*==================================================
  File Local Object Definitions
==================================================*/
static char *error_message = NULL;
static struct Trap *traplist = NULL;
static SCM current_sexp = NULL;

char *get_error_message(void)
{
    return error_message;
}

void set_error_message(char *msg)
{
    free(error_message);
    error_message = strdup(msg);
}

SCM internal_catch(SCM (*func)(), SCM arg)
{
    struct Trap trap;
    SCM value = SCM_NULL;
    trap.owner = traplist;
    traplist = &trap;
    error_message = NULL;

    if (setjmp(trap.jmp) == 0) {
        value = func(arg);
    }

    traplist = trap.owner;
    return value;
}

void scheme_error(char *msg)
{
    set_error_message(msg);
    longjmp(traplist->jmp, 1);
    return ;
}

SCM get_current_sexp(void) { return current_sexp; }
void set_current_sexp(SCM sexp) { current_sexp = sexp; }
