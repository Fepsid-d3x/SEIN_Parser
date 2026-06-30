#include "doctest.h"
#include "../sein.hpp"

TEST_SUITE("SEIN basic") {
    TEST_CASE("load from string") {
        auto result = fd::sein::parse_from_string(R"(
            [App]
            name = "My Game"
            version = 1.2.3
        )");
        
        CHECK(result.ok);
        auto& cfg = result.data;

        CHECK(fd::sein::get_value(cfg, "App", "name") == "My Game");
        CHECK(fd::sein::get_int(cfg, "App", "version") == 1);
    }
}