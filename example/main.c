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
    const char *fullname = sein_get(cfg, "App", "full_name", "unknown");
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
    printf("App full name: %s\n", fullname);
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