# 📖 Curium Syntax Reference (v4.0)

This document provides a comprehensive reference for the Curium programming language.

---

## 🔑 Keywords

| Keyword | Description |
|---|---|
| `let` | Declare an immutable variable. |
| `mut` | Declare a mutable variable. |
| `fn` | Declare a function. |
| `if` / `else` | Conditional branching. |
| `while` | Loop while a condition is true. |
| `for` | Iterate over a collection (Experimental). |
| `match` | Pattern matching on values. |
| `return` | Exit a function and return a value. |
| `dyn` | Define or use a dynamic operator. |
| `try` / `catch` | Exception handling and error propagation. |
| `throw` | Raise an error. |
| `pub` | Mark a declaration as public (for modules). |
| `impl` | Implement methods for a struct or trait. |
| `struct` | Define a data structure. |
| `enum` | Define an enumerated type. |
| `union` | Define a tagged union. |
| `trait` | Define an interface (Experimental). |
| `spawn` | Spawn a new thread/task (Experimental). |
| `gc` | Trigger manual garbage collection. |
| `print` / `println` | Output data to the console. |
| `input` | Fetch user input from the console. |
| `strnum` | Special numeric-string hybrid type. |
| `import` / `require` | Link external Curium modules. |
| `reactor` | Defines a block with a specific memory model. |
| `arena` | Arena allocation mode (used with `reactor`). |
| `manual` | Manual memory management mode (used with `reactor`). |
| `rc` | Reference counting mode (default, used with `reactor`). |

---

## 🏗️ Type System

### Scalar Types
- `int` / `float`: Standard integer and double-precision float.
- `bool`: `true` or `false`.
- `string`: UTF-8 encoded string.
- `void`: No value.

### Sized Numeric Types (v4.0)
- **Signed Integers**: `i8`, `i16`, `i32`, `i64`.
- **Unsigned Integers**: `u8`, `u16`, `u32`, `u64`, `usize`.
- **Floats**: `f32`, `f64`.

### Complex Types
- `^T`: Safe pointer to type `T`.
- `?T`: Option type (either `Some(val)` or `None`).
- `Result<T, E>`: Result type (either `Ok(val)` or `Err(err)`).
- `array<T>`: Fixed-size heap array.
- `slice<T>`: View into an array or buffer.
- `map<K, V>`: Key-value hash map.
- `dyn`: The dynamic 'any' type for flexible operations.

---

## ⚡ Operators

| Operator | Name | Description |
|---|---|---|
| `+`, `-`, `*`, `/` | Arithmetic | Standard math. |
| `==`, `!=` | Equality | Compare values. |
| `<`, `>`, `<=`, `>=` | Relational | Numeric comparisons. |
| `&&`, `||`, `!` | Logical | Boolean operations. |
| `&`, `|`, `^` | Bitwise | Bit-level operations. |
| `^expr` | Address-of | Prefix operator to take a reference/pointer. |
| `expr?` | Error Try | Propagates errors/unwraps Options. |
| `expr ?? default` | Nil Coalescing | Returns default if expr is None/null. |
| `->` | Return arrow | Used in function signatures. |
| `=>` | Fat arrow | Used in `match` and `dyn` arms. |
| `.` | Member access | Access struct fields or methods. |
| `:` | Type colon | Specify a variable's type. |
| `:=` | Type inference | Declare and initialize with inferred type. |
| `$` | Macro symbol | Used in templates and fallback blocks. |

---

## 🧠 Memory Models

Curium v4.0 introduces the **Reactor** system for fine-grained memory control.

### 1. Reference Counting (RC)
The default mode. Curium uses a robust, automatic RC system to manage memory.
```cm
reactor rc {
    let name = "Curium";
    // name is automatically tracked and freed.
}
```

### 2. Arena Allocation
Allocates all memory from a single block. Extremely fast for bulk allocations. Freed instantly when the block ends.
```cm
reactor arena(4096) {
    // All allocations here come from a 4KB arena.
}
```

### 3. Manual Management
Disables automatic tracking. Developers must manually manage lifetimes (Experimental).
```cm
reactor manual {
    let p = malloc(100);
    free(p);
}
```

---

## 🧪 Polyglot Interface

Curium can embed raw C code for performance-critical sections using `c { ... }`.

```cm
c {
    // Raw C99 code here
    #include <stdio.h>
    printf("Native C execution!\n");
}
```
