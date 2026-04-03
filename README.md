# ⚡ Curium Language v3.0

**Professional C#-like/Go-hybrid Language for C-performance Systems**

Curium is a modern programming language designed for high-performance systems development. It combines the safety and ergonomics of modern languages like Go and Rust with the raw power and control of C.

## ✨ Key Features

- **🚀 Performance**: Transpiles directly to C11, ensuring maximum performance and portability.
- **🛡️ Memory Safety**: Features a built-in reference-counting garbage collector for effortless memory management.
- **💎 Modern Syntax**: Clean, C#-like syntax with powerful features like pattern matching and type-agnostic `println`.
- **动态 Dynamic Operators**: Use the `dyn` keyword to define custom infix operators with full logic and safety fallbacks.
- **🛠️ Integrated CLI**: A unified tool for building, running, and managing packages.
- **🔌 C Interoperability**: Seamlessly call C functions or embed C code directly using `c { ... }` blocks.

## 🏁 Quick Start

### Installation

Download and run the installer for your platform:

- **Windows**: `powershell -ExecutionPolicy Bypass -File .\install.ps1`
- **Linux/macOS**: `bash ./install.sh`

### Your First Program

Create a file named `hello.curium`:

```cm
fn main() {
    println("Hello, Curium v3!");
}
```

Run it immediately:

```bash
curium run hello.curium
```

### 🌊 Dynamic Operators

Curium allows you to define custom operators at runtime using the `dyn` keyword:

```cm
fn main() {
    mut x = 10;
    mut y = 20;
    mut op = "+";

    dyn op in (
        "+" => { return x + y; },
        "*" => { return x * y; },
        "avg" => { return (x + y) / 2; }
    ) dyn($) {
        println("Unknown operator!");
        return 0;
    };

    println(x op y); // Output: 30
}
```

## 📖 Documentation

Explore the full Curium documentation suite:

- 📘 **[Language Guide](docs/LANGUAGE_GUIDE.md)**: A high-level introduction for newcomers.
- 📖 **[Syntax Reference](docs/SYNTAX_REFERENCE.md)**: A complete, A-Z manual of every keyword and operator.
- 🛠️ **[CLI Reference](docs/CLI_REFERENCE.md)**: Detailed guide for the `curium` command and package manager.
- 🏢 **[Project Guide](docs/PROJECT_GUIDE.md)**: Information on project structure and modularity.
- 🔌 **[Compiler Internals](docs/INTERNALS.md)**: Deep dive into the Lexer, Parser, and C-codegen.
- 🚀 **[Installation Guide](docs/INSTALLATION.md)**: Setup instructions for all platforms.
- 📝 **[Implementation Examples](docs/EXAMPLES.md)**: Practical patterns for linked lists, concurrency, and more.

## 🤝 Contributing

We welcome contributions! Please check out our [GitHub repository](https://github.com/ALightbolt4G/Curium-lang) for more information.

---
*✨ Happy coding with Curium!*
