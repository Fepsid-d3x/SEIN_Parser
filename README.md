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
- **System env variables via `${SYSENV:VAR}`** — always reads from the OS, never from `@set`
- **Section inheritance** `[Child : [Parent]]`
- **Async parsing** — `usage_async` flag in `parse_sein` / `create_new_config`
- Automatic whitespace trimming
- Getters: string, int, float, bool, arrays
- Writer API for creating and editing configs programmatically

## Features
- No external dependencies
- mmap-based file reading (POSIX & Windows) — zero extra copy into kernel cache
- Lock-free hot paths in C (`_Atomic` counts, acquire/release ordering)
- `std::from_chars` in C++ — no allocations, no exceptions for number parsing
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
| `get_value(cfg, section, key, default)` | `std::string` |
| `get_int(cfg, section, key, default)` | `int` |
| `get_float(cfg, section, key, default)` | `float` |
| `get_bool(cfg, section, key, default)` | `bool` |
| `get_array(cfg, section, key)` | `vector<string>` |
| `get_int_array(cfg, section, key)` | `vector<int>` |
| `get_float_array(cfg, section, key)` | `vector<float>` |

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
| `doc_remove_value(doc, sec, key)` | Remove a key |
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
| `sein_doc_remove_value(doc, sec, key)` | Remove a key |
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
| `result.data` | `dict` | The parsed config dict |
| `result.wait()` | — | Block until async parse finishes |
| `result.done()` | `bool` | Check if async parse is complete |

**Getters** (`cfg` = `result.data`)

| Function | Returns |
|---|---|
| `get_value(cfg, section, key, default)` | `str` |
| `get_int(cfg, section, key, default)` | `int` |
| `get_float(cfg, section, key, default)` | `float` |
| `get_bool(cfg, section, key, default)` | `bool` |
| `get_array(cfg, section, key)` | `list[str]` |
| `get_int_array(cfg, section, key)` | `list[int]` |
| `get_float_array(cfg, section, key)` | `list[float]` |

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
| `remove_value(doc, sec, key)` | Remove a key |
| `remove_section(doc, name)` | Remove a section |
| `save_config(doc)` | Save to `doc["path"]` |
| `save_config(doc, path)` | Save to custom path |
| `load_as_document(path)` | Load file for editing |

---

## Comparison with other formats

| Feature | SEIN | INI | TOML | YAML | HCL | HOCON |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Single-header, no deps | ✓ | ✓ | ✗ | ✗ | ✗ | ✗ |
| mmap + zero-copy read | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |
| Async parsing | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |
| Variable substitution | ✓ | ✗ | ✗ | ~ (anchors) | ✓ | ✓ |
| OS env variable access | ✓ (`${SYSENV:VAR}`) | ✗ | ✗ | ✗ | ✓ | ✓ |
| Section inheritance | ✓ | ✗ | ✗ | ✗ | ✗ | ~ |
| `@include` directive | ✓ | ✗ | ✗ | ✗ | ✓ | ✓ |
| Arrays | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ |
| Multiline strings | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ |
| Native datetime type | ✗ | ✗ | ✓ | ✓ | ✓ | ✓ |
| Nested structures | ~ (dot sections) | ✗ | ✓ | ✓ | ✓ | ✓ |
| JSON/schema validation | ✗ | ✗ | ✗ | ~ | ✓ | ~ |
| Spec complexity | low | minimal | moderate | very high | high | high |
| C/C++ native support | ✓ | ~ | ~ (toml++) | ✗ | ✗ | ✗ |
| Python native support | ✓ | ~ | ~ | ✗ | ✗ | ✗ |

**Parsing speed** (rough order, fastest → slowest for typical game/app configs):

`INI ≈ SEIN > TOML > HCL > YAML > HOCON`

SEIN matches INI-level speed on small files and pulls ahead on large files thanks to mmap - the OS maps the file into memory without an extra kernel copy. All other formats above require at least one full read-into-buffer pass.

---

### Where SEIN wins

- **No dependencies** - drop in one `.h` / `.hpp` / `.py` file, done
- **Fastest cold-start** - mmap avoids the kernel read-copy; background thread (`usage_async`) means the rest of your app initialises in parallel
- **Inheritance** - `[Child : [Parent]]` lets you define entity templates without duplicating keys (unique among the formats listed)
- **`@include`** - split large configs across multiple files without any library support
- **`${SYSENV:VAR}`** - explicit OS-env access that never collides with your `@set` variables
- **C + C++ + Python from one codebase** - single source of truth for the parser logic

### Where SEIN loses

- **No native datetime type** - use a string and parse it yourself (TOML/HCL/YAML have first-class date/time)
- **No true nesting** - deep object graphs are awkward (YAML/TOML/HCL handle them natively)
- **No schema validation** - there is no built-in way to enforce types or required keys

---

**VSCode Extension**

```
ext install Fepsid.sein-language-support
```
