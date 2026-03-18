<div align="center">

<img src="https://img.shields.io/badge/version-5.0.0-blueviolet?style=for-the-badge" />
<img src="https://img.shields.io/badge/language-C99-blue?style=for-the-badge&logo=c" />
<img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux-green?style=for-the-badge" />
<img src="https://img.shields.io/badge/license-MIT-orange?style=for-the-badge" />

# 🚀 CM Framework

**A modern, enterprise-grade backend framework written in pure C99.**  
CM gives you the elegance of Express.js, the performance of raw C, and the safety of a built-in garbage collector — all in one lightweight library.

</div>

---

## ✨ Features

| Module | Description |
|---|---|
| 🧠 **Memory / GC** | Automatic garbage collection with ref-counting, mark-and-sweep, and live stats |
| 🔒 **Thread Safety** | OS-agnostic `CMMutex` and `CMThread` — wraps Win32 & POSIX transparently |
| 🌐 **HTTP Server** | Express.js-style router with `cm_app_get()` / `cm_app_post()` |
| 📄 **File I/O** | GC-tracked file reading and writing via `cm_file_read()` / `cm_file_write()` |
| 🗺️ **Hash Maps** | High-performance `cm_map_t` with automatic resizing and load-factor control |
| 🔤 **Dynamic Strings** | Safe unicode-aware `cm_string_t` with full formatting and slicing API |
| 📦 **JSON** | Full JSON parse + stringify — no third-party dependencies |
| 💾 **Map Persistence** | Serialize / deserialize maps to JSON files in one line |
| 🌍 **Unicode / UTF-8** | Native emoji and international character support on all platforms |

---

## 📁 Project Structure

```
CM/
├── include/cm/        # Public headers
│   ├── core.h         # Version, base types, macros
│   ├── memory.h       # GC allocator API
│   ├── string.h       # Dynamic string API
│   ├── array.h        # Dynamic array API
│   ├── map.h          # Hash map API + JSON persistence
│   ├── json.h         # JSON parse & stringify
│   ├── http.h         # HTTP server & router
│   ├── file.h         # File I/O API
│   ├── thread.h       # OS-agnostic threading API
│   └── error.h        # Error handling & signal detection
├── src/               # Module implementations
├── tests/             # Unit test suite (CTest)
├── public_html/       # Static frontend (HTML/CSS/JS)
│   ├── index.html     # Login + Calculator SPA
│   ├── style.css      # Glassmorphism dark UI
│   └── script.js      # Calculator frontend logic
├── main.c             # Example server entry point
└── CMakeLists.txt     # Build system
```

---

## ⚡ Quick Start

### 1. Build the Library

**Requirements:** CMake 3.10+, GCC / MinGW-w64

```bash
cmake -B build
cmake --build build
```

### 2. Run the Example Server

```bash
cd build
./cm_server.exe        # Windows
./cm_server            # Linux
```

Then open **http://localhost:8080** in your browser.  
Login with `admin` / `1234` to access the live Calculator.

---

## 🧪 Running Tests

```bash
cd build
ctest --output-on-failure
```

---

## 📖 API Reference

### 🧠 Memory & Garbage Collector

```c
cm_gc_init();                  // Initialize GC (call once on startup)
void* p = cm_alloc(size, "T"); // Allocate GC-tracked memory
cm_free(p);                    // Decrement ref count / free
cm_retain(p);                  // Increment ref count
cm_gc_collect();               // Manually trigger sweep
cm_gc_stats();                 // Print memory statistics to console
cm_gc_shutdown();              // Final sweep + shutdown
```

### 🔤 Strings

```c
cm_string_t* s = cm_string_new("Hello!");
cm_string_t* f = cm_string_format("v%s", "5.0.0");
cm_string_length(s);           // UTF-8 aware length
cm_string_free(s);
```

### 🗺️ Hash Maps

```c
cm_map_t* m = cm_map_new();
cm_map_set(m, "key", "value", 6);
char* v = (char*)cm_map_get(m, "key");
cm_map_save_to_json(m, "config.json");   // Persist to disk
cm_map_t* m2 = cm_map_load_from_json("config.json");  // Load from disk
cm_map_free(m);
```

### 🌐 HTTP Server (Express-style)

```c
void home(CMHttpRequest* req, CMHttpResponse* res) {
    cm_res_send(res, "Hello from CM!");
}

void api_data(CMHttpRequest* req, CMHttpResponse* res) {
    struct CMJsonNode* node = cm_json_parse("{\"ok\":true}");
    cm_res_json(res, node);
    CMJsonNode_delete(node);
}

int main() {
    cm_gc_init();

    cm_app_get("/",          home);
    cm_app_get("/style.css", serve_css);
    cm_app_post("/api/data", api_data);

    cm_app_listen(8080);
    cm_gc_shutdown();
}
```

### 📄 File I/O

```c
cm_file_write("out.txt", "Hello, CM!");
cm_string_t* content = cm_file_read("out.txt");
printf("%s\n", content->data);
cm_string_free(content);
```

### 🔒 Threading

```c
void* my_worker(void* arg) {
    printf("Running in thread!\n");
    return NULL;
}

CMThread t = cm_thread_create(my_worker, NULL);
cm_thread_join(t);

CMMutex lock = cm_mutex_init();
cm_mutex_lock(lock);
// ... critical section ...
cm_mutex_unlock(lock);
```

### 📦 JSON

```c
struct CMJsonNode* root = cm_json_parse("{\"name\":\"CM\",\"version\":5}");
cm_string_t* str = cm_json_stringify(root);
printf("%s\n", str->data);
cm_string_free(str);
CMJsonNode_delete(root);
```

---

## 🎨 Included Frontend Demo

CM ships with a **glassmorphism dark-mode** web app served entirely from C:

- 🔐 **Login Screen** — JWT-free session with backend auth via `POST /api/login`
- 🧮 **Calculator UI** — Fully functional, logs every calculation to the server
- 📊 **Live GC Stats** — Memory stats printed to console on every calculation

---

## 🏗️ Architecture

```
Request ─→ TCP Socket ─→ HTTP Parser ─→ Router
                                          │
                            ┌─────────────┼─────────────┐
                            ▼             ▼             ▼
                        Static File   JSON API    Log + GC Stats
                        (File I/O)   (Map/JSON)   (cm_gc_stats)
```

---

## 📊 Memory Safety Guarantee

The CM GC ensures zero unbounded leaks under normal usage:

```
══════════════════════════════════════════════════════════════
              GARBAGE COLLECTOR STATISTICS
──────────────────────────────────────────────────────────────
  Total objects    │                   39
  Total memory     │                 1524 bytes
  Peak memory      │                13058 bytes
  Allocations      │                  214
  Frees            │                  175
  Collections      │                    5
══════════════════════════════════════════════════════════════
```

---

## 🗺️ Roadmap

- [ ] HTTPS / TLS support via `cm_tls.h`
- [ ] WebSocket upgrade protocol
- [ ] Route middleware / pipeline hooks
- [ ] JWT authentication helper
- [ ] Database adapter interface

---

## 📄 License

MIT License — Copyright © 2026 **Adham Hossam**

---

<div align="center">
  Built with ❤️ and pure C99 — no dependencies, no runtime, no compromise.
</div>
