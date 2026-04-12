# 🛠️ Curium CLI Reference (v5.0)

Curium provides a powerful command-line interface for managing, building, and running projects.

## 🚀 Basic Commands

### **cm init <project-name>**
Initializes a new Curium project in a directory with the given name.
- Creates a `src/` directory and a `curium.json` manifest.

### **cm build [entry.cm]**
Compiles the specified entry file (or searches for `main.cm` in `src/`).
- **Options**:
  - `-o <path>`: Specify output binary path.
  - `--emit-c`: Only transpile to C, don't compile.

### **cm run [entry.cm]**
Builds and immediately executes the Curium program.
- **Example**: `cm run my_app.cm`

### **cm check [file.cm]**
Performs a fast type-check without generating C code or binaries. Use this for quick validation during development.

### **cm doctor [project-dir]**
Diagnoses project health, checking for missing dependencies, incorrect manifest settings, or environment issues.

---

## 📦 Package Manager

The Curium package manager is built directly into the CLI.

### **cm packages init [<name>]**
Initializes a package manifest for a library project.

### **cm packages install [name@version]**
Installs a remote package from the official Curium registry.
- **Example**: `cm packages install http@1.0.1`

### **cm packages remove <name>**
Removes an installed package from the project.

### **cm packages list**
Displays all installed packages and their versions.

---

## 🔧 Maintenance & Installation

### **cm fmt [file.cm]**
Automatically formats your Curium source code according to the standard style guide.

### **cm test**
Discovers and runs all `.cm` files in the `tests/` directory.

### **cm install [-o path]**
Installs the compiled binary to your system's global bin directory.

### **cm emitc <entry.cm>**
A specialized command to only output the transpiled C99 source code. Useful for cross-compiling or auditing.

---

## 🏴 Flags

| Flag | Name | Description |
|---|---|---|
| `-o <path>` | Output | Specifies the output file path. |
| `--emit-c` | Emit C | Skip C compilation step. |
| `--stat` | Statistics | Show detailed memory and GC stats after execution. |
| `-v`, `--version` | Version | Display current Curium version. |
