/*===========================================================================
 * alloc.c - cell allocator and garbage collection.
 *
 * $Id$
===========================================================================*/

#include <stdint.h>
#include <setjmp.h>

#include "scheme.h"

/*==================================================
  File Local Utility Macro Definitions
==================================================*/
/* cell constructor */
#define FREE_CELL_CONSTRUCT(obj, kar, kdr)  \
  do {                                      \
    HEADER_TYPE(obj) = CELL_TYPE_FREE;      \
    CAR(obj) = kar;                         \
    CDR(obj) = kdr;                         \
  } while (0)

#define CONS_CONSTRUCT(obj, kar, kdr)  \
  do {                                 \
    HEADER_TYPE(obj) = CELL_TYPE_CONS; \
    CAR(obj) = kar;                    \
    CDR(obj) = kdr;                    \
  } while (0)

#define INTEGER_CONSTRUCT(obj, value)     \
  do {                                    \
    HEADER_TYPE(obj) = CELL_TYPE_INTEGER; \
    INTEGER_VALUE(obj) = value;           \
  } while (0)

#define SYMBOL_CONSTRUCT(obj, name, value) \
  do {                                     \
    HEADER_TYPE(obj) = CELL_TYPE_SYMBOL;   \
    SYMBOL_NAME(obj) = strdup(pname);      \
    SYMBOL_VCELL(obj) = value;             \
  } while (0)

#define STRING_CONSTRUCT(obj, str)       \
  do {                                   \
    HEADER_TYPE(obj) = CELL_TYPE_STRING; \
    STRING_VALUE(obj) = strdup(str);     \
  } while (0)

#define CLOSURE_CONSTRUCT(obj, args, body ,env) \
  do {                                          \
    HEADER_TYPE(obj) = CELL_TYPE_CLOSURE;       \
    CLOSURE_ARGS(obj) = args;                   \
    CLOSURE_BODY(obj) = body;                   \
    CLOSURE_ENV(obj) = env;                     \
  } while (0)

#define MACRO_CONSTRUCT(obj, closure)   \
  do {                                  \
    HEADER_TYPE(obj) = CELL_TYPE_MACRO; \
    MACRO_CLOSURE(obj) = closure;       \
  } while (0)

/*==================================================
  FILE LOCAL VARIABLE DEFINITIONS
==================================================*/
/* memory page list */
static SCM page_list = NULL;
static SCM current_search_page = NULL;
/* free cell list */
static int free_cell_total_size;

/* stakc pointer */
static void *stack_start;
static void *stack_end;

/*===========================================================================
  GC support
===========================================================================*/

/*==================================================
  Stack Pointer
==================================================*/

void stack_initialize(void *start)
{
    stack_start = start;
    return ;
}

#define SCHEME_SET_STACK_END(obj) \
  do {                          \
    stack_end = obj;            \
  } while (0)


/*==================================================
  Cell Allocator
==================================================*/

/* wrapping malloc */
void *xmalloc(size_t size)
{
    void * m = malloc(size);
    if (m == NULL) error(1,0,"out of memory\n");
    return m;
}
void *xrealloc(void *ptr, size_t size)
{
    void * m = realloc(ptr, size);
    if (m == NULL) error(1,0,"out of memory\n");
    return m;
}

/* == Memory Management System Design ==
 *
 * = Page
 *
 * |--------- page object size --------------- .... ----| 
 * +-----------++-----------++-----------+
 * | .---.---. || .---.---. || .---.---. |
 * | | O | O | || | O | O | || | O | O | | 
 * | `---^---' || `---^---' || `---^---' |
 * +-----------++-----------++-----------+
 * first cell <-|-> other cell
 * 
 * first cell : page management cell
 * other cell : free cell
 *
 * 
 * = Page List
 *
 * page list is page management cell's list.
 *  
 * first page    next page
 *  .---.---.    .---.---.   
 *  | O | O-+--->| O | O-+---> NULL
 *  `-|-^---'    `-|-^---'
 *    |            |
 *    V            V
 *  free list    free list 
 *
 * free list : free cell's list
 *
 *
 * = Free Cell List
 *
 * first cell    next cell
 *  .---.---.    .---.---.   
 *  | O | O-+--->| O | O-+---> NULL
 *  `-|-^---'    `-|-^---'
 *    |            |
 *    V            V
 *  cell index   cell index
 *
 * cell index : free cell index == free list size.
 *               ((pointer)integer) value.
 *
 */

/* Page List Accesser */
#define NEXT_PAGE CDR
#define PAGES_FREE_CELL_LIST CAR

/* Free Cell List Accesser */
#define NEXT_FREE_CELL CDR
#define FREE_CELL_INDEX CAR

/* heap size */
#define ALLOCATE_HEAP_PAGE_OBJECT_SIZE 5001

/* cell allocators */
void allocator_initialize(void);
void allocator_finalize(void);
static SCM allocate_cell(void);
static SCM search_free_cell(void);
static void add_heap(void);

/* cell finalizer */
static void cell_finalize(SCM obj);

/* garbage collection */
static void scheme_gc(void);

/* for garbage collecton */
static int is_heap_object(SCM obj);
static int is_this_pages_object(SCM page, SCM obj);

/* for debug*/
static void dump_page_list(void);

/**
 * allocatorの初期化
 */
void allocator_initialize(void)
{
    page_list = NULL;
    current_search_page = page_list;
    free_cell_total_size = 0;
}

void allocator_finalize(void)
{
    SCM next_page = page_list;
    while(next_page != NULL) {
        SCM current_page = next_page;
        SCM cell = current_page;
        SCM last_cell = current_page + ALLOCATE_HEAP_PAGE_OBJECT_SIZE - 1;
        while (cell <= last_cell) {
            cell_finalize(cell);
            cell++;
        }

        next_page = NEXT_PAGE(current_page);
        free(current_page);
    }
}

/**
 * cellを確保する
 */
static SCM allocate_cell(void)
{
    SCM obj = search_free_cell();
    if (obj != NULL) {
        free_cell_total_size--;
        return obj;
    }
    scheme_gc();
    obj = search_free_cell();
    if (obj != NULL) {
        free_cell_total_size--;
        return obj;
    }
    exit(1);
}

/**
 * free listからfree cellを探す
 */
static SCM search_free_cell(void)
{
 again:
    if (current_search_page == NULL) return NULL;

    if (PAGES_FREE_CELL_LIST(current_search_page) == NULL) {
        current_search_page = NEXT_PAGE(current_search_page);
        goto again;
    }
    SCM free_cell = PAGES_FREE_CELL_LIST(current_search_page);
    PAGES_FREE_CELL_LIST(current_search_page) = NEXT_FREE_CELL(free_cell);
    return free_cell;
}

/**
 * 新たなheap pageを確保する
 */
static void add_heap(void)
{
    SCM page = xmalloc(sizeof(struct _Cell) * ALLOCATE_HEAP_PAGE_OBJECT_SIZE);
    SCM management_cell = page;
    SCM first_cell = page + 1;
    SCM curr_cell = NULL;
    SCM last_cell = page + ALLOCATE_HEAP_PAGE_OBJECT_SIZE - 1;
    int cell_index = 0;

    /* register memory page management list*/
    FREE_CELL_CONSTRUCT(management_cell, first_cell, NULL);
    if (page_list == NULL) {
        page_list = page;
    } else {
        SCM next = page_list;
        while(NEXT_PAGE(next) != NULL) {
            next = NEXT_PAGE(next);
        }
        NEXT_PAGE(next) = page;
    }

    /* free list linking */
    for (curr_cell = first_cell, cell_index = 1;
         curr_cell <= last_cell;
         curr_cell++, cell_index++) {
        FREE_CELL_CONSTRUCT(curr_cell, AS_SCM(cell_index), (curr_cell + 1));
    }
    CDR(last_cell) = NULL;

#if DEBUG
    dump_page_list();
#endif
    free_cell_total_size += (ALLOCATE_HEAP_PAGE_OBJECT_SIZE - 1);
#if DEBUG
    printf("free_cell_total_size %d\n", free_cell_total_size);
#endif
}

/**
 * heapで確保したオブジェクトか調べる
 */
static int is_heap_object(SCM obj)
{
    SCM page;

    if (AS_UINT(obj) & 0x03) return (0 != 0); /* false */

    for (page = page_list; page != NULL; page = CDR(page)) {
        if (is_this_pages_object(page, obj)) {
            return (0 == 0); /* true */
        }
    }

    return (0 != 0); /* false */
}


/**
 * このheap pageで確保したオブジェクトか調べる
 */
static int is_this_pages_object(SCM page, SCM obj)
{
    SCM page_begin_address = page;
    SCM page_end_address = page + ALLOCATE_HEAP_PAGE_OBJECT_SIZE - 1;
    SCM obj_address = obj;

    /* in heap address */
    if ( page_begin_address <= obj_address && obj_address <= page_end_address ) {
        size_t diff = AS_UINT(obj_address) - AS_UINT(page_begin_address);
        if ((diff % sizeof(struct _Cell)) == 0) {
            /* cellのアドレス */
            return (0 == 0); /* true */
        }
    }
    return (0 != 0); /* false */
}

/**
 * heap pageを出力する
 */
static void dump_page_list(void)
{
    SCM curr_page = page_list;
    int page_index = 0;
    printf("page_list\n");
    while (curr_page != NULL) {
        SCM curr = CAR(curr_page);
        int i = 0;
        printf("free_list[%d] = %p\n", page_index, curr_page);
        while(curr != NULL) {
            printf("[%d] = %p\n",i++,curr);
            curr = CDR(curr);
        }
        curr_page = CDR(curr_page);
        page_index++;
    }
}

/*==================================================
  GC
==================================================*/
/* garbage collection */
/* root maker */
static void gc_mark_stack(void);
static void gc_mark_symbol_table(void);

static void gc_mark_memory(void *start, void *end, int offset);
static void gc_mark_maybe_object(SCM obj);
static void gc_mark_object(SCM obj);

/* sweeper */
static int gc_sweep(void);
static void gc_sweep_symbol_table_free_cell_remove(void);

/* for debug */
static void dump_cell(void);

void scm_gc_protect(SCM obj)
{
    GC_FOREVER_MARK(obj);
}

static void scheme_gc(void)
{
    int collect_cells;

    gc_mark_stack();
    gc_mark_symbol_table();

#if DEBUG
    dump_page_list();
    dump_cell();
    printf("free_cell_total_size %d\n", free_cell_total_size);
#endif
    collect_cells = gc_sweep();
#if DEBUG
    printf("free_cell_total_size %d\n", free_cell_total_size);
#endif
    if (collect_cells < 600) {
        add_heap();
    }
    current_search_page = page_list;
}


/* マシンスタックのマーク */
static void gc_mark_stack(void)
{
    jmp_buf save_register_for_gc_mark;
    void *start;
    void *end;
    int i;
    void *end_of_stack;

    /* register value dump */
    setjmp(save_register_for_gc_mark);

    /* スタックの先端を記録 */
    SCHEME_SET_STACK_END(&end_of_stack);

#if DEBUG
    printf("stack start %p\n", stack_start);
    printf("stack end %p\n", stack_end);
#endif

    if (AS_UINT(stack_start) < AS_UINT(stack_end)) {
        start = stack_start;
        end = stack_end;
    } else {
        start = stack_end;
        end = stack_start;
    }
    /* 保守的GCを行う */
    for (i = 0; i < sizeof(void *); i++) {
        gc_mark_memory(start, end, i);
    }
    return ;
}

/* mark of symbol table
 */
static void gc_mark_symbol_table()
{
    SCM* start = SYMBOL_TABLE;
    SCM* end = start + SYMBOL_TABLE_SIZE;
    while (start != end) {
        SCM symbol_list = *start;
        SCM symbol;
        FOR_EACH(symbol_list, symbol) {
            GC_MARK(symbol_list);
            if (! UNBOUND_P(SYMBOL_VCELL(symbol))) {
                GC_MARK(symbol);
                gc_mark_object(SYMBOL_VCELL(symbol));
            } else {
                /* 何も指し示していないものはマークしない */
            }
        }
        gc_mark_object(*start);
        start++;
    }
}


/* 指定されたアドレスの範囲をマーク */
static void gc_mark_memory(void *start, void *end, int offset)
{
    SCM *obj;
    for (obj = (SCM *) (char *) start + offset; (void *) obj < end; obj++) {
        gc_mark_maybe_object(*obj);
    }
    fflush(stdout);
}


/* オブジェクトらしいものをマーク */
static void gc_mark_maybe_object(SCM obj)
{
    if (is_heap_object(obj)) {
        gc_mark_object(obj);
    }
}

/**
 * オブジェクトのマーク
 */
static void gc_mark_object(SCM obj)
{
 loop:
    if (obj == NULL) return;
    if (SCM_CONSTANT_P(obj)) return ; /* scm constant is not marking */
    if (FREE_CELL_P(obj)) return ; /* free cell is not marking */
    if (GC_MARK_P(obj)) return ; /* already marked. */
    
    GC_MARK(obj);
    if (CONS_P(obj)) {
        gc_mark_object(CAR(obj));
        obj = CDR(obj);
        goto loop;
    } else if (INTEGER_P(obj)) {
        return ;
    } else if (SYMBOL_P(obj)) {
        gc_mark_object(SYMBOL_VCELL(obj));
    } else if (STRING_P(obj)) {
        return ;
    } else if (PRIMITIVE_P(obj)) {
        return ;
    } else if (CLOSURE_P(obj)) {
        gc_mark_object(CLOSURE_ARGS(obj));
        gc_mark_object(CLOSURE_BODY(obj));
        obj = CLOSURE_ENV(obj);
        goto loop;
    } else if (MACRO_P(obj)) {
        obj = MACRO_CLOSURE(obj);
        goto loop;
    }
}

static void cell_finalize(SCM cell)
{
    if (SYMBOL_P(cell)) {
        free(SYMBOL_NAME(cell));
    }
}

/**
 * sweep phese
 */
static int gc_sweep(void)
{
    SCM page = page_list;
    int collect = 0;

    printf("sweep\n");

    gc_sweep_symbol_table_free_cell_remove();

    while(page != NULL) {
        SCM cell = page;
        SCM last_cell = page + ALLOCATE_HEAP_PAGE_OBJECT_SIZE - 1;
        while (AS_UINT(cell) <= AS_UINT(last_cell)) {
            if (FREE_CELL_P(cell)) {
                /* through */
            } else if (GC_MARK_P(cell)) {
                if (! GC_FOREVER_MARK_P(cell)) {
                    GC_UNMARK(cell);
                }
            } else { /* unmarked */

                cell_finalize(cell);
                int cell_index = 0;
                if (PAGES_FREE_CELL_LIST(page) != NULL) {
                    cell_index = AS_UINT(FREE_CELL_INDEX(PAGES_FREE_CELL_LIST(page)));
                }
                cell_index++;
                FREE_CELL_CONSTRUCT(cell,
                                    AS_SCM(cell_index),
                                    PAGES_FREE_CELL_LIST(page));
                PAGES_FREE_CELL_LIST(page) = cell;
                free_cell_total_size++;
                collect++;
            }
            cell++;
        }
        page = NEXT_PAGE(page);
    }
    return collect;
}

static void gc_sweep_symbol_table_free_cell_remove(void)
{
    SCM* start = SYMBOL_TABLE;
    SCM* end = start + SYMBOL_TABLE_SIZE;
    while (start != end) {
        SCM symbol_list = *start;
        SCM first = symbol_list;

        /* */
        while (! NULL_P(symbol_list)) {
            if (!GC_MARK_P(CAR(symbol_list))) {
                symbol_list = CDR(symbol_list);
            } else {
                first = symbol_list;
                break;
            }
        }
        if (first == *start && NULL_P(symbol_list)) {
            first = symbol_list;
        }

        while (! NULL_P(symbol_list)) {
            if (! NULL_P(CDR(symbol_list)) &&
                ! GC_MARK_P(CAR(CDR(symbol_list)))) {
                CDR(symbol_list) = CDDR(symbol_list);
            } else {
                symbol_list = CDR(symbol_list);
            }
        }
        *start = first;
        start++;
    }    
}


/**
 *
 */
static void dump_cell(void)
{
    SCM page = page_list;
    while(page != NULL) {
        SCM cell = page;
        SCM last_cell = page + ALLOCATE_HEAP_PAGE_OBJECT_SIZE - 1;
        while (cell <= last_cell) {
            printf("%p gc flag %p\n",cell, AS_SCM(HEADER_GC_FLAG(cell)));
            cell++;
        }
        page = NEXT_PAGE(page);
    }
    fflush(stdout);
}




/*==================================================
  object allocator
==================================================*/
SCM new_cons(SCM kar, SCM kdr)
{
    SCM obj = allocate_cell();
    CONS_CONSTRUCT(obj, kar, kdr);
    return obj;
}

SCM new_integer(int value)
{
    SCM obj = allocate_cell();
    INTEGER_CONSTRUCT(obj, value);
    return obj;
}

SCM new_symbol(char *pname, SCM value)
{
    SCM obj = allocate_cell();
    SYMBOL_CONSTRUCT(obj, strdup(pname), value);
    return obj;
}

SCM new_string(char *string)
{
    SCM obj = allocate_cell();
    STRING_CONSTRUCT(obj, strdup(string));
    return obj;
}

SCM new_closure(SCM sexp, SCM env)
{
    SCM obj = allocate_cell();
    CLOSURE_CONSTRUCT(obj, CAR(sexp), CDR(sexp), env);
    return obj;
}

SCM new_macro(SCM closure, SCM env)
{
    SCM obj = allocate_cell();
    MACRO_CONSTRUCT(obj, closure);
    return obj;
}

SCM new_port(FILE *file)
{
    SCM obj = allocate_cell();
    PORT_FILE(obj) = file;
    return obj;
}
