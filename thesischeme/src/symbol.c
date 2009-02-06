/*===========================================================================
 * symbol.c - symbol table
 *
 * $Id$
===========================================================================*/
#include "scheme.h"

/* == Symbol Table Layout ==
 *
 * symbol table
 * +-----------+
 * | .---.---. |  .---.---.  .---.---.
 * | | O | O-+-+->| O | O-+->| O | O-+-> SCM_NULL
 * | `---^---' |  `-|-^---'  `-|-^---'
 * +-----------+    |          |
 * +-----------+    V          V
 * | .---.---. |  symbol     symbol
 * | | O | O | |
 * | `---^---' |
 * +-----------+
 *       :
 *       :
 *        
 */

/*==================================================
  symbol table object 
==================================================*/
SCM _symbol_table[SYMBOL_TABLE_SIZE];

/*===========================================================================
  GLOBAL SCHEME CONSTANT OBJECTS
===========================================================================*/

static long hash_string(char *name)
{
#if 0
    char *p;
    unsigned h = 0, g;
    for(p = name; *p; p++) {
        h = (h << 4) +(*p);
        if (g = h &0xf0000000) {
            h = h ^ (g >> 24);
            h = h ^ g;
        }
    }
    return h % SYMBOL_TABLE_SIZE;
#endif 
    return 0;
}

static SCM symbol_lookup(char *name)
{
    SCM list = SYMBOL_TABLE[hash_string(name)];
    SCM symbol;

    FOR_EACH(list, symbol) {
        assert(! FREE_CELL_P(symbol));
        if (strcmp(SYMBOL_NAME(symbol), name) == 0) {
            return symbol;
        }
    }
    return NULL;
}

static SCM symbol_insert(SCM symbol)
{
    int h = hash_string(SYMBOL_NAME(symbol));
    SYMBOL_TABLE[h] = new_cons(symbol, SYMBOL_TABLE[h]);
    return symbol;
}

SCM intern(char *name)
{
    SCM symbol = symbol_lookup(name);
    if (symbol == NULL) {
        symbol = symbol_insert(new_symbol(name, SCM_UNBOUND));
    }
    return symbol;
}

void symbol_table_initialize(void)
{
    int i;
    for(i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        SYMBOL_TABLE[i] = SCM_NULL;
    }
    return ;
}

void symbols_of_symbol_initialize(void)
{
}
