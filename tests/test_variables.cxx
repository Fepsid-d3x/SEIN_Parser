#include "doctest.h"
#include "../sein.hpp"

TEST_SUITE("Variables @set and ${}") {
    TEST_CASE("Basic @set and substitution") {
        auto result = fd::sein::parse_from_string(R"(
            @set PROJECT_NAME = "My Game"
            @set VERSION = 1.3.7
            @set MAX_PLAYERS = 32

            [App]
            title = ${PROJECT_NAME} v${VERSION}
            max_players = ${MAX_PLAYERS}
        )");

        CHECK(result.ok);
        auto& cfg = result.data;

        CHECK(fd::sein::get_value(cfg, "App", "title") == "My Game v1.3.7");
        CHECK(fd::sein::get_int(cfg, "App", "max_players") == 32);
    }

    TEST_CASE("Variable in section reference") {
        auto result = fd::sein::parse_from_string(R"(
            @set accent_color = #ffaa00

            [UI]
            button_color = ${accent_color}
        )");

        CHECK(result.ok);
        auto& cfg = result.data;

        std::string color = fd::sein::get_value(cfg, "UI", "button_color");
        CHECK(color == "#ffaa00");
    }
}