# SEIN_Parser

---
![VS Marketplace](https://img.shields.io/badge/Install%20from-VS%20Marketplace-blue?logo=visualstudiocode) ![C++](https://img.shields.io/badge/C++-17-blue) ![C++](https://img.shields.io/badge/C-99-blue)
 
---

(Используется в FD-SoulEngine)

Очень лёгкий **однофайловый** парсер конфигов в стиле .ini для C++17 и имеется форк под С99 (никто не останется в обиде :3 )
(расширение `.sein`, но работает с любыми текстовыми файлами)

## Поддерживает:
- секции `[НазваниеСекции]`
- пары ключ = значение
- комментарии через `#`
- многострочные значения с `\` в конце строки
- директиву `@include "другой.sein"` (рекурсивно, с ограничением глубины)
- автоматическая обрезка пробелов
- удобные геттеры: строка, int, float, bool, массивы

## Возможности
- Без внешних зависимостей
- Рекурсивное включение файлов (`@include`)
- Многострочные значения через `"R()R"`
- Ссылание на другое значение через `${Section.value}`
- Распознавание bool без учёта регистра (`true/yes/1`, `false/no/0`)
- Безопасные значения по умолчанию
- Разбиение значений в массивы по любому разделителю (по умолчанию `;`)

## Пример файлов (example.sein, colors.sein)
```sein
# example.sein

@include "colors.sein"

[App]
name    = "My Awesome App"
version = 1.4.2
author  = John Doe

debug   = true
verbose = no
logging = 1

description = "R(Это очень длинное
    описание приложения
    которое не влезает в одну строку)R"

tags     = fast; reliable; cross-platform
log_dirs = /var/log/app; /tmp/app; /home/user/.logs

color_name = ${Colors.red}

[Server]
port        = 9000
max_clients = 128
timeout     = 30.5
retry_delay = 0.25

backoff_times = 0.5; 1.0; 2.0; 4.0; 8.0

allowed_ports = 80; 443; 8080; 8443

[Server.TLS]
enabled  = true
cert     = "/etc/ssl/certs/app.crt"
key      = "/etc/ssl/private/app.key"
protocols = TLSv1.2; TLSv1.3

[Database]
host     = localhost
port     = 5432
name     = mydb
user     = admin
password = "p@ssw0rd!with spaces"
pool_min = 2
pool_max = 16

display_name = "Main Production DB"

[Cache]
enabled  = yes
ttl      = 3600
max_size = 512
nodes    = 192.168.1.10; 192.168.1.11; 192.168.1.12

[ML]
learning_rate = 0.001
epochs        = 100
layers        = 3; 64; 128; 64; 10
dropout       = 0.1; 0.2; 0.1

model_path = "/models/v2/weights.bin"
```
``` sein
# colors.sein

[Colors] 
red = 225; 0; 0; 1 
green = 0; 225; 0; 1
```

## Использование в C++
```cpp
#include "sein.hpp"
#include <iostream>

int main(void)
{
    auto cfg = fd::sein::parse_sein("example.sein");

    // string
    auto name = fd::sein::get_value(cfg, "App",    "name",    "unknown");
    auto host = fd::sein::get_value(cfg, "Database","host",   "localhost");

    auto color_name = fd::sein::get_float_array(cfg, "App", "color_name");

    // int
    int   port    = fd::sein::get_int  (cfg, "Server", "port",    8080);
    float timeout = fd::sein::get_float(cfg, "Server", "timeout", 30.f);

    // bool
    bool debug = fd::sein::get_bool(cfg, "App",      "debug",   false);
    bool tls   = fd::sein::get_bool(cfg, "Server.TLS","enabled", false);

    auto tags   = fd::sein::get_array      (cfg, "App",    "tags");
    auto ports  = fd::sein::get_int_array  (cfg, "Server", "allowed_ports");
    auto layers = fd::sein::get_int_array  (cfg, "ML",     "layers");
    auto lr_arr = fd::sein::get_float_array(cfg, "Server", "backoff_times");

    std::cout << "App name: " << name << "\n";
    std::cout << "App color name(RGBA): r:" << color_name[0] << " g:" << color_name[1] << " b:" << color_name[2] << " a:" << color_name[3] << "\n"; 
    std::cout << "Host: " << host << "\n";
    std::cout << "Port: " << port << "\n";
    std::cout << "Timeout: " << timeout << "\n";
    std::cout << "Debug: " << debug << "\n";
    std::cout << "TLS: " << tls << "\n";
    std::cout << "Tags: " << tags[0] << "\n";
    std::cout << "Ports: " << ports[0] << "\n";
    std::cout << "Layers: " << layers[0] << "\n";
    std::cout << "lr arr: " << lr_arr[0] << "\n";

    return 0;
}
```

## Использование в C
```C
#define SEIN_IMPLEMENTATION
#include "../sein.h"
#include <stdio.h>

int main(void)
{
    SeinConfig *cfg = sein_alloc();
    if (!cfg) return 1;
    sein_parse(cfg, "example.sein");

    /// string ///
    const char *name = sein_get(cfg, "App",      "name",    "unknown");
    const char *host = sein_get(cfg, "Database", "host",    "localhost");

    /// int / float ///
    int   port    = sein_get_int  (cfg, "Server", "port",    8080);
    float timeout = sein_get_float(cfg, "Server", "timeout", 30.f);

    /// bool ///
    int debug = sein_get_bool(cfg, "App",       "debug",   0);
    int tls   = sein_get_bool(cfg, "Server.TLS","enabled", 0);

    /// arrays //
    float color_name[4];
    int   color_count = sein_get_float_array(cfg, "App", "color_name", ';', color_name, 4);

    char tags[32][SEIN_MAX_VAL_LEN];
    int  tag_count = sein_get_array(cfg, "App", "tags", ';', tags, 32);

    int ports[32];
    int port_count = sein_get_int_array(cfg, "Server", "allowed_ports", ';', ports, 32);

    int layers[32];
    int layer_count = sein_get_int_array(cfg, "ML", "layers", ';', layers, 32);

    float lr_arr[32];
    int   lr_count = sein_get_float_array(cfg, "Server", "backoff_times", ';', lr_arr, 32);

    printf("App name: %s\n", name);
    printf("App color name(RGBA): r:%.4f g:%.4f b:%.4f a:%.4f\n",
        color_count > 0 ? color_name[0] : 0.f,
        color_count > 1 ? color_name[1] : 0.f,
        color_count > 2 ? color_name[2] : 0.f,
        color_count > 3 ? color_name[3] : 0.f);
    printf("Host:     %s\n",  host);
    printf("Port:     %d\n",  port);
    printf("Timeout:  %.2f\n",timeout);
    printf("Debug:    %s\n",  debug ? "true" : "false");
    printf("TLS:      %s\n",  tls   ? "true" : "false");
    if (tag_count   > 0) printf("Tags:     %s\n",   tags[0]);
    if (port_count  > 0) printf("Ports:    %d\n",   ports[0]);
    if (layer_count > 0) printf("Layers:   %d\n",   layers[0]);
    if (lr_count    > 0) printf("lr arr:   %.4f\n", lr_arr[0]);

    sein_destroy(cfg);
    return 0;
}
```

**VSCode Extension**
install: 
```
ext install Fepsid.sein-language-support
```