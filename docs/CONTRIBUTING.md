# Contributing to CM Framework

We're excited that you're interested in contributing to CM Framework! As an enterprise-grade C99 backend library, we maintain high standards for code quality, memory safety, and performance.

---

## 📜 Development Philosophy

- **Pure C99**: No C++ features or external dependencies.
- **Memory Safety First**: Every allocation must be tracked by the GC.
- **Portability**: All core features must work on both Windows (Win32) and Linux (POSIX).
- **Performance**: Avoid unnecessary copies and minimize CPU overhead.

---

## 🛠️ Contribution Workflow

### 1. Identify an Issue or Feature
- Browse the [Issue Tracker](https://github.com/adhamhossam/CM/issues).
- If you're proposing a new feature, please open an issue first for discussion.

### 2. Set Up Your Environment
Follow the instructions in **[Getting Started](./GETTING_STARTED.md)** to build the framework and run the test suite.

### 3. Coding Guidelines
- **Indent**: Use 4 spaces for indentation.
- **Naming**: Use `snake_case` for function and variable names. Public APIs should be prefixed with `cm_`.
- **Headers**: Public headers go in `include/cm/`, private implementations go in `src/`.
- **Documentation**: Use Doxygen-style comments for all public functions in header files.

### 4. Testing
- CM uses CTest for unit testing.
- New features **must** include corresponding tests in the `tests/` directory.
- Ensure all tests pass before submitting a Pull Request:
  ```bash
  cd build
  ctest --output-on-failure
  ```

---

## 📮 Submitting a Pull Request

1. Fork the repository.
2. Create a descriptive branch name (e.g., `feat/json-performance` or `fix/memory-leak-in-map`).
3. Commit your changes with clear, concise messages.
4. Open a Pull Request against the `main` branch.
5. Address any feedback from the maintainers.

---

## ⚖️ License
By contributing to CM Framework, you agree that your contributions will be licensed under the **MIT License**.
