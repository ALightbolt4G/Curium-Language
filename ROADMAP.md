# 🗺️ Curium Language Roadmap

This document outlines the evolutionary path for the Curium Language, moving it from a solid core to a production-ready systems programming language.

---

## Phase 1: Core Completeness

### 1. Error Handling
- **Concept:** Robust error propagation using Rust-like `Result<T, E>` and `Option<T>`.
- **Syntax Changes:**
  - `?` operator for early return on errors/none.
  - `try` / `catch` blocks for localized error recovery.
  - Pattern matching integration to extract values (e.g., `match (res) { Some(v) => ..., Error(e) => ... }`).

### 2. Modules & Imports
- **Concept:** A clean module system to divide code into multiple files.
- **Syntax Changes:**
  - `import "math/vector.curium"` or `use str::split`.
  - `pub` keyword to explicitly mark functions/structs as exported.
- **Compiler Changes:**
  - Multi-file parsing and dependency resolution within the `curium build` pipeline.

### 3. Generics (Parametric Polymorphism)
- **Concept:** Write flexible, reusable code without sacrificing static typing.
- **Syntax Changes:**
  - `fn fmap<T, U>(arr: array<T>, fn(T)->U) -> array<U>`
  - `struct Box<T> { val: T }`
- **Compiler Changes:**
  - Monomorphization pass during codegen to emit specialized C functions (e.g., `_curium_fmap_int_float`).

### 4. Unit Testing Framework
- **Concept:** First-class testing integrated directly into the language and CLI.
- **Syntax Changes:**
  - `#[test]` annotation above functions.
- **Compiler/CLI Changes:**
  - `curium test` command that scans for test cases, links them into a temporary executable, runs them, and outputs a formatted report.

---

## Phase 2: Developer Experience (DX)

### 1. Tooling Integration
- **LSP Server:** For real-time syntax checking, Go-to-Definition, and auto-completion in VS Code.
- **Formatter:** `curium fmt` to automatically enforce a standardized code style.
- **Linter:** Real-time warnings for dead code, unused variables, and memory leak vulnerabilities.

### 2. Standard Library Expansion
- **File I/O:** Reading/writing files efficiently, path manipulation.
- **Networking:** Basic cross-platform HTTP client/server capabilities.
- **Data Serialization:** Built-in JSON parsing and emitting.
- **System Calls:** Better abstractions over generic OS functionality (env vars, processes).

---

## Phase 3: Advanced Systems Features

### 1. Concurrency Model
- **Concept:** Safe, high-performance concurrent processing.
- **Options:** Lightweight threads (like Go routines) or an async/await state-machine system (like Rust). Channels for message passing and Mutexes for shared state.

### 2. Advanced Type System
- **Enums with Associated Data:** Algebraic data types for modeling complex states.
- **Interfaces / Traits:** Define shared generic behaviors (e.g., `impl Formattable for User`).

### 3. Memory & C-Interop Deep Dive
- **Raw Pointers:** Safe wrappers and unsafe blocks for zero-cost abstraction when interfacing with C.
- **Stack Allocation:** Force objects to avoid the GC entirely for critical performance paths.

---

## Phase 4: Ecosystem & Distribution

### 1. Package Registry
- A centralized or Git-based registry for publishing and consuming third-party Curium packages.

### 2. Documentation Generator
- `curium doc` command that generates HTML/Markdown references from source code comments (`///`).

### 3. Build Profiles
- Release optimizations vs. Debug builds (stripping symbols, aggressive C compiler optimizations).
