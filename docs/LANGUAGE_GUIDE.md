# 🚀 Getting Started with Curium v5.0

Curium is a next-generation systems programming language transpiling down to raw C11 while guaranteeing absolute memory safety and the ergonomics of C# and Go. It is designed precisely for performance-critical environments where speed and reliability are equally paramount.

---

## 🏗️ Step 1: Installation & CLI

Curium depends on a C99/C11 compliant compiler (MinGW, GCC, Clang, or TCC) and CMake.

### Windows (Recommended)
1. Install [MinGW-w64](https://www.mingw-v64.org/) and/or `tcc` (`choco install tcc`).
2. Install [CMake](https://cmake.org/download/).
3. Pull the Curium project repository.
4. Run the installer or build script:
   ```powershell
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
5. Add the `build/Release/` directory to your global PATH environment variable.

---

## 📝 Step 2: The Structure

Create a test program. Unlike older iterations, v5.0 is fully capable of type-agnostic formatting and automated memory cleanup out-of-the-box.

```cm
// src/main.cm
fn main() {
    mut name = "Developer";
    mut version = 4.0;
    
    // Automatic string interpolation and inference
    println("Hello {name}, welcome to Curium v{version}!");
}
```

---

## 🎨 Step 3: Monumental v5 Features

The v5 compiler comes equipped with a brand-new Neon-DOD TUI offering advanced metrics and real-time visualization of arena memory statistics.

### 1. The `strnum` Super-Type
Curium 5.0 introduces the `strnum` primitive type. It seamlessly pivots between string operations and math operations underneath without sacrificing static typing speed.
```cm
mut payload: strnum = 404;
payload = "Not Found"; // Completely legal, memory handled.
```

### 2. Dynamic Operators (`dyn`)
Define custom infix operations on runtime logic. With v5.0, you can explicitly map patterns to functions or direct return blocks, always guarded by a mandatory fallback for guaranteed compilation stability.
```cm
mut op = "custom_add";

dyn op in (
    "custom_add" => { return x + y + 1; },
    "avg" => { return (x + y) / 2; }
) dyn($) {
    println("Fallback triggered! Unknown action.");
    return 0;
};

// Execution:
let res = x op y;
```

### 3. Safe Pointers (`^`)
Dangling pointers are a thing of the past. Curium introduces reference-counted automatic safe pointers, denoted by `^`.
```cm
mut pointer: ^int = new 42;
println("Dereferenced value: " + (pointer^));
// Freeing happens natively when out of scope.
```

### 4. Zero-Cost Polyglot Blocks
Mix raw C efficiently:
```cm
c {
    printf("I am bypassing the AST directly into C11!\n");
}
```

### 5. Memory Model: Reactors
Opt entirely out of the garbage-collector overhead manually when required using Memory Reactors.
```cm
reactor arena(1024) {
    // 1024 byte bump allocator. Lightning fast allocations. O(blocks) destruction.
}
```

### 6. Enumerations and Match Control Flow
Full support for advanced enums (including variant types) paired with the `match` syntactic flow control.
```cm
enum Status {
    Ok(int),
    Err(string),
    Pending
}

fn handle_status(s: Status) {
    match s {
        Status::Ok(val) => { println("Success: " + val); },
        Status::Err(e) => { println("Failed! Msg: " + e); },
        Status::Pending => { println("Waiting..."); }
    }
}
```

### 7. Concurrency and Error Handling
Spawn tasks on isolated threads seamlessly and handle cascading errors via `try/catch/throw`:
```cm
spawn {
    // Executes entirely in a background thread
    try {
        if check_network() == false {
            throw "Network timeout!";
        }
    } catch (e) {
        println("Caught async error: " + e);
    }
}
```

---

## 📚 Next Steps
Continue to explore the [Syntax Reference](SYNTAX_REFERENCE.md) to dive deeper into Enums, Traits, Iterators, `#[hot]` bindings, and the exhaustive keyword checklist!
