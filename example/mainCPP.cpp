#include "../sein.hpp"
#include <iostream>

int main(void)
{
    auto cfg = fd::sein::parse_sein("example.sein");

    // string
    auto name = fd::sein::get_value(cfg, "App",    "name",    "unknown");
    auto host = fd::sein::get_value(cfg, "Database","host",   "localhost");

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