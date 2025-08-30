// libaff.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>

#define GRAD_AFF_VERSION 0.1

// TODO: Handle STL Objects?

#if defined(_WIN32) || defined(__CYGWIN__)
#    define GRAD_AFF_API_IMPORT __declspec(dllimport)
#    define GRAD_AFF_API_EXPORT __declspec(dllexport)
#else
#    define GRAD_AFF_API_EXPORT __attribute__((visibility("default")))
#    define GRAD_AFF_API_IMPORT
#endif

#if defined(GRAD_AFF_EXPORTS)
#    define GRAD_AFF_API GRAD_AFF_API_EXPORT
#    define GRAD_AFF_EXTIMP
#else
#    define GRAD_AFF_API GRAD_AFF_API_IMPORT
#    define GRAD_AFF_EXTIMP extern
#endif
