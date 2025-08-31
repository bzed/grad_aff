# Top-level CMake project file for grad_aff
#
# Configures the project, dependencies, and build options.

cmake_minimum_required(VERSION 3.16)

project(grad_aff LANGUAGES CXX)

# --- Submodule Integration ---
# Build the lzokay submodule from source.
add_subdirectory(lzokay)

# --- Standard Includes ---
include(CheckIncludeFileCXX)

# --- Build Options ---
option(BUILD_TESTS "Build the project tests" ON)
option(BUILD_WITH_OIIO "Build with OpenImageIO support" ON)
option(BUILD_WITH_OPENSSL "Build with OpenSSL for PBO hash checking" OFF)
option(GRAD_AFF_ENABLE_PARALLELISM "Enable parallel processing features" ON)

# --- Project Configuration ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set a default build type if none is specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# --- Dependencies ---
find_package(pegtl CONFIG REQUIRED)
find_package(tsl-ordered-map CONFIG REQUIRED)
find_package(Boost 1.70.0)

# DXT Compression Library
find_path(SQUISH_INCLUDE_DIR squish.h)
find_library(SQUISH_LIBRARY squish)
if(NOT SQUISH_INCLUDE_DIR OR NOT SQUISH_LIBRARY)
    message(FATAL_ERROR "libsquish not found. Please install it or provide its path.")
endif()
# Create an IMPORTED target for easier linking
add_library(Libsquish::Libsquish UNKNOWN IMPORTED)
set_target_properties(Libsquish::Libsquish PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${SQUISH_INCLUDE_DIR}"
    IMPORTED_LOCATION "${SQUISH_LIBRARY}"
)

# --- Library Target Definition ---
# This must be defined before it is configured in the optional dependency blocks below.
file(GLOB_RECURSE GRAD_AFF_SOURCES "src/*.cpp")
# Build the library as STATIC instead of SHARED.
add_library(grad_aff STATIC ${GRAD_AFF_SOURCES})


# --- Optional Dependencies ---
if(BUILD_WITH_OIIO)
    # Use CMake's find_package in CONFIG mode, which looks for OpenImageIOConfig.cmake
    find_package(OpenImageIO CONFIG REQUIRED)
    if(NOT OpenImageIO_FOUND)
        message(FATAL_ERROR "BUILD_WITH_OIIO was enabled, but OpenImageIO was not found.")
    else()
        # Add the compile definition here to ensure it's set correctly for all dependents.
        target_compile_definitions(grad_aff PUBLIC GRAD_AFF_USE_OIIO)
    endif()
endif()

if(BUILD_WITH_OPENSSL)
    find_package(OpenSSL)
    if(NOT OPENSSL_FOUND)
        message(FATAL_ERROR "BUILD_WITH_OPENSSL was enabled, but OpenSSL was not found.")
    endif()
endif()

# --- Parallelism Configuration ---
if(GRAD_AFF_ENABLE_PARALLELISM)
    check_include_file_cxx("execution" GRAD_AFF_CPP17_PAR)

    if(GRAD_AFF_CPP17_PAR)
        message(STATUS "Using C++17 parallelism")
        add_definitions(-DGRAD_AFF_USE_CPP17_PARALLELISM)
        find_package(TBB) # Check for Intel TBB as a backend
        if(TBB_FOUND)
            message(STATUS "Linking against Intel TBB for C++17 parallelism backend.")
        endif()
    else()
        find_package(OpenMP)
        if(OPENMP_FOUND)
            message(STATUS "Using OpenMP for parallelism")
            add_definitions(-DGRAD_AFF_USE_OPENMP)
        else()
            message(STATUS "Using C++11 threads for parallelism")
            add_definitions(-DGRAD_AFF_USE_CPP11_THREADS)
        endif()
    endif()
endif()


# --- Target Properties & Linking ---
target_compile_definitions(grad_aff
    PUBLIC
        GRAD_AFF_EXPORTS
    PRIVATE
        # The OIIO definition is now handled explicitly above.
        $<$<BOOL:${BUILD_WITH_OPENSSL}>:GRAD_AFF_USE_OPENSSL>
)

target_include_directories(grad_aff
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(grad_aff
    PUBLIC
        lzokay
        taocpp::pegtl
        tsl::ordered_map
        Libsquish::Libsquish
    PRIVATE
        # Boost is header-only in this project, so no library linking is needed.
)

# Link optional dependencies
if(BUILD_WITH_OIIO)
    # Link dependencies as PUBLIC so they are inherited by the CLI executable.
    target_link_libraries(grad_aff PUBLIC OpenImageIO::OpenImageIO OpenImageIO::OpenImageIO_Util)
endif()

if(BUILD_WITH_OPENSSL)
    target_link_libraries(grad_aff PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

# Link parallelism libraries
if(GRAD_AFF_ENABLE_PARALLELISM)
    if(GRAD_AFF_CPP17_PAR AND TBB_FOUND)
        target_link_libraries(grad_aff PUBLIC TBB::tbb)
    elseif(OPENMP_FOUND)
        target_link_libraries(grad_aff PUBLIC OpenMP::OpenMP_CXX)
    endif()
endif()

# Add stdc++fs for filesystem support on older GCC versions
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(grad_aff PUBLIC stdc++fs)
endif()

# --- Build Tests ---
if(BUILD_TESTS)
    add_subdirectory("tests")
endif()

# --- Build CLI ---
add_subdirectory(cli)

# --- Installation Rules ---
# The RUNTIME component is ignored for STATIC libraries, but ARCHIVE and LIBRARY are correct.
install(TARGETS grad_aff
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/grad_aff" DESTINATION "include")
