#include "doctest.h"
#include "../sein.hpp"

TEST_SUITE("section inheritance") {
    TEST_CASE("basic inheritance") {
        auto result = fd::sein::parse_from_string(R"(
            [Character]
            health = 100
            speed = 5.0

            [Player : [Character]]
            name = Hero
            speed = 8.5

            [Enemy : [Character]]
            name = Goblin
            health = 40
        )");

        auto& cfg = result.data;

        CHECK(fd::sein::get_int(cfg, "Player", "health") == 100);
        CHECK(fd::sein::get_float(cfg, "Player", "speed") == 8.5f);
        CHECK(fd::sein::get_int(cfg, "Enemy", "health") == 40);
    }
}