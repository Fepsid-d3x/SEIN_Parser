import sys
sys.path.insert(0, '..')
import sein

# Synchronous parse #
result = sein.parse_sein("example.sein")
# result.data is ready immediately for sync parse $
cfg = result.data

# Async parse (optional) $
# result = sein.parse_sein("example.sein", usage_async=True)
# result.wait()   # block until background thread finishes # #
# cfg = result.data

# strings #
name      = sein.get_value(cfg, "App",      "name",      "unknown")
fullname  = sein.get_value(cfg, "App",      "full_name", "unknown")
host      = sein.get_value(cfg, "Database", "host",      "localhost")

# system env variable (always reads from OS, never from @set) #
home_dir  = sein.get_value(cfg, "App", "home_dir", "")

# int / float #
port    = sein.get_int  (cfg, "Server", "port",    8080)
timeout = sein.get_float(cfg, "Server", "timeout", 30.0)

# bool #
debug = sein.get_bool(cfg, "App",        "debug",   False)
tls   = sein.get_bool(cfg, "Server.TLS", "enabled", False)

# arrays #
color_name = sein.get_float_array(cfg, "App",    "color_name")
tags       = sein.get_array      (cfg, "App",    "tags")
ports      = sein.get_int_array  (cfg, "Server", "allowed_ports")
layers     = sein.get_int_array  (cfg, "ML",     "layers")
lr_arr     = sein.get_float_array(cfg, "Server", "backoff_times")

# section inheritance #
# [Player : [Character]] inherits health/level from [Character] #
player_name   = sein.get_value(cfg, "Player", "name",   "none")
player_health = sein.get_int  (cfg, "Player", "health", 0)   # inherited #
player_level  = sein.get_int  (cfg, "Player", "level",  0)   # inherited #
player_speed  = sein.get_float(cfg, "Player", "speed",  0.0) # overridden #

enemy_name   = sein.get_value(cfg, "Enemy", "name",   "none")
enemy_health = sein.get_int  (cfg, "Enemy", "health", 0)

# output #
print(f"App name:      {name}")
print(f"App full name: {fullname}")
print(f"Home dir:      {home_dir}")
if len(color_name) >= 4:
    print(f"color(RGBA):   r:{color_name[0]} g:{color_name[1]} b:{color_name[2]} a:{color_name[3]}")
print(f"Host:     {host}")
print(f"Port:     {port}")
print(f"Timeout:  {timeout}")
print(f"Debug:    {debug}")
print(f"TLS:      {tls}")
if tags:   print(f"Tags[0]:   {tags[0]}")
if ports:  print(f"Ports[0]:  {ports[0]}")
if layers: print(f"Layers[0]: {layers[0]}")
if lr_arr: print(f"Backoff[0]:{lr_arr[0]}")

print("\nInheritance")
print(f"Player: {player_name}  hp={player_health}  spd={player_speed}  lvl={player_level}")
print(f"Enemy:  {enemy_name}  hp={enemy_health}")