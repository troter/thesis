/*===========================================================================
 * print.c - print function
 *
 * $Id$
===========================================================================*/
#include "scheme.h"

static SCM print_list(SCM sexp, FILE *file);

static SCM print_list(SCM sexp, FILE *file)
{
    SCM lst = sexp;
    putc('(', file);
    for(;;) {
        print(CAR(lst), file);
        lst = CDR(lst);
        if (NULL_P(lst)) break;
        else if (CONS_P(lst)) putc(' ', file);
        else { fprintf(file, " . "); print(lst, file); break;}
    }
    putc(')', file);
    return SCM_NULL;
}

SCM print(SCM sexp, FILE *file)
{
    if (SCM_CONSTANT_P(sexp)) {
        switch(AS_UINT(sexp)) {
        case AS_UINT(SCM_NULL):       fprintf(file, "()"); break;
        case AS_UINT(SCM_FALSE):      fprintf(file, "#f"); break;
        case AS_UINT(SCM_TRUE):       fprintf(file, "#t"); break;
        case AS_UINT(SCM_EOF):        fprintf(file, "#eof"); break;
        case AS_UINT(SCM_UNDEFINED):  fprintf(file, "#undefined"); break;
        default:
            scheme_error("bad constant"); break;
        }
        return SCM_UNDEFINED;
    }
    if (SYMBOL_P(sexp)) {
        fprintf(file, "%s", SYMBOL_NAME(sexp));
    } else if (INTEGER_P(sexp)) {
        fprintf(file, "%d", INTEGER_VALUE(sexp));
    } else if (STRING_P(sexp)) {
        fprintf(file, "\"%s\"", STRING_VALUE(sexp));
    } else if (CONS_P(sexp)) {
        print_list(sexp, file);
    } else if (PRIMITIVE_P(sexp)) {
        fprintf(file, "#<primitive %s>", PRIMITIVE_NAME(sexp));
    } else if (CLOSURE_P(sexp)) {
        fprintf(file, "#<closure>");
        print(CLOSURE_ARGS(sexp), file);
        print(CLOSURE_BODY(sexp), file);
    } else if (MACRO_P(sexp)) {
        fprintf(file, "#<macro>");
        print(MACRO_CLOSURE(sexp), file);
    } else {
        printf("print unsupported: %p", sexp);
    }
    return SCM_UNDEFINED;
}
