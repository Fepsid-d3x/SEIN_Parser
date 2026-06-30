#include "doctest.h"
#include "../sein.hpp"
#include <fstream>

TEST_SUITE("include") {
    TEST_CASE("Include with variables") {
        auto result = fd::sein::parse_from_string(R"(
            @include "tests/colors.sein"

            [Interface]
            background = ${BG}
            accent = ${ACCENT}
        )");

        CHECK(result.ok);
        auto& cfg = result.data;

        CHECK(fd::sein::get_value(cfg, "Interface", "background") == "#1e1e2e");
        CHECK(fd::sein::get_value(cfg, "Interface", "accent") == "#89b4faff");
    }
}