# ⚡ Curium Language v5.0

**Professional C#-like/Go-hybrid Language for C-performance Systems**

Curium is a modern programming language designed for high-performance systems development. It combines the safety and ergonomics of modern languages like Go and Rust with the raw power and control of C, now powered directly by the TCC backend.

## ⚡ Curium in 10 Seconds

1. **Language:** Clean C#-like/Rust-like semantics that transpile directly to safe **C11**, compiled seamlessly via an integrated **TCC Engine**.
2. **Memory Safety:** Effortless automatic memory management using a built-in ARC garbage collector, with Data-Oriented Design override contexts via `reactor arena(size) { ... }`.
3. **Unique Typing:** Combine string/number operations with the `strnum` dual-type, and write safe error flows with `?T` Options and `Result<T,E>`.
4. **Performance Hacks:** Use `#[hot]` attribute on variables to give the CPU absolute register residency guarantees.
5. **Dynamic Operators:** Never be restricted by static operators. Define runtime logic for any syntax operator with `dyn op in (...)` and strong safety fallbacks.
6. **C/C++ Interop:** Seamlessly inject zero-cost abstraction raw code via `c { ... }` blocks.
7. **Developer Experience:** Blazing-fast CLI packed with a graphical `Neon-DOD TUI`, audio cues, and integrated package/project management natively built-in.

## 🏁 Quick Start

### Installation

Download and run the installer for your platform:

- **Windows**: `powershell -ExecutionPolicy Bypass -File .\install.ps1`
- **Linux/macOS**: `bash ./install.sh`

### Your First Program

Create a file named `hello.cm`:

```cm
fn main() {
    println("Hello, Curium v5!");
}
```

Run it immediately after building the `cm` CLI (see [Installation](docs/INSTALLATION.md)):

```bash
cm run hello.cm
```

### GitHub and editor highlighting

Repository language stats on GitHub classify `.cm` files as **C** (see [`.gitattributes`](.gitattributes)) because Linguist does not ship a Curium grammar. For accurate Curium syntax highlighting in VS Code, use the extension under [`vscode-extension/cm-language/`](vscode-extension/cm-language/).

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
    ) dyn(op == "max") {
        return x > y ? x : y;
    } dyn($) {
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
- 🛠️ **[CLI Reference](docs/CLI_REFERENCE.md)**: Detailed guide for the `cm` command and package manager.
- 🏢 **[Project Guide](docs/PROJECT_GUIDE.md)**: Information on project structure and modularity.
- 🔌 **[Compiler Internals](docs/INTERNALS.md)**: Deep dive into the Lexer, Parser, and C-codegen.
- 🚀 **[Installation Guide](docs/INSTALLATION.md)**: Setup instructions for all platforms.
- 📝 **[Implementation Examples](docs/EXAMPLES.md)**: Practical patterns for linked lists, concurrency, and more.

## 🤝 Contributing

We welcome contributions! Please check out our [GitHub repository](https://github.com/ALightbolt4G/Curium-Language) for more information.

---
*✨ Happy coding with Curium!*
