# Curium v5 Migration Guide

Welcome to Curium version 5.0! This document is meant to act as a changelog and migration guide if you are upgrading from v4.0.

## 💥 Breaking Changes

- **Arena Garbage Collector**: Manual allocation tracking is deprecated. The garbage collector architecture has moved strictly toward Automatic Reference Counting overlaid with `reactor arena` block allocators. Code performing direct `malloc`/`free` calls should be enclosed in a `c { ... }` block to avoid warning halts from the type-checker.
- **`cm doctor` enforcement**: The core CLI now ships with `cm doctor`. Before committing, always run it. `cm check` acts identically if executing on a specific file.

## ✨ Important Additions

- **TCC Backend**: Curium 5.0 is formally powered by the Tiny C Compiler (TCC), yielding microsecond linking loops. Gcc and Clang are still partially fallback-supported.
- **The Neon-DOD TUI**: A highly aesthetic TUI built natively into the compiler outputting arena memory utilization and step-by-step progress bars.
- **Registers Hinting**: A new explicit metadata prefix, `#[hot] mut my_counter = 0;`, implicitly translates downstream to C11 `register`, preventing cache misses on intense computational variables.
- **Try / Catch / Throw**: Synchronous robust handling has been injected. See [Language Guide](docs/LANGUAGE_GUIDE.md) for more info.
- **Enumerations**: `enum { ... }` combined with Rust-style pattern `match` functionality finally adds data-wrapping variants (e.g. `Result::Err(string)`).

## Upgrade Strategy

1. Move any raw C calls like `malloc` explicitly inside `c { ... }` polyglot blocks.
2. If memory limits were an issue, restructure logic within a `reactor arena(size)` block.
3. Replace older nested `if check == _` structures with `match` cases.
4. Run `cm doctor` internally within the IDE.
