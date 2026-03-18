# CM Framework: The Enterprise C99 Backend Engine

## 🚀 Mission Statement

CM (C-Modules) is designed to bridge the gap between high-level development ease and low-level C performance. Our mission is to provide developers with a robust, memory-safe, and highly concurrent framework for building enterprise-grade backend systems without the overhead of heavy runtimes or garbage-collected languages like Java or Go.

### Why CM?

- **Pure C99**: No C++ complexity, just clean, portable C.
- **Zero Dependencies**: Everything from JSON parsing to HTTP handling is built from scratch.
- **Embedded Memory Safety**: A custom Garbage Collector (GC) brings the safety of modern languages to the performance of C.
- **Express-Style API**: A familiar routing syntax makes transitioning from Node.js or Python seamless.

---

## 🛠️ Core Features

### 🧠 Intelligent Memory Management
Forget `malloc` and `free` nightmares. CM features a hybrid GC that uses reference counting for immediate cleanup and a mark-and-sweep cycle for cyclical references.

### 🌐 Scalable Networking
Powered by a multi-threaded HTTP server and a flexible routing engine, CM handles thousands of concurrent requests with minimal CPU and RAM footprint.

### 🔒 Cross-Platform Concurrency
Wait-free mutexes and OS-agnostic threading wrappers ensure your code runs identically on Windows (Win32) and Linux (POSIX).

### 📄 Safe File I/O & Persistence
Disk operations are natively tracked by the GC, and our Hash Maps can be persisted to JSON files with a single function call.

---

## 🧭 Documentation Map

- **[Getting Started](./GETTING_STARTED.md)**: Build your first CM application in minutes.
- **[Architecture Deep-Dive](./ARCHITECTURE.md)**: Understand how the GC and Threading models work under the hood.
- **[Full API Reference](./API_REFERENCE.md)**: Detailed documentation for every function and type.
- **[Real-World Examples](./EXAMPLES.md)**: See CM in action with REST APIs and processing workers.
- **[Contributing Guide](./CONTRIBUTING.md)**: Help us build the future of C-based backend development.
