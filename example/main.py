import sein

cfg = sein.parse_sein("example.sein")

## string ##
name      = sein.get_value(cfg, "App",        "name",    "unknown")
fullname  = sein.get_value(cfg, "App",        "full_name")
host      = sein.get_value(cfg, "Database",   "host",    "localhost")
color_name = sein.get_float_array(cfg, "App", "color_name")

## int / float ##
port    = sein.get_int  (cfg, "Server", "port",    8080)
timeout = sein.get_float(cfg, "Server", "timeout", 30.0)

## bool ##
debug = sein.get_bool(cfg, "App",        "debug",   False)
tls   = sein.get_bool(cfg, "Server.TLS", "enabled", False)

## arrays ##
tags   = sein.get_array      (cfg, "App",    "tags")
ports  = sein.get_int_array  (cfg, "Server", "allowed_ports")
layers = sein.get_int_array  (cfg, "ML",     "layers")
lr_arr = sein.get_float_array(cfg, "Server", "backoff_times")

print(f"App name: {name}")
print(f"App full name: {fullname}")
print(f"App color name(RGBA): r:{color_name[0]} g:{color_name[1]} b:{color_name[2]} a:{color_name[3]}")
print(f"Host: {host}")
print(f"Port: {port}")
print(f"Timeout: {timeout}")
print(f"Debug: {debug}")
print(f"TLS: {tls}")
print(f"Tags: {tags[0]}")
print(f"Ports: {ports[0]}")
print(f"Layers: {layers[0]}")
print(f"lr arr: {lr_arr[0]}")