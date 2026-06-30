#include "doctest.h"
#include "../sein.hpp"

TEST_SUITE("SEIN values") {
    TEST_CASE("different types") {
        auto result = fd::sein::parse_from_string(R"(
            [Test]
            int_val = 42
            float_val = 3.14
            bool_true = true
            bool_yes = yes
            text = Hello World
            array = 1; 2; 3; 5; 8
        )");
        
        auto& cfg = result.data;

        CHECK(fd::sein::get_int(cfg, "Test", "int_val") == 42);
        CHECK(fd::sein::get_float(cfg, "Test", "float_val") == 3.14f);
        CHECK(fd::sein::get_bool(cfg, "Test", "bool_true") == true);
        CHECK(fd::sein::get_bool(cfg, "Test", "bool_yes") == true);
        CHECK(fd::sein::get_value(cfg, "Test", "text") == "Hello World");
    }
}