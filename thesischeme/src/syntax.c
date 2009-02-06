/*===========================================================================
 * syntax.c - scheme syntax
 *
 * $Id$
===========================================================================*/

#include "scheme.h"


SCM scm_syntax_quote(SCM sexp, struct EvalState *state)
{
    return CAR(sexp);
}

SCM scm_syntax_setq(SCM sexp, struct EvalState *state)
{
    SCM lvalue = CAR(sexp);
    SCM rsexp = CADR(sexp);
    SCM rvalue = SCM_NULL;
    SCM *ref;
    rvalue = eval(rsexp, state->env);
    ref = lookup_environment(lvalue,state->env);
    if (! (ref == NULL)) {
        *ref =rvalue;
    } else {
        SYMBOL_VCELL(lvalue) = rvalue;
    }
    return rvalue;
}

SCM scm_syntax_if(SCM sexp, struct EvalState *state)
{
    
}

SCM scm_syntax_cond(SCM sexp, struct EvalState *state)
{

}

SCM scm_syntax_lambda(SCM sexp, struct EvalState *state)
{
    return new_closure(sexp, state->env);
}

SCM scm_syntax_begin(SCM sexp, struct EvalState *state)
{
    SCM result = SCM_UNDEFINED;
    SCM kar;
    print(sexp, stdout);
    FOR_EACH(sexp, kar) {
        result = eval(kar, state->env);
        putchar('e');
        print(kar,stdout );
        putchar('\n');
    }
    return result;
}
