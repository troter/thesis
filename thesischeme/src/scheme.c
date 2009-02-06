/*===========================================================================
 * scheme.c - scheme start and end code
 *
 * $Id$
===========================================================================*/

#include "scheme.h"


void scheme_initialize(void)
{
    allocator_initialize();
    symbol_table_initialize();
    symbols_of_symbol_initialize();
    symbols_of_env_initialize();
    symbols_of_read_initialize();
    symbols_of_eval_initialize();
}

void scheme_finalize(void)
{
    allocator_finalize();
    fflush(stdout);
    fflush(stderr);
}
