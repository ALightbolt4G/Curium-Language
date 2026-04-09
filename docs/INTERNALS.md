# 🔌 Compiler Internals (Curium 4.0)

Curium transpiles to C11. This requires multiple distinct internal architectural modules heavily relying on the core `ast_v2.h` tree system.

## 1. The Preprocessor & Lexer
Located in `src/compiler/lexer/lexer.c`, the raw text passes through an immediate tokenization function parsing out string literals, whitespace, polyglot `c { ... }` blocks, and resolving token chains against `include/curium/compiler/tokens.h`.

## 2. The Multi-Pass Parser (AST v2)
The tokenized array is mapped rapidly onto the `curium_ast_v2_node_t` struct graph.
- Every node operates via unionized memory states depending on `curium_ast_v2_kind_t`.
- `dyn` blocks generate complex associative linked-list mappings (`match_arm` unions mapped to sequential `fallback` blocks).
- Polyglot nodes inject unparsed strings to directly insert verbatim outputs.

## 3. The Type Checker
To provide static safety despite dynamic operations, semantic resolution occurs throughout `typecheck.c`.
Features like `strnum` are hardwired to gracefully allow numeric mutations alongside standard string concatenation checks, resolving into standardized internal string formats dynamically while checking bounds.

## 4. C11 Codegen Generation
The heart of Curium translates the AST directly to standard C99/C11 syntax using standard string builder architectures:
- C variables mapped directly directly to `.c` mappings.
- Output file typically emitted to `.cache/curium_out.c`.

## 5. TCC/GCC Linking
Finally, CMake builds or native compiler commands invoke system-available GCC/Clang/TCC hooks mapping the output `.c` against `src/runtime/*.c` (the Curium SDK libraries: `map.c`, `project.c`, `oop.c`).
