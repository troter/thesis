/*===========================================================================
 * read.c - read s-expression
 *
 * $Id$
===========================================================================*/

#include "scheme.h"

/*===========================================================================
  GLOBAL SCHEME CONSTANT OBJECTS
===========================================================================*/
SCM _scm_symbol_quote;

/*
 * An extract
 * 7.1.1 Lexical structure 
 *  
 *   <delimiter> -> <whitespace> | ( | ) | " | ;
 *   <identifier> -> <initial> <subsequent>* | <peculiar identifier>
 *   <initial> -> <letter> | <special initial>
 *   <letter>  -> a | b | c | ... | z
 *
 *   <special initial> -> ! | $ | % | & | * | / | : | < | = | > | ? | ^ | _ | ~
 *   <subsequent> -> <initial> | <digit> | <special subsequent>
 *   <digit> -> 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9
 *   <special subsequent> -> + | - | . | @
 *   <peculiar identifier> + | - | ...
 */

/*
 * 7.1.2 External representations
 * 
 *   <datum> -> <simple datum> | <compound datum>
 *   <simple datum> -> <boolean> | <number> | <character> | <string> | <symbol>
 *   <symbol> -> <identifier>
 *   <compound datanum> -> <list> | <vector>
 *   <list> -> (<datum>*) | (<datum>+ . <datum>) | <abbreviation>
 *   <abbreviation> -> <abbrev prefix> <datum>
 *   <abbrev prefix> ' | ` | , | ,@
 *   <vector> -> #(<datum>*)
 */

/* reader support state
 *
 *   <boolean>     ok
 *   <number>      ok (integer only)
 *   <character>   
 *   <string>      
 *   <symbol>      ok
 *   <list>        ok
 *   <vector>      
 */

/*==================================================
  File Local Variables
==================================================*/
static char scm_initial_characters[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ" /* upper case */
"abcdefghijklmnopqrstuvwxyz" /* lower case */
"!$%&*?:<=>?^_~"             /* special initial */
;
static char scm_special_subsequent[] = "+-.@";
static char scm_delimiters[] = "()\";";

#define SCM_INITIAL_CHARACTERS_P(c) (strchr(scm_initial_characters, (c)) != NULL)
#define SCM_SPECIAL_SUBSEQUENT_P(c) (strchr(scm_special_subsequent, (c)) != NULL)
#define SCM_SUBSEQUENT_P(c) (SCM_INITIAL_CHARACTERS_P((c)) ||  \
                             isdigit((c))                  ||  \
                             SCM_SPECIAL_SUBSEQUENT_P((c)))
#define SCM_PECULIAR_IDENTIFIER_P(c) ('+' == (c) || '-' == (c) || '.' == (c))
#define SCM_DELIMITER_P(c) (isspace((c)) || strchr(scm_delimiters, (c)) != NULL)


/*==================================================
  File Local Function Prototype
==================================================*/
static int skip_comment_and_space(FILE *file);
static char* read_word(FILE *file, int first_char);
static SCM c_string_to_number(char *buf);

static SCM read_simple_datum(FILE *file, int previous);
static SCM read_symbol(FILE *file, int first_char);
static SCM read_number_or_peculiar(FILE *file, int first_char);
static SCM read_number(FILE *file, int first_char);
static SCM read_string(FILE *file);
static SCM read_character(FILE *file);

static SCM read_list(FILE *file);
static SCM read_vector(FILE *file);

SCM scm_proc_read(FILE *file);


/*==================================================
  Function Definitions
==================================================*/

SCM Scheme_read(FILE *file)
{
    return scm_proc_read(file);
}

void symbols_of_read_initialize(void)
{
    _scm_symbol_quote = intern("quote");
    scm_gc_protect(_scm_symbol_quote);
}

static int skip_comment_and_space(FILE *file)
{
    int c;
    for (;;) {
        c = fgetc(file);
        if (c == EOF)   return c;
        if (isspace(c)) continue;
        if (c == ';') { /* comment */
            for (;;) {
                c = fgetc(file);
                if (c == EOF)  return c;
                if (c == '\n') break;
            }
            continue;
        }
        return c;
    }
}

/* *notice*
 * return value needs free!
 */
static char* read_word(FILE *file, int first_char)
{
    char *buf = NULL;
    int buf_size = 100;
    int index = 0;
    buf_size = 100;
    buf = xrealloc(buf, buf_size);
    buf[index] = (char)first_char;
    index++;
    for (;;) {
        /* extend */
        if (index == buf_size) {
            buf_size *=2;
            buf = xrealloc(buf, buf_size);
        }

        buf[index] = fgetc(file);
        if (!SCM_SUBSEQUENT_P(buf[index]) || EOF == buf[index]) {
            ungetc(buf[index], file);
            buf[index] = '\0';
            return buf;
        }
        index++;
    }    
}

static SCM c_string_to_number(char *buf)
{
    int sign = 1;
    int offset = 0;
    switch (buf[offset]) {
    case '+':
        offset++;
        break;
    case '-':
        offset++;
        sign *= -1;
        break;
    }
    if ('\0' == buf[offset]) { return SCM_FALSE; }
    {
        int index = offset;
        while (buf[index]) {
            if (! isdigit(buf[index]) ) {
                return SCM_FALSE;
            }
            index++;
        }
    }
    return new_integer((int) strtol(buf, NULL,10));
}


/* R5RS library procedure read
 *   (read)
 *   (read [port])
 */
SCM scm_proc_read(FILE *file)
{
    int c = skip_comment_and_space(file);

    switch (c) {
    case '(':
        return read_list(file);
    case ')': /* List end */
        scheme_error("symtax error");
    case '[':
    case ']':
        scheme_error("unsupport bracket");
    case '{':
    case '}':
        scheme_error("unsupport brace");
    case '|':
        scheme_error("unsupport bar");
    case '#':
        c = fgetc(file);
        if ('(' == c) {
            return read_vector(file);
        } else {
            ungetc(c, file);
            return read_simple_datum(file, '#');
        }
    case '\'': /* Quotation */
        return new_cons(SCM_SYMBOL_QUOTE, new_cons(scm_proc_read(file), SCM_NULL));
    case '`':  /* Quasiquotation */
        scheme_error("unsupport quasiquotation");
    case ',':  /* (Splicing) Uuquotation */
        scheme_error("unsupport (splicing) unquotation");
    default:
        return read_simple_datum(file, c);
    }
}

/* <simple datum> -> <boolean> | <number> | <character> | <string> | <symbol>
 */
static SCM read_simple_datum(FILE *file, int previous)
{
    int c;
    c = previous == 0 ? skip_comment_and_space(file) : previous;

    if (SCM_INITIAL_CHARACTERS_P(c)) { /* <symbol> */
        return read_symbol(file, c);
    }
    if (isdigit(c)) { /* <number> */
        return read_number(file, c);
    }
    if (SCM_PECULIAR_IDENTIFIER_P(c)) { /* <number> or <symbol> */
        return read_number_or_peculiar(file, c);
    }

    switch (c) {
    case EOF:
        return SCM_EOF;
    case '#': /* <boolean> or <character> or <number prefix> */
        c = fgetc(file);
        switch (c) {
        case EOF:
            scheme_error("syntax error");
            /* <boolean> */
        case 't':    return SCM_TRUE;
        case 'f':    return SCM_FALSE;

            /* <character> */
        case '\\':
            return read_character(file);

            /* <number prefix> */
        default:
            scheme_error("unsupport number prefix");
        }
    case '"': /* <string> */
        return read_string(file);
    default:
        scheme_error("unsupport character");
    }
}


/*
 *   <identifier> -> <initial> <subsequent>* | <peculiar identifier>
 *   <initial> -> <letter> | <special initial>
 *   <letter>  -> a | b | c | ... | z
 *
 *   <special initial> -> ! | $ | % | & | * | / | : | < | = | > | ? | ^ | _ | ~
 *   <subsequent> -> <initial> | <digit> | <special subsequent>
 *   <digit> -> 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9
 *   <special subsequent> -> + | - | . | @
 */
static SCM read_symbol(FILE *file, int first_char)
{
    SCM symbol;
    char *buf = read_word(file, first_char);
    symbol = intern(buf);
    free(buf);
    return symbol;
}

static SCM read_number(FILE *file, int first_char)
{
    SCM number;
    char *buf = read_word(file, first_char);
    number = c_string_to_number(buf);
    free(buf);
    return number;
}

/*
 *   <peculiar identifier> + | - | ...
 *
 */
static SCM read_number_or_peculiar(FILE *file, int first_char)
{
    SCM number;
    char *buf = read_word(file, first_char);
    number = c_string_to_number(buf);
    if (FALSE_P(number)) {
        SCM symbol = intern(buf);
        free(buf);
        return symbol;
    }
    free(buf);
    return number;
}

static SCM read_string(FILE *file)
{
    scheme_error("unsupport string");
    return NULL;
}

static SCM read_character(FILE *file)
{
    scheme_error("unsupport character");
    return NULL;
}

/* <list> -> (<datum>*) | (<datum>+ . <datum>) | <abbreviation>
 */
static SCM read_list(FILE *file)
{
    int c;
    SCM lst = SCM_NULL;
    SCM last_pair = SCM_NULL;
    SCM datum = SCM_NULL;

    for (;;) {
        c = skip_comment_and_space(file);
        switch (c) {
        case EOF:
            goto syntax_error;
            break;

        case ')': /* end of list */
            return lst;

        case '.': /* dot pair */
            if (NULL_P(last_pair)) /* ( . <datum>) is invalid */
                goto syntax_error;
            CDR(last_pair) = scm_proc_read(file);
            c = skip_comment_and_space(file);
            if (c != ')') 
                goto syntax_error;
            return lst;
            break;

        default: /* read datum */
            ungetc(c, file);
            datum = scm_proc_read(file);
            if (NULL_P(lst)) { /* initialize list */
                lst = new_cons(datum, SCM_NULL);
                last_pair= lst;
            } else {
                CDR(last_pair) = new_cons(datum, SCM_NULL);
                last_pair = CDR(last_pair);
            }
        }
    }

 syntax_error:
    scheme_error("syntax error");
    return NULL;
}

/* <vector> -> #(<datum>*)
 */
static SCM read_vector(FILE *file)
{
    scheme_error("unsupport vector");
    return NULL;
}

