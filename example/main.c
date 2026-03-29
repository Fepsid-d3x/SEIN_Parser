#define SEIN_IMPLEMENTATION
#include "../sein.h"
#include <stdio.h>
#include <stdbool.h>

int main(void)
{
    SeinConfig *cfg = sein_alloc();
    if (!cfg) return 1;

    // Synchronous parse //
    sein_parse(cfg, "example.sein", false);

    //  Async parse (optional) //
    // sein_parse(cfg, "example.sein", true);
    // sein_wait(cfg);  // block until background thread finishes // //

    /// strings ///
    const char *name     = sein_get(cfg, "App",      "name",      "unknown");
    const char *fullname = sein_get(cfg, "App",      "full_name", "unknown");
    const char *host     = sein_get(cfg, "Database", "host",      "localhost");

    // system env variable (always reads from OS, never from @set) //
    const char *home_dir = sein_get(cfg, "App", "home_dir", "");

    // int / float //
    int   port    = sein_get_int  (cfg, "Server", "port",    8080);
    float timeout = sein_get_float(cfg, "Server", "timeout", 30.f);

    // bool //
    int debug = sein_get_bool(cfg, "App",        "debug",   0);
    int tls   = sein_get_bool(cfg, "Server.TLS", "enabled", 0);

    // arrays //
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

    // section inheritance //
    // [Player : [Character]] inherits health/level from [Character] //
    const char *player_name   = sein_get      (cfg, "Player", "name",   "none");
    int         player_health = sein_get_int  (cfg, "Player", "health", 0);   // inherited //
    int         player_level  = sein_get_int  (cfg, "Player", "level",  0);   // inherited //
    float       player_speed  = sein_get_float(cfg, "Player", "speed",  0.f); // overridden //

    const char *enemy_name   = sein_get    (cfg, "Enemy", "name",   "none");
    int         enemy_health = sein_get_int(cfg, "Enemy", "health", 0);

    // output //
    printf("App name:      %s\n", name);
    printf("App full name: %s\n", fullname);
    printf("Home dir:      %s\n", home_dir);
    printf("color(RGBA):   r:%.4f g:%.4f b:%.4f a:%.4f\n",
        color_count > 0 ? color_name[0] : 0.f,
        color_count > 1 ? color_name[1] : 0.f,
        color_count > 2 ? color_name[2] : 0.f,
        color_count > 3 ? color_name[3] : 0.f);
    printf("Host:     %s\n",  host);
    printf("Port:     %d\n",  port);
    printf("Timeout:  %.2f\n",timeout);
    printf("Debug:    %s\n",  debug ? "true" : "false");
    printf("TLS:      %s\n",  tls   ? "true" : "false");
    if (tag_count   > 0) printf("Tags[0]:   %s\n",   tags[0]);
    if (port_count  > 0) printf("Ports[0]:  %d\n",   ports[0]);
    if (layer_count > 0) printf("Layers[0]: %d\n",   layers[0]);
    if (lr_count    > 0) printf("Backoff[0]:%.4f\n", lr_arr[0]);

    printf("\nInheritance\n");
    printf("Player: %s  hp=%d  spd=%.1f  lvl=%d\n",
           player_name, player_health, player_speed, player_level);
    printf("Enemy:  %s  hp=%d\n", enemy_name, enemy_health);

    sein_destroy(cfg);
    return 0;
}
