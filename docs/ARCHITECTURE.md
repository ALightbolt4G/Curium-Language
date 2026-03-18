# CM Framework Architecture

This document provides a deep dive into the internal design and key components of the CM Framework.

---

## 🧠 Memory Management & GC

CM implements a sophisticated hybrid memory management system that combines the predictability of manual management with the safety of automatic collection.

### 1. The GC Object System
Every tracked allocation is wrapped in a `CMObject` structure. This structure maintains metadata such as:
- **Reference Count**: For immediate deterministic cleanup.
- **Allocation Source**: Tracks `__FILE__` and `__LINE__` for leak reporting.
- **Object Type**: A string tag for debugging (e.g., "string", "map").
- **Destructors**: Optional callbacks to clean up internal resources (e.g., closing a file handle).

### 2. Hybrid Collection Strategy
- **Reference Counting**: Objects are freed immediately when their ref-count hits zero (`cm_free`).
- **Mark-and-Sweep**: A backup cycle (`cm_gc_collect`) can be triggered manually or during shutdown to identify and break cyclical references or recover objects that lost their handles.
- **Handle Table**: Uses a generation-based handle system (`cm_ptr_t`) to prevent "use-after-free" bugs. Resolving a handle checks if the generation still matches the underlying object.

### 3. Arena Allocation
For high-performance, short-lived tasks (like processing a single HTTP request), CM provides **Memory Arenas**.
- **Linear Allocation**: Extremely fast pointer-bumping.
- **Bulk Cleanup**: Destroying the arena frees all contained memory in one operation.
- **Scoped Usage**: The `CM_WITH_ARENA` macro provides "magical" automatic cleanup using compiler `cleanup` attributes (on GCC/Clang) or deterministic block scoping.

---

## 🌐 Networking & HTTP

The CM Server is designed for high concurrency and ease of use, mimicking the popular Express.js router.

### 1. Multi-threaded Architecture
When `cm_app_listen` is called, the server starts a master listener thread. For every incoming connection:
- A worker thread is spawned (or pulled from a pool in future versions).
- The request is parsed into a `CMHttpRequest` object.
- The router identifies the matching handler function.

### 2. The Router
Routes are registered using `cm_app_get`, `cm_app_post`, etc. The router matches the incoming URI and HTTP Method against registered handlers. It supports:
- **Static File Serving**: Efficiently streaming files from disk via `cm_res_send_file`.
- **JSON Handling**: Native integration with the CM JSON parser for `application/json` payloads.

---

## 🔒 OS-Agnostic Concurrency

CM provides a unified API for threading and synchronization, allowing the same C code to compile and run on Windows and POSIX systems without `#ifdef` blocks in application logic.

### 1. Threading (`CMThread`)
- **Windows**: Maps to `CreateThread` and `WaitForSingleObject`.
- **Linux/POSIX**: Maps to `pthread_create` and `pthread_join`.

### 2. Synchronization (`CMMutex`)
- **Windows**: Uses high-performance `CRITICAL_SECTION` objects.
- **Linux/POSIX**: Uses `pthread_mutex_t`.

---

## 📄 Core Utilities

- **Hash Maps**: Open-addressed hash maps with power-of-two resizing and JSON serialization support.
- **Dynamic Strings**: UTF-8 aware string manipulation with safe bounds checking and formatting.
- **JSON SDK**: Performance-oriented parser and generator with no external dependencies.
- **Error Detector**: A global signal/exception handler that catches crashes and prints full memory statistics before exiting.
