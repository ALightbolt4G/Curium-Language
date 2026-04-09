/*
 * curium/compiler/arena.h — Compiler-internal AST bump-pointer arena (Phase 2 DOD)
 *
 * IMPORTANT: This is the *compiler-internal* arena used for AST node allocation.
 * It is intentionally distinct from the runtime CuriumArena (memory.h)
 * which is the user-facing Reactor allocator.  Both live in the same binary
 * so all symbols use the `curium_ast_arena_` prefix to avoid linker conflicts.
 *
 * Cache Metaphor
 * ─────────────
 *   RAM   = The Fridge   (slow — ~200 cycles per cache-miss)
 *   L2    = The Shelf    (fast — ~12 cycles)
 *   Nodes = Ingredients  (the fewer fridge trips, the faster the chef cooks)
 *
 * Without an arena: every curium_ast_v2_node_t is a separate GC-tracked malloc.
 * The heap scatters them across pages → every pointer dereference during
 * codegen traversal is a potential cache miss.
 *
 * With this arena: an entire parse session fills 64 KB blocks sequentially.
 * Sibling nodes are adjacent in memory → the Shelf is pre-loaded, the Chef
 * grabs many nodes in one trip to the Shelf instead of the Fridge.
 *
 * Usage:
 *   curium_ast_arena_t a;
 *   curium_ast_arena_init(&a);
 *   void* p = curium_ast_arena_alloc(&a, sizeof(T), CURIUM_AST_ARENA_DEFAULT_ALIGN);
 *   curium_ast_arena_destroy(&a);   // O(n_blocks), not O(n_nodes)
 */
#ifndef CURIUM_AST_ARENA_H
#define CURIUM_AST_ARENA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 64 KB per block — fits comfortably inside a typical 256 KB L2 cache.
 * Holds ~200 average AST nodes per block for most Curium programs.     */
#define CURIUM_AST_ARENA_BLOCK_SIZE ((size_t)65536)

/* Default pointer alignment for platforms without C11 _Alignof.        */
#ifndef CURIUM_AST_ARENA_DEFAULT_ALIGN
#  define CURIUM_AST_ARENA_DEFAULT_ALIGN (sizeof(void*))
#endif

/* ── Internal block header ─────────────────────────────────────────────────
 * One malloc per block — header and data live in the same slab.
 * Data bytes start immediately after this header struct.                */
typedef struct curium_ast_arena_block {
    struct curium_ast_arena_block* next;      /* linked list of blocks    */
    size_t                         used;      /* bytes used in this block */
    size_t                         capacity;  /* usable bytes after header*/
} curium_ast_arena_block_t;

/* Pointer to the first usable byte of a block.                          */
#define CURIUM_AST_ARENA_BLOCK_DATA(blk) \
    ((char*)(blk) + sizeof(curium_ast_arena_block_t))

/* ── Arena context ──────────────────────────────────────────────────────── */
typedef struct {
    curium_ast_arena_block_t* first;      /* head of block linked list   */
    curium_ast_arena_block_t* current;    /* active block (alloc here)   */
    size_t                    node_count; /* diagnostic: # allocations   */
    size_t                    total_bytes;/* diagnostic: bytes committed  */
} curium_ast_arena_t;

/* ── Public API ─────────────────────────────────────────────────────────── */

/* Initialise an arena and allocate its first 64 KB block.               */
void curium_ast_arena_init(curium_ast_arena_t* arena);

/* Allocate `size` bytes aligned to `alignment`.  Returns NULL only on OOM.
 * Returned memory is always zero-initialised.                            */
void* curium_ast_arena_alloc(curium_ast_arena_t* arena, size_t size, size_t alignment);

/* Reset all blocks to empty without freeing them (fast reuse).           */
void curium_ast_arena_reset(curium_ast_arena_t* arena);

/* Free every block — O(n_blocks), typically 3-6 free() calls per parse. */
void curium_ast_arena_destroy(curium_ast_arena_t* arena);

/* Print diagnostic stats to stderr (node count, bytes, block count).    */
void curium_ast_arena_dump_stats(const curium_ast_arena_t* arena);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_AST_ARENA_H */
