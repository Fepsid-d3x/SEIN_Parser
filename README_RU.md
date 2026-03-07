# SEIN_Parser

(Используется в FD-SoulEngine)

Очень лёгкий **однофайловый** парсер конфигов в стиле .ini для C++17  
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
- Многострочные значения через `\`
- Распознавание bool без учёта регистра (`true/yes/1`, `false/no/0`)
- Безопасные значения по умолчанию
- Разбиение значений в массивы по любому разделителю (по умолчанию `;`)

## Пример файла (config.sein)
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

## Использование
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
```

**VSCode Extension**
install: 
```
ext install Fepsid.sein-language-support
```