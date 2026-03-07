# SEIN_Parser

(Used in FD-SoulEngine)

**Very lightweight single-file .ini-style config parser for C++17**
(file extension .sein, but works with any text files)

**Supported features:**

- sections `[SectionName]`
- key = value pairs
- comments via `#`
- multi-line values using `\` at the end of line
- `@include "another.sein"` directive (recursive, with depth limit)
- automatic trimming of whitespaces
- convenient getters: string, int, float, bool, arrays

**Highlights**

- Zero external dependencies
- Recursive file inclusion (`@include`)
- Multi-line values with `\` continuation
- Case-insensitive boolean recognition (`true/yes/1`, `false/no/0`)
- Safe default values
- Array splitting with any custom delimiter (default `;`)

**Example file (config.sein)**

```sein
@include "paths.sein" 
@include "colors.sein"

# Game settings 
[Graphics] 
Resolution = 1920x1080 
Fullscreen = true
VSync = yes
Gamma = 1.1

[Audio] 
MasterVolume = 0.85 
MusicVolume = 0.4 
EffectsVolume = 0.7

[Players] 
Names = Player;Enemy;Devil

# Multi-line example 
[Shaders] 
vertex = #version 330 core \ 
		layout(location = 0) in vec3 aPos; \ 
		void main() { gl_Position = vec4(aPos, 1.0); }
```

**Usage**

```cpp
#include "sein.h"
#include <iostream>

int main(void)
{
    auto config = fd::sein::parse_sein("config.sein");

    std::string res = fd::sein::get_value(config, "Graphics", "Resolution", "1280x720");
    bool fullscreen = fd::sein::get_bool(config, "Graphics", "Fullscreen", false);
    float gamma = fd::sein::get_float(config, "Graphics", "Gamma", 1.0f);

    // Arrays
    auto names = fd::sein::get_array(config, "Players", "Names");
    auto vols = fd::sein::get_float_array(config, "Audio", "Volumes", ',');

    std::cout << "Resolution: " << res << "\n";
    std::cout << "Fullscreen: " << fullscreen << "\n";
    std::cout << "First player: " << names[0] << "\n";

    return 0;
}