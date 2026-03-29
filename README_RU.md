# SEIN_Parser

---
![VS Marketplace](https://img.shields.io/badge/Install%20from-VS%20Marketplace-blue?logo=visualstudiocode) ![C++](https://img.shields.io/badge/C++-17-blue) ![C](https://img.shields.io/badge/C-11-blue) ![Python](https://img.shields.io/badge/Python-3.8+-yellow)

---

(Используется в FD-SoulEngine)

Очень лёгкий **однофайловый** парсер конфигов в стиле .ini для C++17 / C11 / Python
(расширение `.sein`, но работает с любыми текстовыми файлами)

## Поддерживает
- Секции `[НазваниеСекции]`
- Пары ключ = значение
- Комментарии через `#`
- Многострочные значения с `\` в конце строки
- Сырые многострочные строки `"R( ... )R"`
- Директиву `@include "другой.sein"` (рекурсивно, с ограничением глубины)
- Глобальные переменные `@set VAR = значение`
- Подстановку значений через `${Section.key}` и `${VAR}`
- **Системные переменные окружения через `${SYSENV:VAR}`** — всегда читает из ОС, никогда из `@set`
- **Наследование секций** `[Child : [Parent]]`
- **Асинхронный парсинг** — флаг `usage_async` в `parse_sein` / `create_new_config`
- Автоматическое удаление пробелов
- Геттеры: string, int, float, bool, массивы
- Writer API для программного создания и редактирования конфигов

## Возможности
- Без внешних зависимостей
- Чтение файлов через mmap (POSIX и Windows) - нет лишнего копирования через ядро
- Lock-free горячие пути в C (`_Atomic`-счётчики, acquire/release)
- `std::from_chars` в C++ - без аллокаций и исключений при парсинге чисел
- Фоновый парсинг в потоке (pthreads / `std::thread` / Python `threading`)
- Распознавание bool без учёта регистра (`true/yes/1`, `false/no/0`)
- Безопасные значения по умолчанию во всех геттерах

---

## Пример файлов конфига

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

# ${SYSENV:HOME} всегда берётся из окружения ОС - никогда из @set
home_dir  = ${SYSENV:HOME}

color_name = ${Colors.red}

[Server]
port          = 9000
timeout       = 30.5
backoff_times = 0.5; 1.0; 2.0; 4.0; 8.0

# Наследование секций
[Character]
name   = none
health = 100
speed  = 5.0
level  = 1

# Player наследует health/level из Character; переопределяет name и speed
[Player : [Character]]
name  = Hero
speed = 8.5

# Enemy тоже наследует из Character
[Enemy : [Character]]
name   = Goblin
health = 40
```

---

## C++ API

Подключение: `#include "sein.hpp"`  
Компиляция: `-std=c++17`

**Парсинг**

| Функция | Возвращает | Примечание |
|---|---|---|
| `fd::sein::parse_sein(path, usage_async=false)` | `SeinResult` | Синхронный или асинхронный парсинг |
| `result.data` | `Config` | Карта разобранного конфига |
| `result.wait()` | — | Ожидание завершения асинхронного парсинга |
| `result.done()` | `bool` | Проверка завершения асинхронного парсинга |

**Геттеры** (`cfg` = `result.data`)

| Функция | Возвращает |
|---|---|
| `get_value(cfg, section, key, default)` | `std::string` |
| `get_int(cfg, section, key, default)` | `int` |
| `get_float(cfg, section, key, default)` | `float` |
| `get_bool(cfg, section, key, default)` | `bool` |
| `get_array(cfg, section, key)` | `vector<string>` |
| `get_int_array(cfg, section, key)` | `vector<int>` |
| `get_float_array(cfg, section, key)` | `vector<float>` |

**Writer** (все с префиксом `doc_`)

| Функция | Описание |
|---|---|
| `doc_create_new_config(path, async=false)` | Создать новый документ |
| `doc_add_header_comment(doc, text)` | Добавить `# комментарий` в начало |
| `doc_add_blank_line(doc)` | Добавить пустую строку |
| `doc_add_include(doc, path)` | Добавить `@include` |
| `doc_add_global_var(doc, name, val)` | Добавить `@set` переменную |
| `doc_add_section(doc, name, comment)` | Добавить `[Секцию]` |
| `doc_add_value(doc, sec, key, val, comment)` | Добавить key = value |
| `doc_remove_value(doc, sec, key)` | Удалить ключ |
| `doc_remove_section(doc, name)` | Удалить секцию |
| `doc_save_config(doc)` | Сохранить в `doc.path` |
| `doc_save_config(doc, path)` | Сохранить по указанному пути |
| `doc_load_as_document(path)` | Загрузить файл для редактирования |

---

## C API

Подключение: `#define SEIN_IMPLEMENTATION`, затем `#include "sein.h"`  
Компиляция: `-lpthread`

**Жизненный цикл**

| Функция | Описание |
|---|---|
| `sein_alloc()` | Выделить новый `SeinConfig*` |
| `sein_parse(cfg, path, usage_async)` | Разобрать файл (синхронно или асинхронно) |
| `sein_wait(cfg)` | Ожидать завершения асинхронного парсинга |
| `sein_destroy(cfg)` | Освободить все ресурсы |

**Геттеры**

| Функция | Возвращает |
|---|---|
| `sein_get(cfg, section, key, default)` | `const char*` |
| `sein_get_int(cfg, section, key, default)` | `int` |
| `sein_get_float(cfg, section, key, default)` | `float` |
| `sein_get_bool(cfg, section, key, default)` | `int` (0 / 1) |
| `sein_get_array(cfg, sec, key, sep, out, max)` | `int` (количество) |
| `sein_get_int_array(cfg, sec, key, sep, out, max)` | `int` (количество) |
| `sein_get_float_array(cfg, sec, key, sep, out, max)` | `int` (количество) |

**Writer**

| Функция | Описание |
|---|---|
| `sein_doc_create_new_config(path, usage_async)` | Создать новый `SeinDocument*` |
| `sein_doc_add_comment(doc, text)` | Добавить `# комментарий` |
| `sein_doc_add_blank(doc)` | Добавить пустую строку |
| `sein_doc_add_include(doc, path, comment)` | Добавить `@include` |
| `sein_doc_add_global_var(doc, name, val, comment)` | Добавить `@set` переменную |
| `sein_doc_add_section(doc, name, comment)` | Добавить `[Секцию]` |
| `sein_doc_add_value(doc, sec, key, val, comment)` | Добавить key = value |
| `sein_doc_remove_value(doc, sec, key)` | Удалить ключ |
| `sein_doc_remove_section(doc, name)` | Удалить секцию |
| `sein_doc_save(doc)` | Сохранить в `doc->path` |
| `sein_doc_free(doc)` | Освободить все ресурсы |
| `sein_doc_load(path)` | Загрузить файл для редактирования |

---

## Python API

Импорт: `import sein`

**Парсинг**

| Функция | Возвращает | Примечание |
|---|---|---|
| `sein.parse_sein(path, usage_async=False)` | `SeinResult` | Синхронный или асинхронный парсинг |
| `result.data` | `dict` | Словарь разобранного конфига |
| `result.wait()` | — | Ожидание завершения асинхронного парсинга |
| `result.done()` | `bool` | Проверка завершения асинхронного парсинга |

**Геттеры** (`cfg` = `result.data`)

| Функция | Возвращает |
|---|---|
| `get_value(cfg, section, key, default)` | `str` |
| `get_int(cfg, section, key, default)` | `int` |
| `get_float(cfg, section, key, default)` | `float` |
| `get_bool(cfg, section, key, default)` | `bool` |
| `get_array(cfg, section, key)` | `list[str]` |
| `get_int_array(cfg, section, key)` | `list[int]` |
| `get_float_array(cfg, section, key)` | `list[float]` |

**Writer**

| Функция | Описание |
|---|---|
| `create_new_config(path, usage_async=False)` | Создать новый документ |
| `add_header_comment(doc, text)` | Добавить `# комментарий` |
| `add_blank_line(doc)` | Добавить пустую строку |
| `add_include(doc, path)` | Добавить `@include` |
| `add_global_var(doc, name, val)` | Добавить `@set` переменную |
| `add_section(doc, name, comment=None)` | Добавить `[Секцию]` |
| `add_value(doc, sec, key, val, comment=None)` | Добавить key = value |
| `remove_value(doc, sec, key)` | Удалить ключ |
| `remove_section(doc, name)` | Удалить секцию |
| `save_config(doc)` | Сохранить в `doc["path"]` |
| `save_config(doc, path)` | Сохранить по указанному пути |
| `load_as_document(path)` | Загрузить файл для редактирования |

---

## Сравнение с другими форматами

| Функция / особенность | SEIN | INI | TOML | YAML | HCL | HOCON |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| Один файл, без зависимостей | ✓ | ✓ | ✗ | ✗ | ✗ | ✗ |
| mmap + zero-copy чтение | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |
| Асинхронный парсинг | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ |
| Подстановка переменных | ✓ | ✗ | ✗ | ~ (anchors) | ✓ | ✓ |
| Доступ к переменным ОС | ✓ (`${SYSENV:VAR}`) | ✗ | ✗ | ✗ | ✓ | ✓ |
| Наследование секций | ✓ | ✗ | ✗ | ✗ | ✗ | ~ |
| Директива `@include` | ✓ | ✗ | ✗ | ✗ | ✓ | ✓ |
| Массивы | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ |
| Многострочные строки | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ |
| Нативный тип datetime | ✗ | ✗ | ✓ | ✓ | ✓ | ✓ |
| Глубокая вложенность | ~ (dot-секции) | ✗ | ✓ | ✓ | ✓ | ✓ |
| JSON / schema-валидация | ✗ | ✗ | ✗ | ~ | ✓ | ~ |
| Сложность спецификации | низкая | минимальная | средняя | очень высокая | высокая | высокая |
| Нативная поддержка C/C++ | ✓ | ~ | ~ (toml++) | ✗ | ✗ | ✗ |
| Нативная поддержка Python | ✓ | ~ | ~ | ✗ | ✗ | ✗ |

**Скорость парсинга** (грубый порядок, от быстрого к медленному для типичных game/app-конфигов):

`INI ≈ SEIN > TOML > HCL > YAML > HOCON`

На малых файлах SEIN сопоставим с INI. На больших файлах вырывается вперёд благодаря mmap - ОС проецирует файл в память без лишнего копирования через ядро. Все остальные форматы выше требуют как минимум одного полного прохода read-в-буфер.

---

### Где SEIN выигрывает

- **Нет зависимостей** - бросил один `.h` / `.hpp` / `.py`, и всё
- **Быстрый холодный старт** - mmap убирает копирование ядра; фоновый поток (`usage_async`) позволяет инициализировать остальное приложение параллельно
- **Наследование** - `[Child : [Parent]]` позволяет описывать шаблоны сущностей без дублирования ключей (уникальная функция среди перечисленных форматов)
- **`@include`** - разбивать большой конфиг на файлы без какой-либо поддержки библиотек
- **`${SYSENV:VAR}`** - явный доступ к переменным ОС, который никогда не конфликтует с `@set`-переменными
- **C + C++ + Python из одной кодовой базы** - один источник правды для логики парсера

### Где SEIN проигрывает

- **Нет нативного типа datetime** - нужно хранить строку и парсить самостоятельно (TOML/HCL/YAML поддерживают даты из коробки)
- **Нет глубокой вложенности** - сложные объектные графы неудобны (YAML/TOML/HCL поддерживают их нативно)
- **Нет валидации схемы** - нет встроенного способа проверить типы или обязательные ключи

---

**VSCode Extension**

```
ext install Fepsid.sein-language-support
```
