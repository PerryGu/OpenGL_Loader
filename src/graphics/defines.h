/**
 * @file defines.h
 * @brief Centralized math constants for the 3D renderer engine
 * 
 * This file provides a centralized location for mathematical constants to ensure
 * consistency across the entire renderer. All constants use modern C++20 constexpr
 * for compile-time evaluation and type safety.
 * 
 * The constants are organized in the MathConstants namespace to avoid global namespace
 * pollution and provide clear scoping. Both double-precision (for high-precision calculations)
 * and float-precision (for GPU-friendly values) versions are provided.
 */

#pragma once

namespace MathConstants {
    // High-precision double constants (for CPU calculations)
    constexpr double PI = 3.14159265358979323846;
    constexpr double TWO_PI = 6.28318530717958647692;
    
    // GPU-friendly float constants (for shader uploads and GPU calculations)
    constexpr float PI_F = 3.1415927f;
    constexpr float TWO_PI_F = 6.2831853f;
    
    // Conversion constants (pre-calculated for performance)
    constexpr double DEG_TO_RAD = 0.017453292519943295;  // PI / 180.0
    constexpr double RAD_TO_DEG = 57.29577951308232;      // 180.0 / PI
    constexpr float DEG_TO_RAD_F = 0.0174532925f;
    constexpr float RAD_TO_DEG_F = 57.2957795f;
}

/**
 * Convert degrees to radians (type-safe template function)
 * 
 * This function replaces the old TO_RADIANS macro with a type-safe,
 * debuggable inline constexpr function. Works with any numeric type.
 * 
 * @tparam T Numeric type (float, double, int, etc.)
 * @param degrees Angle in degrees
 * @return Angle in radians
 */
template <typename T>
inline constexpr T toRadians(T degrees) {
    return degrees * static_cast<T>(MathConstants::DEG_TO_RAD);
}

/**
 * Convert radians to degrees (type-safe template function)
 * 
 * This function replaces the old TO_DEGREES macro with a type-safe,
 * debuggable inline constexpr function. Works with any numeric type.
 * 
 * @tparam T Numeric type (float, double, int, etc.)
 * @param radians Angle in radians
 * @return Angle in degrees
 */
template <typename T>
inline constexpr T toDegrees(T radians) {
    return radians * static_cast<T>(MathConstants::RAD_TO_DEG);
}
