#pragma once

/**
 * @file version.h
 * @brief Centralized version information for OpenGL_Loader
 * 
 * This file provides a single source of truth for application versioning.
 * All version-related strings and numbers are defined here using preprocessor macros.
 * 
 * Usage:
 * - APP_FULL_NAME: "OpenGL_Loader v2.0.0" (for window titles, UI displays)
 * - APP_VERSION: "v2.0.0" (for version-only displays)
 * - VER_MAJOR, VER_MINOR, VER_PATCH: Individual version components (for migration logic)
 */

#define APP_NAME "OpenGL_Loader"
#define VER_MAJOR 2
#define VER_MINOR 0
#define VER_PATCH 0

// Stringification helper macros (required for proper macro expansion)
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

// Version string macros
#define APP_VERSION "v" STR(VER_MAJOR) "." STR(VER_MINOR) "." STR(VER_PATCH)
#define APP_FULL_NAME APP_NAME " " APP_VERSION
