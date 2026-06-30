# SEIN_Parser

[![VS Marketplace](https://img.shields.io/badge/Install%20from-VS%20Marketplace-blue?logo=visualstudiocode)](https://marketplace.visualstudio.com/) ![C++](https://img.shields.io/badge/C++-17-blue) ![C](https://img.shields.io/badge/C-11-blue) ![Python](https://img.shields.io/badge/Python-3.8+-yellow)

[![DOCTEST](https://img.shields.io/badge/DOCTEST-v2.5.2-green)](sein_test.cxx)

---

Single-file .ini-style config parser for C++17 / C11 / Python.

SEIN is a simplified, typed take on the classic `.ini` format. Instead of hand-rolling string parsing for every config you ship, you describe sections and key/value pairs in a `.sein` file and load them at runtime - sections can inherit from one another, values can reference variables or other keys, and everything is parsed once into a typed form. (`.sein` is just a convention - any text file works.)

```sein
@set accent = "blue"

[Player]
name  = Hero
speed = 8.5
color = ${accent}
```

> Note: the C++ version is updated more often and is the most stable of the three.

---

## Features

- **Sections** - `[SectionName]`, including **inheritance** via `[Child : [Parent]]`
- **Typed values** - parsed once into `Int`, `Float`, `String`, or `Array`; getters read the stored type directly
- **Variables** - `@set VAR = value` and substitution via `${Section.key}` or `${VAR}`
- **System env access** - `${SYSENV:VAR}` always reads from the OS, never from `@set`
- **`@include`** - split a config across multiple files (recursive, depth-limited)
- **Subkeys** - `key[subkey] = value` with dedicated getters
- **Multi-line values** - trailing `\` continuation, or raw blocks via `"R( ... )R"`
- **Async parsing** - background-thread loading via the `usage_async` flag
- **Writer API** - build and edit configs programmatically, in all three languages
- **No dependencies** - mmap-based reads, no external libraries

---

## Setup

**C++** - include the header and compile with C++17:

```cpp
#include "sein.hpp"
using namespace fd::sein;
```

**C** - define the implementation macro once, then include and link pthreads:

```c
#define SEIN_IMPLEMENTATION
#include "sein.h"
```

**Python** - just import it:

```python
import sein
```

---

## Examples

### Loading a file

```cpp
auto result = parse_sein("example.sein");
auto& cfg = result.data;
```

```python
result = sein.parse_sein("example.sein")
cfg = result.data
```

### Reading values

Direct lookups, with a safe fallback if the key is missing:

```cpp
std::string name = get_value(cfg, "App", "name", "unknown");
int    port      = get_int(cfg, "Server", "port", 8080);
float  timeout   = get_float(cfg, "Server", "timeout", 5.0f);
bool   debug     = get_bool(cfg, "App", "debug", false);
```

```python
name    = sein.get_value(cfg, "App", "name", "unknown")
port    = sein.get_int(cfg, "Server", "port", 8080)
timeout = sein.get_float(cfg, "Server", "timeout", 5.0)
debug   = sein.get_bool(cfg, "App", "debug", False)
```

Values are stored once as a typed `SeinValue`, so array getters handle both a single typed value and a multi-element string the same way - `port = 9000` and `size = 32; 32` both work correctly through `get_int_array`.

```cpp
std::vector<float> backoff = get_float_array(cfg, "Server", "backoff_times", ';');
```

### Variables, includes, and env vars

`colors.sein`:
```sein
[Colors]
red = 225; 0; 0; 1
```

`example.sein`:
```sein
@include "colors.sein"

@set PROJECT_NAME = "My Awesome App"
@set VERSION      = 1.4.2

[App]
name       = ${PROJECT_NAME}
full_name  = ${PROJECT_NAME} v${VERSION}
color_name = ${Colors.red}

# ${SYSENV:HOME} always reads from the OS - never from @set //
home_dir   = ${SYSENV:HOME}
```

### Section inheritance

A child section pulls in every key from its parent and overrides only what it needs:

```sein
[Character]
name   = none
health = 100
speed  = 5.0
level  = 1

[Player : [Character]]
name  = Hero
speed = 8.5

[Enemy : [Character]]
name   = Goblin
health = 40
```

```cpp
int player_health = get_int(cfg, "Player", "health", 0); // 100, inherited
float player_speed = get_float(cfg, "Player", "speed", 0); // 8.5, overridden
```

### Subkeys

Useful for grouped data like animation frames:

```sein
[Animation]
animation["idle"] = 0; 64; 4; 16; 16; true
animation["run"]  = 0; 16; 6; 16; 16; true
```

```cpp
std::string idle = get_subkey(cfg, "Animation", "animation", "idle", "");
auto frames       = get_subkeys(cfg, "Animation", "animation");
```

### Async loading

```cpp
auto result = parse_sein("big_config.sein", /*usage_async=*/true);

init_renderer();
load_textures();

result.wait();
auto& cfg = result.data;
```

Non-blocking check:

```cpp
if (result.done()) {
    // parsing is complete //
}
```

### Writer API

Build a config from scratch and save it back out:

```cpp
auto doc = doc_create_new_config("generated.sein");

doc_add_header_comment(doc, "auto-generated config");
doc_add_global_var(doc, "VERSION", "1.0.0");
doc_add_section(doc, "App", "");
doc_add_value(doc, "App", "name", "My App", "");
doc_add_value(doc, "App", "version", "${VERSION}", "");

doc_save_config(doc);
```

```python
doc = sein.create_new_config("generated.sein")

sein.add_header_comment(doc, "auto-generated config")
sein.add_global_var(doc, "VERSION", "1.0.0")
sein.add_section(doc, "App")
sein.add_value(doc, "App", "name", "My App")
sein.add_value(doc, "App", "version", "${VERSION}")

sein.save_config(doc)
```

You can also load an existing file as a document and edit it in place with `doc_load_as_document` / `remove_value` / `remove_section`, then save.

---

## API Reference

### C++ (`sein.hpp`, `-std=c++17`)

| Function | Returns |
|---|---|
| `parse_sein(path, usage_async=false)` | `SeinResult` |
| `result.data` / `result.wait()` / `result.done()` | `Config` / — / `bool` |
| `get_value_ptr / get_value / get_int / get_float / get_bool` | `SeinValue* / string / int / float / bool` |
| `get_array / get_int_array / get_float_array` | `vector<...>` |
| `get_subkey / get_subkey_view / get_subkeys` | `string / string_view / vector<string>` |

Writer (`doc_` prefix): `doc_create_new_config`, `doc_add_header_comment`, `doc_add_blank_line`, `doc_add_include`, `doc_add_global_var`, `doc_add_section`, `doc_add_value`, `doc_add_subkey_value`, `doc_remove_value`, `doc_remove_subkey_value`, `doc_remove_section`, `doc_save_config`, `doc_load_as_document`.

### C (`sein.h`, `#define SEIN_IMPLEMENTATION`, `-lpthread`)

| Function | Returns |
|---|---|
| `sein_alloc / sein_parse / sein_wait / sein_destroy` | lifecycle |
| `sein_get / sein_get_int / sein_get_float / sein_get_bool` | scalar getters |
| `sein_get_array / sein_get_int_array / sein_get_float_array` | array getters, return count |
| `sein_get_subkey / sein_get_subkeys` | subkey getters |

Writer: `create_new_config`, `sein_doc_add_comment`, `sein_doc_add_blank`, `sein_doc_add_include`, `sein_doc_add_global_var`, `sein_doc_add_section`, `sein_doc_add_value`, `sein_doc_add_subkey_value`, `sein_doc_remove_value`, `sein_doc_remove_subkey_value`, `sein_doc_remove_section`, `sein_doc_save`, `sein_doc_free`, `sein_doc_load`.

### Python (`import sein`)

| Function | Returns |
|---|---|
| `parse_sein(path, usage_async=False)` | `SeinResult` |
| `get_value_ptr / get_value / get_int / get_float / get_bool` | `SeinValue \| None / str / int / float / bool` |
| `get_array / get_int_array / get_float_array` | `list[...]` |
| `get_subkey / get_subkeys` | `str / list[str]` |

Each `SeinValue` also exposes `.as_int()`, `.as_float()`, `.as_bool()`, `.as_str()`.

Writer: `create_new_config`, `add_header_comment`, `add_blank_line`, `add_include`, `add_global_var`, `add_section`, `add_value`, `add_subkey_value`, `remove_value`, `remove_subkey_value`, `remove_section`, `save_config`, `load_as_document`.

---

## Performance

Thanks to mmap-based (zero-copy) reads, `std::from_chars`, and minimal allocations, SEIN parses very quickly - especially in the C/C++ versions. On small configs it's comparable to classic INI parsers; on larger files with many `@include` directives, mmap gives a noticeable edge over approaches that read the whole file into memory.

---

## Where SEIN is especially useful

- When minimal size and zero dependencies matter
- In game engines and tools (C++ core + Python scripts)
- When a config needs to be split across multiple files (`@include`)
- When you need presets via section inheritance
- When you want one config format shared across multiple languages

## Where SEIN falls short

- No built-in support for datetime
- Limited support for deep nesting (dot-sections are a better fit)
- No built-in schema validation

---

**VSCode Extension**

```
ext install Fepsid.sein-language-support
```