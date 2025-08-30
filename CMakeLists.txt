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
option(BUILD_WITH_OIIO "Build with OpenImageIO support" OFF)
option(BUILD_WITH_OPENSSL "Build with OpenSSL for PBO hash checking" OFF)
option(GRAD_AFF_ENABLE_PARALLELism "Enable parallel processing features" ON)

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

# --- Optional Dependencies ---
if(BUILD_WITH_OIIO)
    find_package(OpenImageIO CONFIG)
    if(NOT OpenImageIO_FOUND)
        message(FATAL_ERROR "BUILD_WITH_OIIO was enabled, but OpenImageIO was not found.")
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


# --- Library Target Definition ---
file(GLOB_RECURSE GRAD_AFF_SOURCES "src/*.cpp")
add_library(grad_aff SHARED ${GRAD_AFF_SOURCES})

# --- Target Properties & Linking ---
target_compile_definitions(grad_aff
    PUBLIC
        GRAD_AFF_EXPORTS
    PRIVATE
        $<$<BOOL:${BUILD_WITH_OIIO}>:GRAD_AFF_USE_OIIO>
        $<$<BOOL:${BUILD_WITH_OPENSSL}>:GRAD_AFF_USE_OPENSSL>
)

target_include_directories(grad_aff
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(grad_aff
    PUBLIC
        # Corrected: Link against the plain 'lzokay' target from the submodule
        lzokay
        taocpp::pegtl
        tsl::ordered_map
        Libsquish::Libsquish
    PRIVATE
        ${Boost_LIBRARIES}
)

# Link optional dependencies
if(BUILD_WITH_OIIO)
    target_link_libraries(grad_aff PRIVATE OpenImageIO::OpenImageIO OpenImageIO::OpenImageIO_Util)
endif()

if(BUILD_WITH_OPENSSL)
    target_link_libraries(grad_aff PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

# Link parallelism libraries
if(GRAD_AFF_ENABLE_PARALLELISM)
    if(GRAD_AFF_CPP17_PAR AND TBB_FOUND)
        target_link_libraries(grad_aff PRIVATE TBB::tbb)
    elseif(OPENMP_FOUND)
        target_link_libraries(grad_aff PRIVATE OpenMP::OpenMP_CXX)
    endif()
endif()

# Add stdc++fs for filesystem support on older GCC versions
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(grad_aff PRIVATE stdc++fs)
endif()

# --- Build Tests ---
if(BUILD_TESTS)
    add_subdirectory("tests")
endif()

# --- Build CLI ---
add_subdirectory(cli)


# --- Installation Rules ---
install(TARGETS grad_aff
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/grad_aff" DESTINATION "include")
