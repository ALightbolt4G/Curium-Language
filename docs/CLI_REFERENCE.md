# 🛠️ Curium CLI Reference (v4.0)

Curium provides a powerful command-line interface for managing, building, and running projects.

## 🚀 Basic Commands

### **curium init <project-name>**
Initializes a new Curium project in a directory with the given name.
- Creates a `src/` directory and a `curium.json` manifest.

### **curium build [entry.cm]**
Compiles the specified entry file (or searches for `main.cm` in `src/`).
- **Options**:
  - `-o <path>`: Specify output binary path.
  - `--emit-c`: Only transpile to C, don't compile.

### **curium run [entry.cm]**
Builds and immediately executes the Curium program.
- **Example**: `curium run my_app.cm`

### **curium check [file.cm]**
Performs a fast type-check without generating C code or binaries. Use this for quick validation during development.

### **curium doctor [project-dir]**
Diagnoses project health, checking for missing dependencies, incorrect manifest settings, or environment issues.

---

## 📦 Package Manager

The Curium package manager is built directly into the CLI.

### **curium packages init [<name>]**
Initializes a package manifest for a library project.

### **curium packages install [name@version]**
Installs a remote package from the official Curium registry.
- **Example**: `curium packages install http@1.0.1`

### **curium packages remove <name>**
Removes an installed package from the project.

### **curium packages list**
Displays all installed packages and their versions.

---

## 🔧 Maintenance & Installation

### **curium fmt [file.cm]**
Automatically formats your Curium source code according to the standard style guide.

### **curium test**
Discovers and runs all `.cm` files in the `tests/` directory.

### **curium install [-o path]**
Installs the compiled binary to your system's global bin directory.

### **curium emitc <entry.cm>**
A specialized command to only output the transpiled C99 source code. Useful for cross-compiling or auditing.

---

## 🏴 Flags

| Flag | Name | Description |
|---|---|---|
| `-o <path>` | Output | Specifies the output file path. |
| `--emit-c` | Emit C | Skip C compilation step. |
| `--stat` | Statistics | Show detailed memory and GC stats after execution. |
| `-v`, `--version` | Version | Display current Curium version. |
