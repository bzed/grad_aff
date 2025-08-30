cmake_minimum_required(VERSION 3.16)

find_package(Catch2 CONFIG REQUIRED)
find_package(OpenMP)

# Define a function to simplify adding and configuring a test executable.
# This avoids repetition and makes the file easier to read and maintain.
function(add_grad_aff_test test_name source_file)
    add_executable(${test_name} "${source_file}")
    target_link_libraries(${test_name} PRIVATE grad_aff Catch2::Catch2WithMain)

    # Link OpenMP if it was found by CMake.
    if(OpenMP_FOUND)
        target_link_libraries(${test_name} PUBLIC OpenMP::OpenMP_CXX)
    endif()
endfunction()

# Add all test executables by calling the function with the
# target name and the correct corresponding source file.
add_grad_aff_test(grad_aff_tests_wrp wrp_tests.cpp)
add_grad_aff_test(grad_aff_tests_rap rap_tests.cpp)
add_grad_aff_test(grad_aff_tests_paa paa_tests.cpp)
add_grad_aff_test(grad_aff_tests_p3d p3d_tests.cpp)
add_grad_aff_test(grad_aff_tests_pbo pbo_tests.cpp)
