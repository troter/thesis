/*===========================================================================
 * eval.c - scheme interpreter core
 *
 * $Id$
===========================================================================*/
#include <setjmp.h>

#include "scheme.h"

/*==================================================
  Global 
==================================================*/
SCM _scm_symbol_else;
SCM _scm_symbol_double_arrow;

/*==================================================
  Eval Utility
==================================================*/
/* length of list */
static int length(SCM lst)
{
    int len = 0;
    SCM kar;
    FOR_EACH(lst, kar) {
        len++;
    }
    return len;
}
#if 0
static SCM reverse(SCM sexp)
{
    SCM r = SCM_NULL;
    SCM tmp;
    while(!NULL_P(sexp)) {
        tmp = new_cons(CAR(sexp), r);
        r = tmp;
        sexp = CDR(sexp);
    }
    return r;
}
#endif /* 0 */

static SCM nreverse(SCM sexp)
{
    SCM curr = sexp;
    SCM kdr;
    SCM result = SCM_NULL;
    while(CONS_P(curr)) {
        kdr = CDR(curr);
        CDR(curr) = result;
        result = curr;
        curr = kdr;
    }
    return result;
}
static SCM eval_list(SCM sexp, SCM env)
{
    SCM r = SCM_NULL;
    SCM tmp;
    while(!NULL_P(sexp)) {
        tmp = new_cons(eval(CAR(sexp), env), r);
        r = tmp;
        sexp = CDR(sexp);
    }
    return nreverse(r);
}

static SCM apply_primitive(SCM subr, SCM arg, struct EvalState *state, SCM need_argument_eval)
{
    if (SPECIAL_FORM_P(subr)) { 
        if (FALSE_P(need_argument_eval)) {
            scheme_error("can't apply/map a special form");
        }
        return PRIMITIVE_PROC(subr)(arg, state);
    } else if (LIST_EXPR_P(subr)) {
        SCM evaled_arg;
        if (FALSE_P(need_argument_eval) ) {
            evaled_arg = arg;
        } else {
            evaled_arg = eval_list(arg, state->env);
        }
        return PRIMITIVE_PROC(subr)(evaled_arg);
    } else {
        SCM result;
        SCM evaled_arg;
        if (FALSE_P(need_argument_eval) ) {
            evaled_arg = arg;
        } else {
            evaled_arg = eval_list(arg, state->env);
        }
        switch(PRIMITIVE_TYPE(subr)) {
        case PRIMITIVE_TYPE_EXPR_0:
            if (length(arg) != 0)
                goto ARG_ERROR;
            result = PRIMITIVE_PROC(subr)();
            break;
        case PRIMITIVE_TYPE_EXPR_1:
            if (length(arg) != 1)
                goto ARG_ERROR;
            result = PRIMITIVE_PROC(subr)(CAR(evaled_arg));
            break;
        case PRIMITIVE_TYPE_EXPR_2:
            if (length(arg) != 2)
                goto ARG_ERROR;
            result = PRIMITIVE_PROC(subr)(CAR(evaled_arg),
                                          CADR(evaled_arg));
            break;
        case PRIMITIVE_TYPE_EXPR_3:
            if (length(arg) != 3)
                goto ARG_ERROR;
            result = PRIMITIVE_PROC(subr)(CAR(  evaled_arg),
                                          CADR( evaled_arg),
                                          CADDR(evaled_arg) );
            break;
        case PRIMITIVE_TYPE_EXPR_4:
            if (length(arg) != 4)
                goto ARG_ERROR;
            result = PRIMITIVE_PROC(subr)(CAR(evaled_arg),
                                          CADR(evaled_arg),
                                          CADDR(evaled_arg),
                                          CAR(CDDDR(evaled_arg)));
        case PRIMITIVE_TYPE_EXPR_5:
            if (length(arg) != 5)
                goto ARG_ERROR;
        default:
            return SCM_NULL;
        }
        return result;
    ARG_ERROR:
        scheme_error("eval unsupported :");
    }
    /* */
    return SCM_NULL;
}
static SCM apply_closure(SCM closure, SCM arg, struct EvalState *state, SCM need_argument_eval)
{
    SCM closure_env = NULL;
    SCM result = SCM_NULL;
    SCM evaled_arg = SCM_NULL;
    SCM body = CLOSURE_BODY(closure);

    if (length(CLOSURE_ARGS(closure)) > length(arg)) {
        scheme_error("argument error");
    }
    if (FALSE_P(need_argument_eval) ) {
        evaled_arg = arg;
    } else {
        evaled_arg = eval_list(arg, state->env);
    }
    closure_env = extend_environment(CLOSURE_ARGS(closure), evaled_arg, CLOSURE_ENV(closure));
#if DEBUG
    printf("closure - env\n");
    fflush(stdout);
    dump_environment(closure_env);
#endif

    while (! NULL_P(body)) {
        // need_argument_evalで末尾再帰の
        if (LIST_1_P(body) && FALSE_P(need_argument_eval)) {
            state->status = EVAL_STATUS_NEED_EVAL;
            state->env=closure_env;
            result = CAR(body);
            break;
        }
        result = eval(CAR(body), closure_env);
        body = CDR(body);
    }
    
    return result;
}
static SCM apply_macro(SCM subr, SCM arg, struct EvalState *state)
{
    SCM closure = MACRO_CLOSURE(subr);
    SCM closure_body = CLOSURE_BODY(closure);
    SCM closure_arg = CLOSURE_ARGS(closure);
    SCM closure_env = CLOSURE_ENV(closure);
    SCM result = SCM_NULL;
    state->env = extend_environment(closure_arg, arg, state->env);

    while(! NULL_P(closure_body)) {
        result = eval(CAR(closure_body), state->env);
        closure_body = CDR(closure_body);
    }
    state->status = EVAL_STATUS_NEED_EVAL;
    state->env=closure_env;
    print (result, stdout);
    fflush(stdout);
    return result;
}
static SCM apply(SCM subr, SCM arg, struct EvalState *state, SCM need_argument_eval)
{
    if (PRIMITIVE_P(subr)) {
        return apply_primitive(subr, arg, state, need_argument_eval);
    } else if (CLOSURE_P(subr)) {
        return apply_closure(subr, arg, state, need_argument_eval);
    } else if (MACRO_P(subr)) {
        if (FALSE_P(need_argument_eval)) {
            scheme_error("can't apply/map a macro");
        }
        return apply_macro(subr, arg, state);
    } else {
        scheme_error("subroutine type error");
        return NULL;
    }
}

SCM eval(SCM sexp, SCM env)
{
    SCM kar = SCM_NULL;
    SCM result = NULL;
    struct EvalState state = { env, EVAL_STATUS_RETURN_VALUE};

 eval_loop:
#if DEBUG
    print(sexp, stdout);
    putchar('\n');
    dump_environment(env);
    fflush(stdout);
#endif
    set_current_sexp(sexp);

    if (SYMBOL_P(sexp)) {
#if DEBUG
        print(symbol_value(sexp, state.env), stdout);
        putchar('\n');
        fflush(stdout);
#endif
        return symbol_value(sexp, state.env);
    }
    if (! CONS_P(sexp)) {
        return sexp;
    }

    if (CONS_P(sexp)) {
        SCM subr;
        /* eval state init */
        state.status = EVAL_STATUS_RETURN_VALUE;

        kar = CAR(sexp);
        if (SYMBOL_P(kar)) {
            subr = symbol_value(kar, state.env);
        } else if (CONS_P(kar)) {
            subr = eval(kar, state.env);
        }
        result = apply(subr, CDR(sexp), &state, SCM_TRUE);
        sexp = result;
        if (state.status == EVAL_STATUS_NEED_EVAL) {
            goto eval_loop;
        }
    } 

    /* literal or result */
    return sexp;
}

/*************************************************** 
 * Special Form
 */
DEFINE_PRIMITIVE("quote", quote, (SCM sexp, struct EvalState *state), special_form)
{
    return CAR(sexp);
}
DEFINE_PRIMITIVE("set!", setq, (SCM sexp, struct EvalState *state), special_form)
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

/* symtax cond
 *
 * (cond <clause> <clause> ...)
 * 
 */
DEFINE_PRIMITIVE("cond", cond, (SCM sexp, struct EvalState *state), special_form)
{
    SCM clauses; /* (<clause> <clause> .. <clasue>) */
    SCM clause;  /* (<condition> . <body>)          */
    SCM condition;
    SCM body;
    SCM result = SCM_UNDEFINED;
    int else_break = FALSE;

    clauses = sexp;

    /* conditionが#fでない節を探す */
    while (! NULL_P(clauses)) {
        clause = CAR(clauses);
        condition = CAR(clause);

        /* conditionがelse */
        if (EQ_P(condition, SCM_SYMBOL_ELSE)) {
            if (LIST_1_P(clauses)) {
                else_break = TRUE;
                break;
            } else {
                scheme_error("syntax error");
            }
        }
        condition = eval(condition, state->env);
        if (! FALSE_P(condition)) {
            break;
        }
        clauses = CDR(clauses);
    }

    /* 最後まで見付からなかった */
    if (NULL_P(clauses)) return result;

    body = CDR(clause);

    /* (<condition> => <expr>) */
    if (EQ_P(CAR(body),SCM_SYMBOL_DOUBLE_ARROW) &&
        LIST_2_P(body)) {
        if (else_break) scheme_error("syntax error");
        return apply(eval(CADR(body), state->env), new_cons(condition, SCM_NULL), state, SCM_FALSE);
    }

    /* 節の評価 */
    while(!NULL_P(body)) {
        /* 単一の要素の場合は一旦evalに返す */
        if (LIST_1_P(body)) {
            state->status = EVAL_STATUS_NEED_EVAL;
            result = CAR(body);
            break;
        }
        result = eval(CAR(body), state->env);
        body = CDR(body);
    }
    return result;    
}
DEFINE_PRIMITIVE("lambda", lambda, (SCM sexp, struct EvalState *state), special_form)
{
    return new_closure(sexp, state->env);
}
/* (macro i (lambda ))
 */
DEFINE_PRIMITIVE("macro",  macro,  (SCM sexp, struct EvalState *state), special_form)
{
    SCM identifier = CAR(sexp);
    SCM macro = SCM_NULL;
    if (CONS_P(identifier)) {
        /* (macor (idenfifer . arg) body) */
        identifier = CAR(identifier);
        SCM arg = CDR(identifier);
        SCM body = CDR(sexp);
        macro = new_macro(new_closure(new_cons(arg, body), state->env), state->env);
        //(identifier, macro, env);
    } else {
        SCM body = CADR(sexp);
        SCM closure = SCM_NULL;
        if (! EQ_P(CAR(body), intern("lambda")) ) {
            scheme_error("macro define error");
        }
        closure = eval(CADR(sexp), state->env);
        macro = new_macro(closure, state->env);
        //(identifier, macro, env);
    }
    SYMBOL_VCELL(identifier) = macro;
    
    return macro;
}
DEFINE_PRIMITIVE("begin", begin, (SCM sexp, struct EvalState *state), special_form)
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


/*************************************************** 
 * Builtin Function
 */
DEFINE_PRIMITIVE("exit", exit, (SCM l), expr1)
{
    scheme_finalize();
    exit(INTEGER_VALUE(l));
}

DEFINE_PRIMITIVE("car", car, (SCM l), expr1)
{
    return CAR(l);
}
DEFINE_PRIMITIVE("cdr", cdr, (SCM l), expr1)
{
    return CDR(l);
}
DEFINE_PRIMITIVE("cons", cons, (SCM o1, SCM o2), expr2)
{
    return new_cons(o1, o2);
}
DEFINE_PRIMITIVE("assq", assq, (SCM o, SCM l), expr2)
{
    SCM key = o;
    SCM pairs = l;
    while (!NULL_P(pairs)) {
        SCM pair = CAR(pairs);
        if (EQ_P(CAR(pair), key)) {
            return CDR(pair);
        }
        pairs = CDR(pairs);
    }
    return SCM_NULL;
}

static SCM transposed(SCM lst)
{
    SCM kars = SCM_NULL;
    SCM kdrs = lst;
    while (1) {
        SCM iter = kdrs;
        SCM acc_kars = SCM_NULL;
        SCM acc_kdrs = SCM_NULL;
        while (! NULL_P(iter)) {
            if (NULL_P(CAR(iter))) { goto END; }
            acc_kars = new_cons(CAAR(iter), acc_kars);
            acc_kdrs = new_cons(CDAR(iter), acc_kdrs);
            iter = CDR(iter);
        }
        kars = new_cons(nreverse(acc_kars), kars);
        kdrs = nreverse(acc_kdrs);
    }
 END:
    return nreverse(kars);    
}

static SCM map(SCM subr, SCM lst)
{
    SCM r = SCM_NULL;
    lst = transposed(lst);
    SCM tmp;
    struct EvalState state = { TOPLEVEL_ENVIRONMENT, EVAL_STATUS_NEED_EVAL };
    while (!NULL_P(lst)) {
        
        tmp = new_cons(apply(subr,CAR(lst), &state, SCM_NULL), r);
        r = tmp;
        lst = CDR(lst);
    }
    return nreverse(r);
}
DEFINE_PRIMITIVE("map",  map,  (SCM l), list_expr)
{
    if (length(l) < 2)
        scheme_error("argument error :");
    return map(CAR(l), CDR(l));
}

DEFINE_PRIMITIVE("atom?", atom, (SCM o), expr1)
{
    return C_TO_SCM_BOOLEAN(! CONS_P(o));
}
DEFINE_PRIMITIVE("eq?", eq, (SCM o1, SCM o2), expr2)
{
    return C_TO_SCM_BOOLEAN(EQ_P(o1,o2));
}

DEFINE_PRIMITIVE("newline", newline, (),          expr0)
{
    putchar('\n');
    return SCM_NULL;
}

DEFINE_PRIMITIVE("eval",   eval,   (SCM sexp),              expr1)
{
    return eval(sexp, TOPLEVEL_ENVIRONMENT);
}

DEFINE_PRIMITIVE("apply",  apply,  (SCM subr, SCM arg), expr2)
{
    struct EvalState state = { TOPLEVEL_ENVIRONMENT, EVAL_STATUS_NEED_EVAL };
    return apply(subr, arg, &state, SCM_NULL);
}

DEFINE_PRIMITIVE("load",   load,   (SCM filename),          expr1)
{
    SCM sexp;
    print(filename, stdout);
    FILE *in = fopen(SYMBOL_NAME(filename), "r");
    if (in == NULL) {
        return SCM_NULL;
    }
    while(! EOF_P(sexp = Scheme_read(in))) {
        print(sexp, stdout);
        eval(sexp, TOPLEVEL_ENVIRONMENT);
    }
    fclose(in);
    return SCM_TRUE;
}

DEFINE_PRIMITIVE("intern", intern, (SCM str),               expr1)
{
    return SCM_UNDEFINED;
}

DEFINE_PRIMITIVE("symbol->string", symbol2string, (SCM symbol), expr1)
{
    return new_string(SYMBOL_NAME(symbol));
}

DEFINE_PRIMITIVE("set-car!", set_carq, (SCM lvar, SCM rvar), expr2)
{
    CAR(lvar) = rvar;
    return lvar;
}
DEFINE_PRIMITIVE("set-cdr!", set_cdrq, (SCM lvar, SCM rvar), expr2)
{
    CDR(lvar) = rvar;
    return lvar;
}

DEFINE_PRIMITIVE("<", less_than, (SCM l), list_expr)
{
    int result = 1;
    SCM kar = CAR(l);
    SCM lst = CDR(l);
    while (! NULL_P(lst)) {
        result = INTEGER_VALUE(kar) < INTEGER_VALUE(CAR(lst));
        kar = CAR(lst);
        lst = CDR(lst);
    }
    return C_TO_SCM_BOOLEAN(result);
}
DEFINE_PRIMITIVE("+", num_plus,  (SCM l), list_expr)
{
    int sum = 0;
    SCM lst = l;
    while (! NULL_P(lst)) {
        sum += INTEGER_VALUE(CAR(lst));
        lst = CDR(lst);
    }
    return new_integer(sum);
}
DEFINE_PRIMITIVE("-", num_minus, (SCM l), list_expr)
{
    int sum = 0;
    SCM lst = l;
    sum -= INTEGER_VALUE(CAR(lst));
    lst = CDR(lst);
    if (! NULL_P(lst)) { sum *= -1; }
    while (! NULL_P(lst)) {
        sum -= INTEGER_VALUE(CAR(lst));
        lst = CDR(lst);
    }
    return new_integer(sum);
}
DEFINE_PRIMITIVE("*", num_mul,   (SCM l), list_expr)
{
    int mul = 1;
    SCM lst = l;
    while (! NULL_P(lst)) {
        mul *= INTEGER_VALUE(CAR(lst));
        lst = CDR(lst);
    }
    return new_integer(mul);
}
DEFINE_PRIMITIVE("/", num_div,   (SCM l), list_expr)
{
    int mul = 0;
    SCM lst = l;
    mul = INTEGER_VALUE(CAR(lst));
    lst = CDR(lst);
    while (! NULL_P(lst)) {
        mul /= INTEGER_VALUE(CAR(lst));
        lst = CDR(lst);
    }
    return new_integer(mul);
}

void symbols_of_eval_initialize(void)
{
    _scm_symbol_else = intern("else");
    _scm_symbol_double_arrow = intern("=>");
    scm_gc_protect(_scm_symbol_else);
    scm_gc_protect(_scm_symbol_double_arrow);
    ADD_PRIMITIVE("quote",  quote,  (SCM sexp, struct EvalState *state), SPECIAL_FORM);
    ADD_PRIMITIVE("set!",   setq,   (SCM sexp, struct EvalState *state), SPECIAL_FORM);
    ADD_PRIMITIVE("cond",   cond,   (SCM sexp, struct EvalState *state), SPECIAL_FORM);
    ADD_PRIMITIVE("lambda", lambda, (SCM sexp, struct EvalState *state), SPECIAL_FORM);
    ADD_PRIMITIVE("macro",  macro,  (SCM sexp, struct EvalState *state), SPECIAL_FORM);
    ADD_PRIMITIVE("begin",  begin,  (SCM sexp, struct EvalState *state), SPECIAL_FORM);
    ADD_PRIMITIVE("exit", exit, (SCM l),              EXPR_1);

    ADD_PRIMITIVE("car",  car,  (SCM l),              EXPR_1);
    ADD_PRIMITIVE("cdr",  cdr,  (SCM l),              EXPR_1);
    ADD_PRIMITIVE("cons", cons, (SCM o1, SCM o2), EXPR_2);
    ADD_PRIMITIVE("assq", assq, (SCM o,  SCM l),  EXPR_2);
    ADD_PRIMITIVE("map",  map,  (SCM l),              LIST_EXPR);

    ADD_PRIMITIVE("atom?", atom, (SCM o),              EXPR_1);
    ADD_PRIMITIVE("eq?",   eq,   (SCM o1, SCM o2), EXPR_2);

/*     ADD_PRIMITIVE("write",   write,   (SCM o), expr1); */
/*     ADD_PRIMITIVE("display", display, (SCM o), expr1); */
    ADD_PRIMITIVE("newline", newline, (),          EXPR_0);

    ADD_PRIMITIVE("eval",   eval,   (SCM sexp),              EXPR_1);
    ADD_PRIMITIVE("apply",  apply,  (SCM subr, SCM arg), EXPR_2);
    ADD_PRIMITIVE("load",   load,   (SCM filename),          EXPR_1);

    ADD_PRIMITIVE("intern", intern, (SCM str),               EXPR_1);
    ADD_PRIMITIVE("symbol->string", symbol2string, (SCM symbol), EXPR_1);

    ADD_PRIMITIVE("set-car!", set_carq, (SCM lvar, SCM rvar), EXPR_2);
    ADD_PRIMITIVE("set-cdr!", set_cdrq, (SCM lvar, SCM rvar), EXPR_2);

    ADD_PRIMITIVE("<", less_than, (SCM l), LIST_EXPR);
    ADD_PRIMITIVE("+", num_plus,  (SCM l), LIST_EXPR);
    ADD_PRIMITIVE("-", num_minus, (SCM l), LIST_EXPR);
    ADD_PRIMITIVE("*", num_mul,   (SCM l), LIST_EXPR);
    ADD_PRIMITIVE("/", num_div,   (SCM l), LIST_EXPR);
    return ;
}



/*
(set! tak (lambda (x y z) (cond ((< y x) (tak (tak (- x 1) y z) (tak (- y 1) z x) (tak (- z 1) x y))) (else y))))
(tak 4 3 2)
4
Starting program: /home/troter/work/jmc-scheme/jmcl 
> (set! cons (lambda (x y) (lambda (m) (cond ((< 0 (- m 1)) x) (else y)))))
(set! cons (lambda (x y) (lambda (m) (cond ((< 0 (- m 1)) x) (else y)))))
#<closure>(x y)((lambda (m) (cond ((< 0 (- m 1)) x) (t y))))
> (set! car (lambda (z) (z 0)))
(set! car (lambda (z) (z 0)))
#<closure>(z)((z 0))
> (car (cons 1 2))
(car (cons 1 2))
(set! cdr (lambda (z) (z 1)))

//(set! cons (lambda (x y) (lambda (m) (cond ((eq? 'car m) x) (t y)))))
//(set! car (lambda (z) (z 'car)))
//(set! cdr (lambda (z) (z 'cdr)))
(map (lambda (x) (+ x 1)) ((lambda x x) 1 2 3 4 5))

Program received signal SIGSEGV, Segmentation fault.
eval_list (sexp=0x804b028) at eval.c:32

(set! tail-recursive
(lambda (i)
(cond ((< 1 i) (tail-recursive (- i 1)))
(t i))))
(tail-recursive 10)

(cond (t
((lambda l l)
((lambda (x) x) 10)
((lambda (y) y) 11))))

(macro user-if
(lambda (a b)
(cons 'cond (cons (cons '< (cons 'a (cons 'b '()))) (cons 'a (cons 'b '()))))))

(macro our-min
(lambda (a b)
(cons 'cond (cons 
(cons (cons '< (cons a (cons b '())))(cons a '()))
(cons (cons 'else (cons b '())) '())
))
)
)

評価用関数
マージソート
N Queen
ハノイ4本
たらいまわし
*/
