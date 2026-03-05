#pragma once
#ifndef MATH_3D_H
#define	MATH_3D_H

#include "defines.h"
#include <string.h>
#include <vector>  // For std::vector in GetParentChain

#ifdef WIN32
#include <cmath>
#else
#include <math.h>
#endif
#define _USE_MATH_DEFINES 

#include <assimp/vector3.h>
#include <assimp/matrix3x3.h>
#include <assimp/matrix4x4.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cstring>  // For memcpy in matrix conversion functions


// Legacy macros removed - use MathConstants::toRadians() and MathConstants::toDegrees() instead
// These are kept as inline functions for backward compatibility
inline float ToRadian(float degrees) {
    return toRadians(degrees);
}

inline float ToDegree(float radians) {
    return toDegrees(radians);
}

struct Vector2i
{
	int x;
	int y;
};

struct Vector2f
{
	float x;
	float y;

	Vector2f()
	{
	}

	Vector2f(float _x, float _y)
	{
		x = _x;
		y = _y;
	}
};


struct Vector3f
{
	float x;
	float y;
	float z;

	Vector3f()
	{
	}

	Vector3f(float _x, float _y, float _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}

	Vector3f& operator+=(const Vector3f& r)
	{
		x += r.x;
		y += r.y;
		z += r.z;

		return *this;
	}

	Vector3f& operator-=(const Vector3f& r)
	{
		x -= r.x;
		y -= r.y;
		z -= r.z;

		return *this;
	}

	Vector3f& operator*=(float f)
	{
		x *= f;
		y *= f;
		z *= f;

		return *this;
	}

	Vector3f Cross(const Vector3f& v) const;

	Vector3f& Normalize();

	void Rotate(float Angle, const Vector3f& Axis);

	// Print() method removed for production build (v2.0.0)
	// Use LOG_DEBUG in calling code if debug output is needed
	// void Print() const;
};


struct Vector4f
{
	float x;
	float y;
	float z;
	float w;

	Vector4f()
	{
	}

	Vector4f(float _x, float _y, float _z, float _w)
	{
		x = _x;
		y = _y;
		z = _z;
		w = _w;
	}

	// Print() method removed for production build (v2.0.0)
	// Use LOG_DEBUG in calling code if debug output is needed
	// void Print() const;
};



inline Vector3f operator+(const Vector3f& l, const Vector3f& r)
{
	Vector3f Ret(l.x + r.x,
		l.y + r.y,
		l.z + r.z);

	return Ret;
}

inline Vector3f operator-(const Vector3f& l, const Vector3f& r)
{
	Vector3f Ret(l.x - r.x,
		l.y - r.y,
		l.z - r.z);

	return Ret;
}

inline Vector3f operator*(const Vector3f& l, float f)
{
	Vector3f Ret(l.x * f,
		l.y * f,
		l.z * f);

	return Ret;
}

struct PersProjInfo
{
	float FOV;
	float Width;
	float Height;
	float zNear;
	float zFar;
};

class Matrix4f
{
public:
	float m[4][4];

	Matrix4f()
	{
	}

	// constructor from Assimp matrix
	Matrix4f(const aiMatrix4x4& AssimpMatrix)
	{
		m[0][0] = AssimpMatrix.a1; m[0][1] = AssimpMatrix.a2; m[0][2] = AssimpMatrix.a3; m[0][3] = AssimpMatrix.a4;
		m[1][0] = AssimpMatrix.b1; m[1][1] = AssimpMatrix.b2; m[1][2] = AssimpMatrix.b3; m[1][3] = AssimpMatrix.b4;
		m[2][0] = AssimpMatrix.c1; m[2][1] = AssimpMatrix.c2; m[2][2] = AssimpMatrix.c3; m[2][3] = AssimpMatrix.c4;
		m[3][0] = AssimpMatrix.d1; m[3][1] = AssimpMatrix.d2; m[3][2] = AssimpMatrix.d3; m[3][3] = AssimpMatrix.d4;
	}

	Matrix4f(const aiMatrix3x3& AssimpMatrix)
	{
		m[0][0] = AssimpMatrix.a1; m[0][1] = AssimpMatrix.a2; m[0][2] = AssimpMatrix.a3; m[0][3] = 0.0f;
		m[1][0] = AssimpMatrix.b1; m[1][1] = AssimpMatrix.b2; m[1][2] = AssimpMatrix.b3; m[1][3] = 0.0f;
		m[2][0] = AssimpMatrix.c1; m[2][1] = AssimpMatrix.c2; m[2][2] = AssimpMatrix.c3; m[2][3] = 0.0f;
		m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
	}

	Matrix4f(float a00, float a01, float a02, float a03,
		float a10, float a11, float a12, float a13,
		float a20, float a21, float a22, float a23,
		float a30, float a31, float a32, float a33)
	{
		m[0][0] = a00; m[0][1] = a01; m[0][2] = a02; m[0][3] = a03;
		m[1][0] = a10; m[1][1] = a11; m[1][2] = a12; m[1][3] = a13;
		m[2][0] = a20; m[2][1] = a21; m[2][2] = a22; m[2][3] = a23;
		m[3][0] = a30; m[3][1] = a31; m[3][2] = a32; m[3][3] = a33;
	}

	/**
	 * Set all matrix elements to zero
	 * 
	 * Uses memset for raw performance (faster than std::fill for fixed-size arrays).
	 * This is the "Bible" of the engine's math - performance is critical here.
	 */
	void SetZero()
	{
		memset(m, 0, sizeof(m));
	}

	Matrix4f Transpose() const
	{
		Matrix4f n;

		for (unsigned int i = 0; i < 4; i++) {
			for (unsigned int j = 0; j < 4; j++) {
				n.m[i][j] = m[j][i];
			}
		}

		return n;
	}


	inline void InitIdentity()
	{
		m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
		m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
		m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
		m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
	}

	inline Matrix4f operator*(const Matrix4f& Right) const
	{
		Matrix4f Ret;

		for (unsigned int i = 0; i < 4; i++) {
			for (unsigned int j = 0; j < 4; j++) {
				Ret.m[i][j] = m[i][0] * Right.m[0][j] +
					m[i][1] * Right.m[1][j] +
					m[i][2] * Right.m[2][j] +
					m[i][3] * Right.m[3][j];
			}
		}

		return Ret;
	}

	Vector4f operator*(const Vector4f& v) const
	{
		Vector4f r;

		r.x = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w;
		r.y = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w;
		r.z = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w;
		r.w = m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w;

		return r;
	}

	// Print() method removed for production build (v2.0.0)
	// Use LOG_DEBUG in calling code if debug output is needed
	// void Print() const;


	glm::mat4 toGlmMatrix() {

		glm::mat4  glmMatrix = { m[0][0], m[0][1], m[0][2], m[0][3],
					m[1][0], m[1][1], m[1][2], m[1][3],
					m[2][0], m[2][1], m[2][2], m[2][3],
					m[3][0], m[3][1], m[3][2], m[3][3]
		};


		return glmMatrix;
	}


	float Determinant() const;

	Matrix4f& Inverse();

	void InitScaleTransform(float ScaleX, float ScaleY, float ScaleZ);
	void InitRotateTransform(float RotateX, float RotateY, float RotateZ);
	void InitTranslationTransform(float x, float y, float z);
	void InitCameraTransform(const Vector3f& Target, const Vector3f& Up);
	void InitPersProjTransform(const PersProjInfo& p);



};


struct Quaternion
{
	float x, y, z, w;

	Quaternion(float _x, float _y, float _z, float _w);

	void Normalize();

	Quaternion Conjugate();
};

Quaternion operator*(const Quaternion& l, const Quaternion& r);

Quaternion operator*(const Quaternion& q, const Vector3f& v);

//==================================================================================
// GLM Matrix Conversion Utilities
//==================================================================================
// These functions provide conversion between GLM matrices and float arrays
// Used primarily for interfacing with libraries that use float[16] arrays (e.g., ImGuizmo)

/**
 * Convert GLM 4x4 matrix to float array (column-major format)
 * 
 * GLM matrices are stored in column-major order, which matches the format
 * expected by libraries like ImGuizmo. This function performs a direct memory copy.
 * 
 * @param mat Input GLM matrix (column-major)
 * @param out Output float array [16] (column-major, must be pre-allocated)
 */
inline void glmMat4ToFloat16(const glm::mat4& mat, float* out)
{
    // GLM matrices are column-major, ImGuizmo expects column-major
    // So we can directly copy the memory
    memcpy(out, &mat[0][0], 16 * sizeof(float));
}

/**
 * Convert float array to GLM 4x4 matrix (column-major format)
 * 
 * Converts a float[16] array (column-major) to a GLM matrix.
 * Used when receiving matrix data from libraries like ImGuizmo.
 * 
 * @param in Input float array [16] (column-major)
 * @return GLM 4x4 matrix (column-major)
 */
inline glm::mat4 float16ToGlmMat4(const float* in)
{
    // GLM matrices are column-major, ImGuizmo provides column-major
    // So we can directly construct from the array
    return glm::mat4(
        in[0], in[1], in[2], in[3],
        in[4], in[5], in[6], in[7],
        in[8], in[9], in[10], in[11],
        in[12], in[13], in[14], in[15]
    );
}

//==================================================================================
// Transform Hierarchy and Coordinate Space Conversion
//==================================================================================
// These functions provide robust hierarchy traversal and coordinate space conversion
// using matrix mathematics. They handle inherited transformations (scaling, rotation, etc.)
// correctly through matrix operations.

// Forward declaration for FBX Object (to avoid including ofbx.h in header)
namespace ofbx { struct Object; }

/**
 * Get the complete parent chain of a node, ordered from root down to immediate parent.
 * 
 * This function traverses up the hierarchy starting from the target node,
 * collecting all ancestor nodes in order from the root to the immediate parent.
 * 
 * @param targetNode The node whose parent chain we want to retrieve (const pointer)
 * @return Vector of parent nodes, ordered from root (index 0) to immediate parent (last index)
 *         Returns empty vector if targetNode is nullptr or has no parents
 */
std::vector<ofbx::Object*> GetParentChain(const ofbx::Object* targetNode);

/**
 * Compute the world (absolute) transformation matrix of a node.
 * 
 * Calculates the absolute transformation by multiplying the node's local matrix
 * with its parent's world matrix: World_node = World_parent * Local_node
 * 
 * This function recursively traverses up the parent chain to accumulate all
 * inherited transformations (translation, rotation, scale) from ancestors.
 * 
 * @param node The node whose world matrix to compute (const pointer)
 * @param localMatrix The local transformation matrix of the node
 * @return The world (absolute) transformation matrix
 */
glm::mat4 ComputeWorldMatrix(const ofbx::Object* node, const glm::mat4& localMatrix);

/**
 * Convert a world-space position to local space relative to a parent node.
 * 
 * This function converts a world-space position into the local space of a potential child
 * by using the inverse of the parent's world matrix.
 * 
 * Formula: Position_local = Inverse(World_parent) * Position_world
 * 
 * This correctly handles all inherited transformations (scaling, rotation, translation)
 * from the parent through matrix inversion.
 * 
 * @param worldPos The world-space position to convert (as vec3)
 * @param parentNode The parent node whose local space we want to convert to (const pointer)
 * @return The position in the parent's local space
 */
glm::vec3 WorldToLocal(const glm::vec3& worldPos, const ofbx::Object* parentNode);

/**
 * Convert a world-space transformation matrix to local space relative to a parent node.
 * 
 * This is a more general version that converts an entire transformation matrix
 * (including rotation and scale) from world space to local space.
 * 
 * Formula: Matrix_local = Inverse(World_parent) * Matrix_world
 * 
 * @param worldMatrix The world-space transformation matrix to convert
 * @param parentNode The parent node whose local space we want to convert to (const pointer)
 * @return The transformation matrix in the parent's local space
 */
glm::mat4 WorldToLocalMatrix(const glm::mat4& worldMatrix, const ofbx::Object* parentNode);

#endif	/* MATH_3D_H */
