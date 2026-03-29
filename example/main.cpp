#include "../sein.hpp"
#include <iostream>

int main(void)
{
    // Synchronous parse //
    auto result = fd::sein::parse_sein("example.sein");
    // result.data is ready immediately for sync parse //
    const auto& cfg = result.data;

    // Async parse (optional) //
    // auto result = fd::sein::parse_sein("example.sein", true //usage_async//) //
    // result.wait();  // block until background thread finishes // //
    // const auto& cfg = result.data; //

    // Strings //
    auto name     = fd::sein::get_value(cfg, "App",      "name",      "unknown");
    auto fullname = fd::sein::get_value(cfg, "App",      "full_name", "unknown");
    auto host     = fd::sein::get_value(cfg, "Database", "host",      "localhost");

    // System env variable (always reads from OS, never from @set) //
    auto home_dir = fd::sein::get_value(cfg, "App", "home_dir", "");

    // int / float //
    int   port    = fd::sein::get_int  (cfg, "Server", "port",    8080);
    float timeout = fd::sein::get_float(cfg, "Server", "timeout", 30.f);

    // bool //
    bool debug = fd::sein::get_bool(cfg, "App",       "debug",   false);
    bool tls   = fd::sein::get_bool(cfg, "Server.TLS","enabled", false);

    // Arrays //
    auto color_name = fd::sein::get_float_array(cfg, "App",    "color_name");
    auto tags       = fd::sein::get_array       (cfg, "App",    "tags");
    auto ports      = fd::sein::get_int_array   (cfg, "Server", "allowed_ports");
    auto layers     = fd::sein::get_int_array   (cfg, "ML",     "layers");
    auto lr_arr     = fd::sein::get_float_array (cfg, "Server", "backoff_times");

    // Section inheritance //
    // [Player : [Character]] inherits health/level from [Character] //
    auto player_name   = fd::sein::get_value(cfg, "Player", "name",   "none");
    auto player_health = fd::sein::get_int  (cfg, "Player", "health", 0);   // inherited //
    auto player_level  = fd::sein::get_int  (cfg, "Player", "level",  0);   // inherited //
    auto player_speed  = fd::sein::get_float(cfg, "Player", "speed",  0.f); // overridden //

    auto enemy_name   = fd::sein::get_value(cfg, "Enemy", "name",   "none");
    auto enemy_health = fd::sein::get_int  (cfg, "Enemy", "health", 0);

    // Output //
    std::cout << "App name:      " << name     << "\n";
    std::cout << "App full name: " << fullname << "\n";
    std::cout << "Home dir:      " << home_dir << "\n";

    if (color_name.size() >= 4)
        std::cout << "color(RGBA):   r:" << color_name[0]
                  << " g:" << color_name[1]
                  << " b:" << color_name[2]
                  << " a:" << color_name[3] << "\n";

    std::cout << "Host:     " << host    << "\n";
    std::cout << "Port:     " << port    << "\n";
    std::cout << "Timeout:  " << timeout << "\n";
    std::cout << "Debug:    " << debug   << "\n";
    std::cout << "TLS:      " << tls     << "\n";

    if (!tags.empty())   std::cout << "Tags[0]:   " << tags[0]   << "\n";
    if (!ports.empty())  std::cout << "Ports[0]:  " << ports[0]  << "\n";
    if (!layers.empty()) std::cout << "Layers[0]: " << layers[0] << "\n";
    if (!lr_arr.empty()) std::cout << "Backoff[0]:" << lr_arr[0] << "\n";

    std::cout << "\n Inheritance \n";
    std::cout << "Player: " << player_name
              << "  hp=" << player_health
              << "  spd=" << player_speed
              << "  lvl=" << player_level << "\n";
    std::cout << "Enemy:  " << enemy_name
              << "  hp=" << enemy_health << "\n";

    return 0;
}
