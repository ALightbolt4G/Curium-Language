# 🛠️ Curium Compiler & Runtime Internals

This document explains the internal architecture and implementation details of the Curium compiler and its supporting runtime system.

---

## 🏗️ The Compiler Pipeline

Curium is a **transpiler** that converts Curium source code (`.cm`) into highly optimized C99 source code. The compilation process follows three primary phases:

### 1. Lexing (Lexical Analysis)
**Source**: `src/compiler/lexer/lexer_v2.c`
- **Output**: A stream of `curium_token_t` objects.
- **Process**: The lexer reads source strings and converts them into tokens like keywords (`let`, `fn`), operators (`+`, `-`), or identifiers. It also handles string interpolation templates and raw string literals ($r"...").

### 2. Parsing (AST Construction)
**Source**: `src/compiler/parser/parser_v2.c`
- **Output**: An Abstract Syntax Tree (AST) rooted at `curium_ast_v2_node_t`.
- **Process**: Using a recursive descent parser, Curium converts the token stream into a tree structure representing the logical syntax of the program. This phase also handles operator precedence and statement grouping.

### 3. Code Generation (C Transpilation)
**Source**: `src/compiler/codegen/codegen_v2.c`
- **Output**: Optimized C99 source code.
- **Process**: The AST is traversed, and for each node, a corresponding C code fragment is generated. Curium maps its unique features (like `dyn` or `reactor`) into standard C macros or runtime calls.

---

## 🔌 The Runtime System

The Curium runtime provides the infrastructure for memory management, string operations, and safety.

### 1. String Management (`curium_string_t`)
**Source**: `src/runtime/string.c`
- Curium strings are more than raw `char*` arrays. They are managed objects tracking length and capacity, providing safety from buffer overflows and providing high-performance appending and formatting.

### 2. Safe Pointers (`curium_safe_ptr_t`)
**Source**: `src/runtime/safe_ptr.c` / `include/curium/safe_ptr.h`
- The `^T` operator creates a safe pointer. These pointers include **metadata for bounds checking**. Accessing a safe pointer outside its allocated region triggers a runtime safety error instead of a crash.

### 3. Garbage Collection (GC)
**Source**: `src/runtime/gc.c`
- Curium uses an **Automatic Reference Counting (ARC)** system with an integrated cycle detector.
- **Increment/Decrement**: Every time an object is passed or assigned, its reference count is updated. When the count hits zero, the object is immediately reclaimed.

### 4. Reactor Memory Model
**Concept**: `src/runtime/reactor.c`
- Reactors allow for manual or region-based memory control inside specific blocks.
- **Arena**: Allocates from a pre-allocated pool, providing O(1) allocation and zero-cost deallocation (the whole pool is cleared at once).

---

## ⚡ C-Polyglot Handling

When the compiler encounters a `c { ... }` block:
1.  The parser extracts the raw text exactly as it appears.
2.  The codegen emits this text directly into the generated `curium_out.c` at the corresponding location.
3.  Include headers found in the polyglot block are moved to the top of the generated file to ensure proper compilation.
