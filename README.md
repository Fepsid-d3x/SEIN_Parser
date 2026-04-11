# SEIN_Parser

---
![VS Marketplace](https://img.shields.io/badge/Install%20from-VS%20Marketplace-blue?logo=visualstudiocode) ![C++](https://img.shields.io/badge/C++-17-blue) ![C](https://img.shields.io/badge/C-11-blue) ![Python](https://img.shields.io/badge/Python-3.8+-yellow)

---

(Used in FD-SoulEngine)

A very lightweight **single-file** .ini-style config parser for C++17 / C11 / Python.
(`.sein` extension, but works with any text file)

## Supports
- Sections `[SectionName]`
- Key = value pairs
- Comments via `#`
- Multi-line values with `\` at the end of a line
- Raw multi-line strings `"R( ... )R"`
- `@include "other.sein"` directive (recursive, with depth limit)
- `@set VAR = value` global variables
- Referencing values via `${Section.key}` or `${VAR}`
- **System env variables via `${SYSENV:VAR}`** - always reads from the OS, never from `@set`
- **Section inheritance** `[Child : [Parent]]`
- **Async parsing** - `usage_async` flag in `parse_sein` / `create_new_config`
- **Subkey entries** `key[subkey] = value` with dedicated getters
- **Fully typed values** - parsed once as `Int`, `Float`, or `String`; all getters work on the stored type directly
- Automatic whitespace trimming
- Getters: string, int, float, bool, arrays, subkeys
- Writer API for creating and editing configs programmatically

## Features
- No external dependencies
- mmap-based file reading (POSIX & Windows) - zero extra copy into kernel cache
- Lock-free hot paths in C (`_Atomic` counts, acquire/release ordering)
- `std::from_chars` in C++ - no allocations, no exceptions for number parsing
- Background thread parsing (pthreads / `std::thread` / Python `threading`)
- Case-insensitive bool recognition (`true/yes/1`, `false/no/0`)
- Safe default values on all getters

---

## Example config files

```sein
# colors.sein

[Colors]
red   = 225; 0; 0; 1
green = 0; 225; 0; 1
```

```sein
# example.sein

@include "colors.sein"

@set PROJECT_NAME = "My Awesome App"
@set VERSION      = 1.4.2

[App]
name      = ${PROJECT_NAME}
full_name = ${PROJECT_NAME} v${VERSION}
author    = John Doe
debug     = true

# ${SYSENV:HOME} always reads from the OS environment - never from @set
home_dir  = ${SYSENV:HOME}

color_name = ${Colors.red}

[Server]
port          = 9000
timeout       = 30.5
backoff_times = 0.5; 1.0; 2.0; 4.0; 8.0

# Section inheritance
[Character]
name   = none
health = 100
speed  = 5.0
level  = 1

# Player inherits health/level from Character; overrides name and speed
[Player : [Character]]
name  = Hero
speed = 8.5

# Enemy also inherits from Character
[Enemy : [Character]]
name   = Goblin
health = 40

[Animation]
animation["idle"]  = 0; 64; 4; 16; 16; true
animation["run"]   = 0; 16; 6; 16; 16; true
```

---

## C++ API

Include: `#include "sein.hpp"`  
Compile: `-std=c++17`

**Parse**

| Function | Returns | Notes |
|---|---|---|
| `fd::sein::parse_sein(path, usage_async=false)` | `SeinResult` | Sync or async parse |
| `result.data` | `Config` | The parsed config map |
| `result.wait()` | — | Block until async parse finishes |
| `result.done()` | `bool` | Check if async parse is complete |

**Getters** (`cfg` = `result.data`)

| Function | Returns |
|---|---|
| `get_value_ptr(cfg, section, key)` | `const SeinValue*` |
| `get_value(cfg, section, key, default)` | `std::string` |
| `get_int(cfg, section, key, default)` | `int` |
| `get_float(cfg, section, key, default)` | `float` |
| `get_bool(cfg, section, key, default)` | `bool` |
| `get_array(cfg, section, key, delim)` | `vector<string>` |
| `get_int_array(cfg, section, key, delim)` | `vector<int>` |
| `get_float_array(cfg, section, key, delim)` | `vector<float>` |
| `get_subkey(cfg, section, key, subkey, default)` | `std::string` |
| `get_subkey_view(cfg, section, key, subkey)` | `std::string_view` |
| `get_subkeys(cfg, section, key)` | `vector<string>` |

Values are stored as typed `SeinValue` (`Int`, `Float`, `String`, `Array`). All getters work directly on the stored type — no intermediate string conversion. Array getters handle typed single values and multi-element strings equally, so `size = 32; 32` and `port = 9000` both work correctly through `get_int_array`.

**Writer** (all prefixed with `doc_`)

| Function | Description |
|---|---|
| `doc_create_new_config(path, async=false)` | Create new document |
| `doc_add_header_comment(doc, text)` | Add `# comment` at top |
| `doc_add_blank_line(doc)` | Add blank line |
| `doc_add_include(doc, path)` | Add `@include` directive |
| `doc_add_global_var(doc, name, val)` | Add `@set` variable |
| `doc_add_section(doc, name, comment)` | Add `[Section]` |
| `doc_add_value(doc, sec, key, val, comment)` | Add key = value |
| `doc_add_subkey_value(doc, sec, key, subkey, val, comment)` | Add key[subkey] = value |
| `doc_remove_value(doc, sec, key)` | Remove a key |
| `doc_remove_subkey_value(doc, sec, key, subkey)` | Remove key[subkey] |
| `doc_remove_section(doc, name)` | Remove a section |
| `doc_save_config(doc)` | Save to `doc.path` |
| `doc_save_config(doc, path)` | Save to custom path |
| `doc_load_as_document(path)` | Load file for editing |

---

## C API

Include: `#define SEIN_IMPLEMENTATION` then `#include "sein.h"`  
Compile: `-lpthread`

**Lifecycle**

| Function | Description |
|---|---|
| `sein_alloc()` | Allocate a new `SeinConfig*` |
| `sein_parse(cfg, path, usage_async)` | Parse file (sync or async) |
| `sein_wait(cfg)` | Block until async parse finishes |
| `sein_destroy(cfg)` | Free all resources |

**Getters**

| Function | Returns |
|---|---|
| `sein_get(cfg, section, key, default)` | `const char*` |
| `sein_get_int(cfg, section, key, default)` | `int` |
| `sein_get_float(cfg, section, key, default)` | `float` |
| `sein_get_bool(cfg, section, key, default)` | `int` (0 / 1) |
| `sein_get_array(cfg, sec, key, sep, out, max)` | `int` (count) |
| `sein_get_int_array(cfg, sec, key, sep, out, max)` | `int` (count) |
| `sein_get_float_array(cfg, sec, key, sep, out, max)` | `int` (count) |
| `sein_get_subkey(cfg, section, key, subkey, default)` | `const char*` |
| `sein_get_subkeys(cfg, section, key, out, max)` | `int` (count) |

**Writer**

| Function | Description |
|---|---|
| `create_new_config(path, usage_async)` | Create new `SeinDocument*` |
| `sein_doc_add_comment(doc, text)` | Add `# comment` |
| `sein_doc_add_blank(doc)` | Add blank line |
| `sein_doc_add_include(doc, path, comment)` | Add `@include` directive |
| `sein_doc_add_global_var(doc, name, val, comment)` | Add `@set` variable |
| `sein_doc_add_section(doc, name, comment)` | Add `[Section]` |
| `sein_doc_add_value(doc, sec, key, val, comment)` | Add key = value |
| `sein_doc_add_subkey_value(doc, sec, key, subkey, val, comment)` | Add key[subkey] = value |
| `sein_doc_remove_value(doc, sec, key)` | Remove a key |
| `sein_doc_remove_subkey_value(doc, sec, key, subkey)` | Remove key[subkey] |
| `sein_doc_remove_section(doc, name)` | Remove a section |
| `sein_doc_save(doc)` | Save to `doc->path` |
| `sein_doc_free(doc)` | Free all resources |
| `sein_doc_load(path)` | Load file for editing |

---

## Python API

Import: `import sein`

**Parse**

| Function | Returns | Notes |
|---|---|---|
| `sein.parse_sein(path, usage_async=False)` | `SeinResult` | Sync or async parse |
| `result.data` | `Config` | The parsed config dict |
| `result.wait()` | — | Block until async parse finishes |
| `result.done()` | `bool` | Check if async parse is complete |

**Getters** (`cfg` = `result.data`)

| Function | Returns |
|---|---|
| `get_value_ptr(cfg, section, key)` | `SeinValue \| None` |
| `get_value(cfg, section, key, default)` | `str` |
| `get_int(cfg, section, key, default)` | `int` |
| `get_float(cfg, section, key, default)` | `float` |
| `get_bool(cfg, section, key, default)` | `bool` |
| `get_array(cfg, section, key, delim)` | `list[str]` |
| `get_int_array(cfg, section, key, delim)` | `list[int]` |
| `get_float_array(cfg, section, key, delim)` | `list[float]` |
| `get_subkey(cfg, section, key, subkey, default)` | `str` |
| `get_subkeys(cfg, section, key)` | `list[str]` |

Values are stored as typed `SeinValue` objects (`SeinType.Int`, `SeinType.Float`, `SeinType.String`, `SeinType.Array`). Each value exposes `.as_int()`, `.as_float()`, `.as_bool()`, `.as_str()` methods. All getters work on the stored type directly.

**Writer**

| Function | Description |
|---|---|
| `create_new_config(path, usage_async=False)` | Create new document |
| `add_header_comment(doc, text)` | Add `# comment` |
| `add_blank_line(doc)` | Add blank line |
| `add_include(doc, path)` | Add `@include` directive |
| `add_global_var(doc, name, val)` | Add `@set` variable |
| `add_section(doc, name, comment=None)` | Add `[Section]` |
| `add_value(doc, sec, key, val, comment=None)` | Add key = value |
| `add_subkey_value(doc, sec, key, subkey, val, comment=None)` | Add key[subkey] = value |
| `remove_value(doc, sec, key)` | Remove a key |
| `remove_subkey_value(doc, sec, key, subkey)` | Remove key[subkey] |
| `remove_section(doc, name)` | Remove a section |
| `save_config(doc)` | Save to `doc.path` |
| `save_config(doc, path)` | Save to custom path |
| `load_as_document(path)` | Load file for editing |

---

### Performance
- **Thanks to the use of mmap (zero-copy read), `std::from_chars` and minimal allocations, SEIN shows very good parsing speed, especially in C/C++ versions.**
- **On small configs, it is comparable to classic INI parsers. On large files with many `@include`, mmap offers a noticeable advantage over approaches that read the entire file into memory.**

### Where SEIN is especially useful
- **When minimum size and no dependencies are important**
- **In game engines and tools (C++ core + Python scripts)**
- **Split config into multiple files if necessary (`@include`)**
- **When you need presets through section inheritance**
- **If you want a single config format across multiple languages**

### Where SEIN Falls Short
- **No built-in support for datetime**
- **Limited support for deep nesting (dot sections are better)**
- **No built-in schema validation**

---

**VSCode Extension**

```
ext install Fepsid.sein-language-support
```