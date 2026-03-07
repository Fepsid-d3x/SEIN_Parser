# SEIN_Parser

(Used in FD-SoulEngine)

A very lightweight **single-file** .ini-style config parser for C++17, with a C99 fork available (no one gets left out :3 )
(`.sein` extension, but works with any text file)

## Supports:
- Sections `[SectionName]`
- Key = value pairs
- Comments via `#`
- Multi-line values with `\` at the end of a line
- `@include "other.sein"` directive (recursive, with depth limit)
- Automatic whitespace trimming
- Convenient getters: string, int, float, bool, arrays

## Features
- No external dependencies
- Recursive file inclusion (`@include`)
- Multi-line values via `"R()R"`
- Case-insensitive bool recognition (`true/yes/1`, `false/no/0`)
- Safe default values
- Value splitting into arrays by any delimiter (default `;`)

## Example file (config.sein)
```sein
@include "paths.sein" 
@include "colors.sein"

# Game settings 
[Graphics] 
Resolution = "1920x1080" 
Fullscreen = true
VSync = yes
Gamma = 1.1

[Audio] 
MasterVolume = 0.85 
MusicVolume = 0.4 
EffectsVolume = 0.7

[Players] 
Names = Player; "Enemy"; "Devil"

# Multi-line example 
[Shaders] 
vertex = "R(#version 330 core
		layout(location = 0) in vec3 aPos;
		void main() { gl_Position = vec4(aPos, 1.0); })R"
```

## C++ Usage
```cpp
#include "sein.h"
#include <iostream>

int main(void)
{
    auto config = fd::sein::parse_sein("config.sein");

    std::string res = fd::sein::get_value(config, "Graphics", "Resolution", "1280x720");
    bool fullscreen = fd::sein::get_bool(config, "Graphics", "Fullscreen", false);
    float gamma = fd::sein::get_float(config, "Graphics", "Gamma", 1.0f);

    auto names = fd::sein::get_array(config, "Players", "Names");
    auto vols = fd::sein::get_float_array(config, "Audio", "Volumes", ',');

    std::cout << "Resolution: " << res << "\n";
    std::cout << "Fullscreen: " << fullscreen << "\n";
    std::cout << "First player: " << names[0] << "\n";

    return 0;
}
```

## C Usage
```C
#define SEIN_IMPLEMENTATION
#include "sein.h"
#include <stdio.h>

int main(void)
{
    SeinConfig config;
    sein_parse(&config, "config.sein");

    const char *res = sein_get(&config, "Graphics", "Resolution", "1280x720");
    int fullscreen = sein_get_bool(&config, "Graphics", "Fullscreen", 0);
    float gamma = sein_get_float(&config, "Graphics", "Gamma", 1.0f);

    char names[32][SEIN_MAX_VAL_LEN];
    int name_count = sein_get_array(&config, "Players", "Names", ';', names, 32);

    float vols[32];
    int vol_count = sein_get_float_array(&config, "Audio", "Volumes", ',', vols, 32);

    printf("Resolution: %s\n", res);
    printf("Fullscreen: %s\n", fullscreen ? "true" : "false");
    printf("Gamma: %.2f\n", gamma);

    if (name_count > 0)
        printf("First player: %s\n", names[0]);

    sein_free(&config);
    return 0;
}
```

**VSCode Extension**

Install:
```
ext install Fepsid.sein-language-support
```