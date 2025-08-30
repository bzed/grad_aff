#define CATCH_CONFIG_MAIN

#include <catch2/catch_all.hpp>

#include "grad_aff/grad_aff.h"

TEST_CASE("version", "[version]") {
    REQUIRE(GRAD_AFF_VERSION > 0);
}