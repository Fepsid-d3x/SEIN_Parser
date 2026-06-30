# SEIN_Parser

[![VS Marketplace](https://img.shields.io/badge/Install%20from-VS%20Marketplace-blue?logo=visualstudiocode)](https://marketplace.visualstudio.com/) ![C++](https://img.shields.io/badge/C++-17-blue) ![C](https://img.shields.io/badge/C-11-blue) ![Python](https://img.shields.io/badge/Python-3.8+-yellow)

[![DOCTEST](https://img.shields.io/badge/DOCTEST-v2.5.2-green)](sein_test.cxx)

---

Однофайловый парсер конфигураций в стиле `.ini` для C++17 / C11 / Python.

SEIN - это упрощённая и типизированная версия классического формата `.ini`. Вместо того чтобы каждый раз писать парсинг строк вручную, вы описываете секции и пары ключ/значение в файле `.sein` и загружаете их во время выполнения. Секции могут наследоваться друг от друга, значения могут ссылаться на переменные или другие ключи, и всё парсится один раз в типизированную форму. (`.sein` — это просто соглашение, подойдёт любой текстовый файл.)

```sein
@set accent = "blue"

[Player]
name  = Hero
speed = 8.5
color = ${accent}
```

> Примечание: версия на C++ обновляется чаще всего и является самой стабильной из трёх.

---

## Возможности

- **Секции** - `[SectionName]`, включая **наследование** через `[Child : [Parent]]`
- **Типизированные значения** - парсятся один раз в `Int`, `Float`, `String` или `Array`; геттеры сразу работают с хранимым типом
- **Переменные** — `@set VAR = value` и подстановка через `${Section.key}` или `${VAR}`
- **Доступ к системным переменным окружения** - `${SYSENV:VAR}` всегда читает из ОС, а не из `@set`
- **`@include`** - разбиение конфига на несколько файлов (рекурсивно, с ограничением глубины)
- **Подключи** - `key[subkey] = value` с отдельными геттерами
- **Многострочные значения** - продолжение через завершающий `\` или raw-блоки через `"R( ... )R"`
- **Асинхронный парсинг** - загрузка в фоновом потоке с флагом `usage_async`
- **API для записи** - программное создание и редактирование конфигов (во всех трёх языках)
- **Без зависимостей** - чтение через mmap, никаких внешних библиотек

---

## Установка

**C++** - подключите заголовок и компилируйте с C++17:

```cpp
#include "sein.hpp"
using namespace fd::sein;
```

**C** - определите макрос реализации один раз, подключите заголовок и слинкуйте pthreads:

```c
#define SEIN_IMPLEMENTATION
#include "sein.h"
```

**Python** - просто импортируйте:

```python
import sein
```

---

## Примеры

### Загрузка файла

```cpp
auto result = parse_sein("example.sein");
auto& cfg = result.data;
```

```python
result = sein.parse_sein("example.sein")
cfg = result.data
```

### Чтение значений

Прямые запросы с безопасным значением по умолчанию при отсутствии ключа:

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

Значения хранятся один раз как типизированный `SeinValue`, поэтому геттеры массивов одинаково работают и с одиночным значением, и с перечислением через точку с запятой.

```cpp
std::vector<float> backoff = get_float_array(cfg, "Server", "backoff_times", ';');
```

### Переменные, include и переменные окружения

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

# ${SYSENV:HOME} всегда читается из ОС — никогда из @set //
home_dir   = ${SYSENV:HOME}
```

### Наследование секций

Дочерняя секция получает все ключи от родителя и переопределяет только нужное:

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
int player_health = get_int(cfg, "Player", "health", 0);    // 100 (унаследовано)   //
float player_speed = get_float(cfg, "Player", "speed", 0); // 8.5 (переопределено) //
```

### Подключи

Удобно для группировки данных, например, кадров анимации:

```sein
[Animation]
animation["idle"] = 0; 64; 4; 16; 16; true
animation["run"]  = 0; 16; 6; 16; 16; true
```

```cpp
std::string idle = get_subkey(cfg, "Animation", "animation", "idle", "");
auto frames      = get_subkeys(cfg, "Animation", "animation");
```

### Асинхронная загрузка

```cpp
auto result = parse_sein("big_config.sein", /*usage_async=*/true);

init_renderer();
load_textures();

result.wait();
auto& cfg = result.data;
```

Проверка без блокировки:

```cpp
if (result.done()) {
    // парсинг завершён //
}
```

### API для записи (Writer)

Создание конфига с нуля и сохранение:

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

Можно также загрузить существующий файл как документ (`doc_load_as_document`), отредактировать и сохранить.

---

## Справочник API

### C++ (`sein.hpp`, `-std=c++17`)

| Функция | Возвращает |
|---|---|
| `parse_sein(path, usage_async=false)` | `SeinResult` |
| `result.data` / `result.wait()` / `result.done()` | `Config` / — / `bool` |
| `get_value_ptr / get_value / get_int / get_float / get_bool` | `SeinValue* / string / int / float / bool` |
| `get_array / get_int_array / get_float_array` | `vector<...>` |
| `get_subkey / get_subkey_view / get_subkeys` | `string / string_view / vector<string>` |

Writer (префикс `doc_`): `doc_create_new_config`, `doc_add_header_comment`, `doc_add_blank_line`, `doc_add_include`, `doc_add_global_var`, `doc_add_section`, `doc_add_value`, `doc_add_subkey_value`, `doc_remove_value`, `doc_remove_subkey_value`, `doc_remove_section`, `doc_save_config`, `doc_load_as_document`.

### C (`sein.h`, `#define SEIN_IMPLEMENTATION`, `-lpthread`)

| Функция | Возвращает |
|---|---|
| `sein_alloc / sein_parse / sein_wait / sein_destroy` | управление жизненным циклом |
| `sein_get / sein_get_int / sein_get_float / sein_get_bool` | скалярные геттеры |
| `sein_get_array / sein_get_int_array / sein_get_float_array` | геттеры массивов (возвращают количество) |
| `sein_get_subkey / sein_get_subkeys` | геттеры подключи |

Writer: `create_new_config`, `sein_doc_add_comment`, `sein_doc_add_blank`, `sein_doc_add_include`, `sein_doc_add_global_var`, `sein_doc_add_section`, `sein_doc_add_value`, `sein_doc_add_subkey_value`, `sein_doc_remove_value`, `sein_doc_remove_subkey_value`, `sein_doc_remove_section`, `sein_doc_save`, `sein_doc_free`, `sein_doc_load`.

### Python (`import sein`)

| Функция | Возвращает |
|---|---|
| `parse_sein(path, usage_async=False)` | `SeinResult` |
| `get_value_ptr / get_value / get_int / get_float / get_bool` | `SeinValue \| None / str / int / float / bool` |
| `get_array / get_int_array / get_float_array` | `list[...]` |
| `get_subkey / get_subkeys` | `str / list[str]` |

Каждый `SeinValue` также имеет методы `.as_int()`, `.as_float()`, `.as_bool()`, `.as_str()`.

Writer: `create_new_config`, `add_header_comment`, `add_blank_line`, `add_include`, `add_global_var`, `add_section`, `add_value`, `add_subkey_value`, `remove_value`, `remove_subkey_value`, `remove_section`, `save_config`, `load_as_document`.

---

## Производительность

Благодаря чтению через mmap (zero-copy), `std::from_chars` и минимальному количеству аллокаций парсер SEIN работает очень быстро — особенно в версиях на C/C++. На небольших конфигах скорость сравнима с классическими INI-парсерами, а на больших файлах с множеством `@include` mmap даёт заметное преимущество.

---

## Где SEIN особенно полезен

- Когда важны минимальный размер и отсутствие зависимостей
- В игровых движках и инструментах (ядро на C++ + скрипты на Python)
- Когда конфиг нужно разбивать на несколько файлов (`@include`)
- Для пресетов через наследование секций
- Когда нужен единый формат конфигурации для нескольких языков

## Где SEIN уступает

- Нет встроенной поддержки datetime
- Ограниченная поддержка глубокого вложенности (лучше использовать dot-секции)
- Нет встроенной валидации схемы

---

**Расширение для VSCode**

```
ext install Fepsid.sein-language-support
```