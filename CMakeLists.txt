# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
cmake_minimum_required(VERSION 3.16)

project("grad_aff")

option(USE_STATIC_LINKING "Static Build" ON)
option(BUILD_TESTS "Build Tests" ON)
option(BUILD_WITH_OIIO "Build with OpenImageIO" ON)
option(BUILD_WITH_OPENSSL "Build with OpenSSL" ON)

if(USE_STATIC_LINKING)
    set(LIB_TYPE STATIC)
else()
    set(LIB_TYPE SHARED)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(grad_aff ${LIB_TYPE}
    "src/grad_aff.cpp"
    "src/paa/paa.cpp"
    "src/StreamUtil.cpp"
    "src/GridBlockTree.cpp"
    "src/a3lzo.cpp"
    "src/pbo/Pbo.cpp"
    "src/rap/rap.cpp"
    "src/rap/RapParser.cpp"
    "src/wrp/wrp.cpp"
)

target_include_directories(grad_aff PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(BUILD_WITH_OIIO)
    target_compile_definitions(grad_aff PUBLIC GRAD_AFF_USE_OIIO)
endif()

if(BUILD_WITH_OPENSSL)
    target_compile_definitions(grad_aff PUBLIC GRAD_AFF_USE_OPENSSL)
endif()

if(USE_STATIC_LINKING)
    if(WIN32)
        target_compile_definitions(grad_aff PRIVATE OIIO_STATIC_DEFINE)
    endif()
endif()

if(MSVC)
    if(USE_STATIC_LINKING)
        if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
            target_compile_options(grad_aff PRIVATE "/MTd")
        else()
            target_compile_options(grad_aff PRIVATE "/MT")
        endif()
    else()
        if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
            target_compile_options(grad_aff PRIVATE "/MDd")
        else()
            target_compile_options(grad_aff PRIVATE "/MD")
        endif()
    endif()
endif()

# Find dependencies using pkg-config
find_package(PkgConfig REQUIRED)

# LZO
pkg_check_modules(LZO_PKG REQUIRED IMPORTED_TARGET lzo2)
target_link_libraries(grad_aff PRIVATE PkgConfig::LZO_PKG)

# DXT
pkg_check_modules(SQUISH_PKG REQUIRED IMPORTED_TARGET libsquish)
target_link_libraries(grad_aff PRIVATE PkgConfig::SQUISH_PKG)

# Image
if(BUILD_WITH_OIIO)
    find_package(OpenImageIO CONFIG REQUIRED)
    target_link_libraries(grad_aff PRIVATE OpenImageIO::OpenImageIO OpenImageIO::OpenImageIO_Util)

    if(USE_STATIC_LINKING AND UNIX)
        pkg_check_modules(OPENEXR_PKG REQUIRED IMPORTED_TARGET OpenEXR)
        pkg_check_modules(ZLIB_PKG REQUIRED IMPORTED_TARGET zlib)
        pkg_check_modules(TIFF_PKG REQUIRED IMPORTED_TARGET libtiff-4)
        pkg_check_modules(JPEG_PKG REQUIRED IMPORTED_TARGET libjpeg)
        pkg_check_modules(PNG_PKG REQUIRED IMPORTED_TARGET libpng)

        target_link_libraries(grad_aff PRIVATE
            PkgConfig::OPENEXR_PKG
            PkgConfig::ZLIB_PKG
            PkgConfig::TIFF_PKG
            PkgConfig::JPEG_PKG
            PkgConfig::PNG_PKG
        )
    endif()
endif()

# Parsing
find_package(pegtl CONFIG REQUIRED)
target_link_libraries(grad_aff PRIVATE taocpp::pegtl)

# SHA1 PBO
if(BUILD_WITH_OPENSSL)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(grad_aff PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

find_package(Boost 1.70.0 REQUIRED COMPONENTS filesystem)
target_link_libraries(grad_aff PRIVATE Boost::filesystem)

find_package(tsl-ordered-map CONFIG REQUIRED)
target_link_libraries(grad_aff PRIVATE tsl::ordered_map)

if(UNIX)
    target_link_libraries(grad_aff PRIVATE stdc++fs)
endif()

if(BUILD_TESTS)
    add_subdirectory("tests")
endif()
