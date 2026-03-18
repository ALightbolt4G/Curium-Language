# Getting Started with CM Framework

This guide will walk you through setting up your environment, building the library, and running your first "Hello World" backend.

## 📋 Prerequisites

Before you begin, ensure you have the following installed:

- **CMake** (3.10 or higher)
- **C Compiler**: 
  - **Windows**: MinGW-w64 or MSVC
  - **Linux**: GCC or Clang
- **Git** (optional, for version control)

---

## 🏗️ 1. Building the Library

CM uses CMake for a flexible, cross-platform build process.

```bash
# Clone the repository
git clone https://github.com/adhamhossam/CM.git
cd CM

# Generate build files
cmake -B build

# Build the project
cmake --build build
```

The build process generates:
- `cm_server.exe` (or `cm_server` on Linux): An example server.
- `cm_test.exe`: The internal unit test suite.

---

## 🧪 2. Running Your First Test

Verify your installation by running the test suite:

```bash
cd build
ctest --output-on-failure
```

If all tests pass, you are ready to build!

---

## 🌐 3. Hello World: Your First Server

Create a new file called `app.c` and paste the following code:

```c
#include <cm/core.h>
#include <cm/http.h>

void hello_handler(CMHttpRequest* req, CMHttpResponse* res) {
    cm_res_send(res, "<h1>Hello, Enterprise C!</h1>");
}

int main() {
    // 1. Initialize the framework and GC
    cm_init();

    // 2. Define a route
    cm_app_get("/", hello_handler);

    // 3. Start the server on port 8080
    printf("Server starting on http://localhost:8080\n");
    cm_app_listen(8080);

    // 4. Shutdown gracefully
    cm_shutdown();
    return 0;
}
```

### Compile and Run

```bash
gcc app.c -Iinclude -Lbuild -lcm -o my_app
./my_app
```

Now open **http://localhost:8080** in your browser.

---

## 📁 Project Structure

When building an enterprise application with CM, we recommend the following layout:

- `/include`: Public header files for your project.
- `/src`: Your C implementation files.
- `/public_html`: Your static frontend assets (HTML, CSS, JS).
- `/config`: JSON configuration files (leveraging `cm_map_load_from_json`).

---

## 🛠️ Next Steps

- Explore the **[API Reference](./API_REFERENCE.md)** to see available data structures.
- Learn about the **[Garbage Collector](./ARCHITECTURE.md)** to write leak-free code.
- Check out **[Advanced Examples](./EXAMPLES.md)** for more complex use cases.
