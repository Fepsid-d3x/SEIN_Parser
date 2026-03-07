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