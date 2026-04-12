# 🚀 Curium Installation Guide (v5.0)

Curium is a lightweight language designed for peak performance and safety. Getting started is easy on any major platform.

---

## 🏗️ Prerequisites

Curium requires the following tools to be installed on your system:

### Windows (Primary Support)
1.  **MinGW-w64**: Download from [MinGW-w64.org](https://www.mingw-w64.org/). Ensure the `bin` directory is in your system's PATH.
2.  **CMake**: Download from [CMake.org](https://cmake.org/download/).

### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install build-essential cmake
```

### macOS
```bash
brew install cmake
```

---

## 🛠️ Step 1: Building from Source

1.  Clone the repository:
    ```bash
    git clone https://github.com/ALightbolt4G/CM-lang.git
    cd CM-lang
    ```
2.  Create and enter the build directory:
    ```bash
    cmake -B build
    ```
3.  Compile the compiler and CLI:
    ```bash
    cmake --build build
    ```

---

## 🏁 Step 2: Global Installation

After building, you can install the CLI globally:

### Windows (PowerShell)
```powershell
.\build\cm.exe install
```

### Linux/macOS
```bash
sudo ./build/cm install
```

---

## 🧪 Step 3: Verifying the Installation

1.  Open a new terminal session.
2.  Run `cm -v` to check the version.
3.  Initialize a test project:
    ```bash
    cm init hello_cm
    cd hello_cm
    cm run
    ```

If you see `Hello World!`, your installation is complete!

---

## 🛠️ Troubleshooting

### Compiler Not Found
If you get an error saying `gcc` or `clang` is missing, ensure your MinGW or Clang installation is added to the system's PATH.

### CMake Errors
Ensure you are using CMake 3.10 or higher. You can check your version with `cmake --version`.

### Windows Execution Policy
If you cannot run Curium from PowerShell, you may need to set your execution policy:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```
