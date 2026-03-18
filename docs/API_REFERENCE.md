# CM Framework API Reference

This document provides a comprehensive list of the public functions and types available in the CM Framework, organized by module.

---

## 🧠 Memory Management (`memory.h`)

| Function | Description |
|---|---|
| `cm_gc_init()` | Initializes the GC systems. Call at the start of `main`. |
| `cm_alloc(size, tag)` | Allocates tracked memory. `tag` is for debugging. |
| `cm_retain(ptr)` | Increments the reference count of an object. |
| `cm_free(ptr)` | Decrements the reference count. Frees if it hits zero. |
| `cm_gc_collect()` | Manually triggers a mark-and-sweep cycle. |
| `cm_gc_stats()` | Prints memory usage statistics to the console. |
| `cm_gc_shutdown()` | Cleans up all tracked memory and reports leaks. |
| `cm_arena_create(size)` | Creates a high-speed memory arena. |
| `CM_WITH_ARENA(name, size)` | Macro for scoped arena allocation. |

---

## 🌐 HTTP Server (`http.h`)

| Function | Description |
|---|---|
| `cm_app_get(path, handler)` | Registers a GET route. |
| `cm_app_post(path, handler)` | Registers a POST route. |
| `cm_app_put(path, handler)` | Registers a PUT route. |
| `cm_app_delete(path, handler)` | Registers a DELETE route. |
| `cm_app_listen(port)` | Starts the HTTP server on the specified port. |
| `cm_res_send(res, text)` | Sends a plain text or HTML response. |
| `cm_res_json(res, node)` | Sends a JSON response from a `CMJsonNode`. |
| `cm_res_status(res, code)` | Sets the HTTP status code (e.g., 200, 404). |
| `cm_res_send_file(res, path, ct)` | Streams a file from disk with a content-type. |

### Client-Side API

| Function | Description |
|---|---|
| `cm_http_get(url)` | Sends a synchronous GET request. |
| `cm_http_post(url, body, type)` | Sends a synchronous POST request. |
| `cm_http_put(url, body, type)` | Sends a synchronous PUT request. |
| `cm_http_delete(url)` | Sends a synchronous DELETE request. |

---

## 🔤 Dynamic Strings (`string.h`)

| Function | Description |
|---|---|
| `cm_string_new(text)` | Creates a new dynamic string. |
| `cm_string_format(fmt, ...)` | Creates a formatted string (like `sprintf`). |
| `cm_string_length(s)` | Returns the UTF-8 aware length. |
| `cm_string_free(s)` | Frees a dynamic string. |

---

## 🗺️ Hash Maps (`map.h`)

| Function | Description |
|---|---|
| `cm_map_new()` | Creates a new hash map. |
| `cm_map_set(map, key, val, sz)` | Inserts or updates a key-value pair. |
| `cm_map_get(map, key)` | Retrieves a value by its key. |
| `cm_map_save_to_json(map, path)` | Serializes the map to a JSON file. |
| `cm_map_load_from_json(path)` | Deserializes a map from a JSON file. |

---

## 📦 JSON SDK (`json.h`)

| Function | Description |
|---|---|
| `cm_json_parse(json_str)` | Parses a JSON string into a node tree. |
| `cm_json_stringify(node)` | Converts a node tree back to a dynamic string. |
| `CMJsonNode_delete(node)` | Recursively frees a JSON node tree. |

---

## 🔒 Threading & Concurrency (`thread.h`)

| Function | Description |
|---|---|
| `cm_thread_create(func, arg)` | Spawns a new concurrent thread. |
| `cm_thread_join(thread)` | Waits for a thread to complete. |
| `cm_mutex_init()` | Creates a new OS-native mutex. |
| `cm_mutex_lock(mutex)` | Locks a mutex. |
| `cm_mutex_unlock(mutex)` | Unlocks a mutex. |
| `cm_mutex_destroy(mutex)` | Destroys a mutex. |

---

## 📄 File I/O (`file.h`)

| Function | Description |
|---|---|
| `cm_file_read(path)` | Reads an entire file into a `cm_string_t`. |
| `cm_file_write(path, content)` | Writes a string to a file on disk. |

---

## 💻 CMD SDK (`cmd.h`)

| Function | Description |
|---|---|
| `cm_cmd_new(program)` | Creates a new command builder for the specified program. |
| `cm_cmd_arg(cmd, arg)` | Safely appends an argument to the command. |
| `cm_cmd_run(cmd)` | Executes the command and captures its output. |
| `cm_cmd_free(cmd)` | Frees a command builder. |
| `cm_cmd_result_free(res)` | Frees a command execution result. |
