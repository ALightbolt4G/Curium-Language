/*
 * src/compiler/arena.c — Compiler-internal AST arena implementation (Phase 2 DOD)
 *
 * See include/curium/compiler/arena.h for design notes.
 *
 * All symbols use the curium_ast_arena_ prefix to coexist with the runtime
 * CuriumArena (memory.h) in the same binary without linker conflicts.
 */
#include "curium/compiler/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal helpers ─────────────────────────────────────────────────────── */

static curium_ast_arena_block_t* curium_ast_arena_block_new(size_t capacity) {
    /* One allocation: header + data in one contiguous slab (cache-friendly). */
    curium_ast_arena_block_t* blk =
        (curium_ast_arena_block_t*)malloc(sizeof(curium_ast_arena_block_t) + capacity);
    if (!blk) return NULL;
    blk->next     = NULL;
    blk->used     = 0;
    blk->capacity = capacity;
    /* Zero the data region — curium_ast_v2_node_t fields must start clean. */
    memset(CURIUM_AST_ARENA_BLOCK_DATA(blk), 0, capacity);
    return blk;
}

/* Round x up to the nearest multiple of align (align must be power-of-2). */
static size_t curium_ast_arena_align_up(size_t x, size_t align) {
    return (x + align - 1u) & ~(align - 1u);
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void curium_ast_arena_init(curium_ast_arena_t* arena) {
    memset(arena, 0, sizeof(*arena));
    arena->first   = curium_ast_arena_block_new(CURIUM_AST_ARENA_BLOCK_SIZE);
    arena->current = arena->first;
    /* If the first malloc fails, arena->current is NULL.  alloc() will
     * try again for each request — this keeps the fallback path alive. */
}

void* curium_ast_arena_alloc(curium_ast_arena_t* arena, size_t size, size_t alignment) {
    if (!arena || size == 0) return NULL;

    /* Clamp alignment to at least pointer size. */
    if (alignment < sizeof(void*)) alignment = sizeof(void*);

    /* ── Fast path: fit in the current block (Shelf) ─────────────────────── */
    if (arena->current) {
        size_t offset = curium_ast_arena_align_up(arena->current->used, alignment);
        size_t end    = offset + size;

        if (end <= arena->current->capacity) {
            void* ptr = CURIUM_AST_ARENA_BLOCK_DATA(arena->current) + offset;
            /* Memory is already zeroed from block_new/reset — just mark used. */
            arena->current->used = end;
            arena->node_count++;
            arena->total_bytes += size;
            return ptr;
        }
    }

    /* ── Slow path: current block is full — grow the Shelf ───────────────── */
    size_t block_capacity = (size > CURIUM_AST_ARENA_BLOCK_SIZE)
                            ? size                        /* oversized single alloc */
                            : CURIUM_AST_ARENA_BLOCK_SIZE;

    curium_ast_arena_block_t* blk = curium_ast_arena_block_new(block_capacity);
    if (!blk) return NULL;

    if (arena->current) arena->current->next = blk;
    else                arena->first         = blk;
    arena->current = blk;

    /* Fresh block is already zeroed; allocation starts at offset 0. */
    blk->used = size;
    arena->node_count++;
    arena->total_bytes += size;
    return CURIUM_AST_ARENA_BLOCK_DATA(blk);
}

void curium_ast_arena_reset(curium_ast_arena_t* arena) {
    /* Clear used counters and re-zero data; blocks stay allocated. */
    curium_ast_arena_block_t* blk = arena->first;
    while (blk) {
        memset(CURIUM_AST_ARENA_BLOCK_DATA(blk), 0, blk->used);
        blk->used = 0;
        blk = blk->next;
    }
    arena->current     = arena->first;
    arena->node_count  = 0;
    arena->total_bytes = 0;
}

void curium_ast_arena_destroy(curium_ast_arena_t* arena) {
    /* O(n_blocks) — one free() per 64 KB slab.
     * For a typical parse session this is ~3-6 free() calls total. */
    curium_ast_arena_block_t* blk = arena->first;
    while (blk) {
        curium_ast_arena_block_t* next = blk->next;
        free(blk);
        blk = next;
    }
    arena->first       = NULL;
    arena->current     = NULL;
    arena->node_count  = 0;
    arena->total_bytes = 0;
}

void curium_ast_arena_dump_stats(const curium_ast_arena_t* arena) {
    size_t blocks = 0, capacity_total = 0;
    const curium_ast_arena_block_t* blk = arena->first;
    while (blk) {
        blocks++;
        capacity_total += blk->capacity;
        blk = blk->next;
    }
    fprintf(stderr,
        "[ast_arena] nodes=%zu  bytes=%zu  blocks=%zu  capacity=%zu  eff=%.1f%%\n",
        arena->node_count, arena->total_bytes, blocks, capacity_total,
        (arena->total_bytes > 0 && capacity_total > 0)
            ? 100.0 * (double)arena->total_bytes / (double)capacity_total
            : 0.0);
}
