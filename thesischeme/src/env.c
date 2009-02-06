/*===========================================================================
 * env.c - environment implementation
 *
 * $Id$
===========================================================================*/

#include "scheme.h"

/*===========================================================================
  GLOBAL SCHEME CONSTANT OBJECTS
===========================================================================*/

/* Environment 
 *
 * frame (cons (var1 var2 var3 ...)
 *             (val1 val2 val3 ...))
 * environment (frame1 frame2 frame3 ... )
 */

SCM extend_environment(SCM parameters, SCM arguments, SCM env)
{
    SCM frame = new_cons(parameters, arguments);
    return new_cons(frame, env);
}

SCM* lookup_frame(SCM var, SCM frame)
{
    SCM parameters;
    SCM arguments;
    for (parameters = CAR(frame), arguments = new_cons(SCM_NULL, CDR(frame));
         CONS_P(parameters);
         parameters = CDR(parameters), arguments = CDR(arguments) )
    {
        if (EQ_P(CAR(parameters), var)) {
            return CAR_REF(CDR(arguments));
        }
    }
    if (EQ_P(parameters, var)) {
        return CAR_REF(arguments);
    }
    return NULL;
}

SCM* lookup_environment(SCM var, SCM env)
{
    SCM frame;
    SCM *value;
    while (! TOPLEVEL_ENVIRONMENT_P(env)) {
        frame = CAR(env);
        value = lookup_frame(var, frame);
        if (NULL != value) {
            return value;
        }
        env = CDR(env);
    }
    return NULL;
}

SCM symbol_value(SCM var, SCM env)
{
    SCM *value = lookup_environment(var, env);
    if (NULL == value) {
        SCM v = SYMBOL_VCELL(var);
        if (UNBOUND_P(v)) {
            scheme_error("invalid reference");
        }
        return v;
    }
    return *value;
}

/* debug print */
static void dump_frame(SCM frame)
{
    SCM parameters;
    SCM arguments;
    for (parameters = CAR(frame), arguments = CDR(frame);
         CONS_P(parameters);
         parameters = CDR(parameters), arguments = CDR(arguments) )
    {
        print(CAR(parameters), stdout);
        fprintf(stdout, " -> ");
        print(CAR(arguments), stdout);
        putchar('\n');
    }
    if (! NULL_P(parameters)) {
        print(parameters, stdout);
        fprintf(stdout, " -> ");
        print(arguments, stdout);
        putchar('\n');
    }
}
void dump_environment(SCM env)
{
    SCM frame;
    fprintf(stdout,"env-------------------\n");
    while (! TOPLEVEL_ENVIRONMENT_P(env)) {

        frame = CAR(env);
        dump_frame(frame);
        env = CDR(env);
    }
    fprintf(stdout,"env-end---------------\n");
    fflush(stdout);
}

void symbols_of_env_initialize(void)
{
}
