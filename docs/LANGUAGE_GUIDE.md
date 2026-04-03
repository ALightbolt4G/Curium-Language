# 🚀 Getting Started with Curium

Curium is a powerful, expressive language that brings modern ergonomic features like memory safety and dynamic operators to the world of performance-critical software.

---

## 🏗️ Step 1: Installation

Curium depends on a C99-compliant compiler (MinGW, GCC, or Clang) and CMake for building project files.

### Windows (recommended)
1. Install [MinGW-w64](https://www.mingw-v64.org/) and add the `bin` folder to your PATH.
2. Install [CMake](https://cmake.org/download/).
3. Clone or download the Curium project and navigate to it in your terminal.
4. Run the installer script or build the project from source:
   ```powershell
   cmake -B build
   cmake --build build
   ```
5. Add the `build/` folder to your PATH to use the `curium` command globally.

---

## 📝 Step 2: Your First Project

Let's create a "Hello World" application.

1.  Initialize a new project:
    ```bash
    curium init my_app
    cd my_app
    ```
2.  Open `src/main.cm` and write:
    ```cm
    fn main() {
        println("Hello, Curium!");
    }
    ```
3.  Run the project:
    ```bash
    curium run
    ```

---

## 🎨 Step 3: Key Features to Explore

### 1. Variables and Types
Curium uses `let` for immutable variables and `mut` for mutable ones.
```cm
let x = 10;
mut y = 20;
y = 30; // Correct
x = 10; // Error: x is immutable
```

### 2. Dynamic Operators (`dyn`)
Define custom operator logic that can be changed at runtime.
```cm
mut op = "+";
dyn op in (
    "+" => { return x + y; },
    "avg" => { return (x + y) / 2.0; }
) dyn($) { return 0; };
```

### 3. Safety First (`^` and `?`)
Curium provides safe pointers and optional types to prevent null-pointer exceptions and out-of-bounds access.
```cm
mut head: ^Node = ^node; // Safe pointer
let val: int = res?;   // Error-propagating unwrap
```

### 4. Memory Model (Reactors)
Choose between performance and convenience.
```cm
reactor arena(1024) {
    // Bulk allocate and free instantly
}
```

---

## 📚 Next Steps
- Read the [Syntax Reference](SYNTAX_REFERENCE.md) for a full index of features.
- Explore the [Standard Library](PROJECT_GUIDE.md) to build more complex apps.
- Join the community and start building amazing frameworks!
