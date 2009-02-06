/*
 * main.c - scheme processer
 *
 * $Id: main.c 27 2007-11-19 07:54:26Z troter $
 */

/*
 * 命名規則
 *
 * struct StructName
 * enum Scheme_hogehoge
 * macro MACRO_NAME
 * function name_hoge
 * variable name_hoge
 *
 * 命名規則そのに
 * structは名詞
 * enumの要素はタグ名がプレフィックス
 * macroは全て大文字
 * primitiveのc言語名のプレフィックスはSchemePrimitive
 */

#include "scheme.h"

/*==================================================
  File Local Macro Definitions
==================================================*/
#define SCM_PROMPT "> "

static void debug_print_sexp(char *prefix, SCM sexp)
{
    fprintf(stdout, "%s", prefix);
    print(sexp, stdout);
    putc('\n', stdout);
    fflush(stdout);
}

static SCM read_eval_print(void)
{
    SCM sexp, result;

    set_current_sexp(NULL);

    /* read */
    sexp = Scheme_read(stdin);
#if DEBUG
    debug_print_sexp("input sexp is ", sexp);
#endif
    if (EOF_P(sexp)) { return NULL; }

    /* eval */
    result = eval(sexp, TOPLEVEL_ENVIRONMENT);

    /* print */
    print(result, stdout);
    putc('\n', stdout);
    fflush(stdout);

    return result;
}

static SCM read_eval_print_loop(void)
{
    SCM result;
    
    for(;;) {
        /* show prompt */
        fprintf(stdout, SCM_PROMPT);
        result = internal_catch(read_eval_print, SCM_NULL);
        if (result == NULL) {
            return NULL;
        }
        if (get_error_message() != NULL) {
            fprintf(stdout, get_error_message());
            fputc(' ', stdout);
            if (get_current_sexp() != NULL) {
                print(get_current_sexp(), stdout);
            }
            putc('\n', stdout);
            fflush(stdout);
        }

    }
    return SCM_UNDEFINED;
}


static void usage(char *program_name)
{
    printf("Usage %s [-help] filename\n", program_name);
    return;
}

int main(int argc, char **argv)
{
    char *program_name = argv[0];
    char *filename = NULL;

    if (argc > 1) {
        if (strcmp(argv[1], "-help") == 0) {
            usage(program_name);
            return EXIT_SUCCESS;
        }
        filename = argv[1];
    }
    {
        SCHEME_STACK_INITIALIZE;
        scheme_initialize();
        if (filename) {
            fprintf(stderr, "unsupport load file\n");
            read_eval_print_loop();
        } else {
            read_eval_print_loop();
        }
        scheme_finalize();
        exit(EXIT_SUCCESS);
    }
}
