# 📂 Curium Project Guide

This document outlines the standard conventions and project structure recommended for building Curium applications and libraries.

---

## 🏗️ Standard Folder Structure

While Curium provides flexibility, following these conventions ensures compatibility with the `curium` CLI and package manager.

```text
my_project/
├── curium.json         <- Project manifest and dependencies.
├── src/                <- Your Curium source code.
│   ├── main.cm         <- Standard entry point for applications.
│   ├── lib.cm          <- Standard entry point for library exports.
│   └── util/           <- Module directories.
├── tests/              <- Automation tests (.cm files).
├── .build/             <- Internal build artifacts (ignore).
└── README.md           <- Project description.
```

---

## 🛠️ Modularity and Imports

Curium uses a simple file-based import system.

### Creating a Module
Any `.cm` file can be used as a module. Symbols marked with the `pub` keyword are exported.

```cm
// src/math/util.cm
pub fn add(a: int, b: int) -> int {
    return a + b;
}
```

### Importing a Module
Use the `import` keyword to bring a local or remote module into your scope.

```cm
// src/main.cm
import "math/util.cm";

fn main() {
    let x = add(10, 20);
    println(x);
}
```

---

## 📦 Creating Packages

If you want to share your code with others, you'll need a `curium.json` manifest.

### Example `curium.json`:
```json
{
    "name": "my_package",
    "version": "1.0.0",
    "entry": "src/lib.cm",
    "dependencies": {
        "http": ">=1.0.0"
    }
}
```

### Manifest Fields:
- **`name`**: The unique identifier for your package.
- **`version`**: Semantic versioning (e.g., `1.2.3`).
- **`entry`**: The primary export file for your module.
- **`dependencies`**: Other Curium packages required by your code.

---

## 🧪 Testing Your Code

Curium makes testing easy. Simply place your test scripts in the `tests/` folder.

1.  Create a file like `tests/math_test.cm`.
2.  Write your test logic as a `fn main()`.
3.  Run `curium test` from the project root.

The CLI will automatically find, compile, and run every `.cm` file in the folder and report passes or failures.
