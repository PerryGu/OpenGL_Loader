#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
//#include <glm/gtc/quaternion.hpp>
//#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/scene.h>

#include "ofbx.h"

typedef unsigned int uint;
typedef unsigned char byte;


inline glm::mat4 assimpToGlmMatrix(aiMatrix4x4 mat) {
	glm::mat4 m;
	for (int y = 0; y < 4; y++)
	{
		for (int x = 0; x < 4; x++)
		{
			m[x][y] = mat[y][x];
		}
	}
	return m;
}


inline glm::vec3 assimpToGlmVec3(aiVector3D vec) {
	return glm::vec3(vec.x, vec.y, vec.z);
}

inline glm::quat assimpToGlmQuat(aiQuaternion quat) {
	glm::quat q;
	q.x = quat.x;
	q.y = quat.y;
	q.z = quat.z;
	q.w = quat.w;

	return q;
}



/**
 * Convert FBX Matrix to GLM matrix format
 * 
 * Converts an openFBX Matrix to GLM mat4 format. The FBX matrix is stored
 * in column-major order, which matches GLM's internal storage.
 * 
 * @param mat Input FBX matrix (column-major)
 * @param inverseMatrix If true, returns the inverse of the converted matrix
 * @return GLM 4x4 matrix (column-major). If inverseMatrix is true, returns inverted matrix.
 * 
 * Matrix layout:
 * - Elements 0, 5, 10: Scale components (diagonal)
 * - Elements 12, 13, 14: Translation components (position)
 */
inline glm::mat4 fbxToGlmMatrix(ofbx::Matrix mat, bool inverseMatrix) {
	glm::mat4 matrix{mat.m[0] , mat.m[1], mat.m[2], mat.m[3],
				mat.m[4], mat.m[5],  mat.m[6],  mat.m[7], // 0, 5, 10 = scale
				mat.m[8], mat.m[9], mat.m[10], mat.m[11], 
				mat.m[12], mat.m[13], mat.m[14], mat.m[15] }; // 12, 13, 14 = pos

	// Fix: Modify the existing matrix variable instead of creating a shadowing local variable
	if (inverseMatrix) {
		matrix = glm::inverse(matrix);
	}

	return matrix;
}

