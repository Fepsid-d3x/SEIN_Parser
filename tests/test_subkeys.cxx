#include "doctest.h"
#include "../sein.hpp"

TEST_SUITE("subkeys key[subkey]") {
    TEST_CASE("basic subkeys") {
        auto result = fd::sein::parse_from_string(R"(
            [Animation]
            frame["idle"] = 0; 0; 4; 16; 16; true
            frame["run"]  = 0; 16; 6; 16; 16; true
            frame["jump"] = 0; 32; 2; 16; 16; false

            [Objects]
            player["model"] = hero.obj
            player["scale"] = 1.5
        )");

        auto& cfg = result.data;

        CHECK(fd::sein::get_subkey(cfg, "Animation", "frame", "idle") == "0; 0; 4; 16; 16; true");
        CHECK(fd::sein::get_subkey(cfg, "Animation", "frame", "run") == "0; 16; 6; 16; 16; true");

        CHECK(fd::sein::get_value(cfg, "Objects", "player[model]") == "hero.obj");
        CHECK(fd::sein::get_float(cfg, "Objects", "player[scale]") == 1.5f);
    }

    TEST_CASE("get subkeys list") {
        auto result = fd::sein::parse_from_string(R"(
            [Animation]
            frame["idle"] = ...
            frame["run"] = ...
            frame["attack"] = ...
        )");

        auto& cfg = result.data;
        auto subkeys = fd::sein::get_subkeys(cfg, "Animation", "frame");

        CHECK(subkeys.size() == 3);
    }
}