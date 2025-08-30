cmake_minimum_required(VERSION 3.16)

find_package(Catch2 CONFIG REQUIRED)

add_executable(grad_aff_tests
    "grad_aff_tests.cpp"
    "rap_parser_tests.cpp"
    "pbo_tests.cpp"
    "paa_tests.cpp"
    "wrp_tests.cpp"
    "rap_tests.cpp"
)

target_link_libraries(grad_aff_tests PRIVATE
    grad_aff
    Catch2::Catch2WithMain
)
