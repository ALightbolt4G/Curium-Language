# 📖 Curium Syntax Reference

A comprehensive, A-Z reference of Curium's syntax and keywords.

---

## 1. Types & Variables

### Primitive Types
- `int`: Native integer.
- `float`: Double-precision float.
- `bool`: `true` or `false`.
- `string`: RC managed string slice. Supports interpolation: `"hello {name}"`.
- `strnum`: A dual-state container capable of acting as both a discrete number and a string depending on context.

### Variables
- `let x = 10;`: Immutable explicitly.
- `mut y = 20;`: Mutable.
- `#[hot] mut counter = 0;`: The compiler will emit a memory-register lock hint to the underlying CPU (compiling to `register` in C11).

### Complex Types
- `^T`: Safe Reference Pointer to `T`.
- `array<T>`: Fixed size contiguous block.
- `map<K, V>`: HashMap wrapper.

---

## 2. Control Flow

### `if`/`else`
```cm
if condition {
    // ...
} else if other_cond {
    // ...
} else {
    // ...
}
```

### `for`
The `for` loop dynamically understands ranges and native iterators.
```cm
for i in 0..10 {
    println(i);
}
```

### `while`
```cm
while condition {
    break; // or continue;
}
```

### `match`
Exhaustive pattern matching block:
```cm
match value {
    0 => { println("Zero"); },
    1 => { println("One"); },
    _ => { println("Other"); }
}
```

---

## 3. Data Structures

### `struct`
Construct records.
```cm
struct Point {
    x: int;
    y: int;
}

mut p = Point { x: 10, y: 20 };
```

### `union`
Memory-overlapped data structures.
```cm
union Overlay {
    bytes: array<int>;
    raw_memory: ^int;
}
```

### `enum`
Advanced multi-variant tagged unions natively integrated with the compiler match logic.
```cm
enum Token {
    EOF,
    Identifier(string),
    Number(float)
}
```

### `impl` and `trait`
Traits declare expected method signatures, and `impl` maps a trait onto a structure.
```cm
trait Drawable {
    fn draw();
}

impl Drawable for Point {
    fn draw() {
        println("Drawing {x}, {y}");
    }
}
```

---

## 4. Concurrent & Dynamic Mechanics

### `spawn`
Easily spin closures into active independent threads (or logical execution points depending on the compiler backend).
```cm
spawn {
    println("Background task!");
};
```

### `reactor`
Reactor contexts are the key to high-performance Curium applications. The default context uses automatic Reference Counting. By wrapping execution in an `arena` reactor, memory is slab-allocated and wiped upon exit, costing zero atomic overheads.
```cm
reactor arena(2048) {
    // 2MB allocation slab. 
    mut ptr: ^int = new 999;
} // Slab wiped. Memory completely freed here.
```

### `dyn`
Dynamically resolve and call match arms dynamically during operator logic:
```cm
dyn action in (
    "do" => { do_thing(); }
) dyn($) { };
```

---

## 5. Functions & Error Handling
```cm
fn calculate(value: int) -> Result<int, string> {
    if value == 0 {
        return Err("Cannot be zero");
    }
    return Ok(value * 2);
}

fn main() {
    let output = calculate(10)?; // The '?' operator unpacks Ok or early returns Err.
}
```
