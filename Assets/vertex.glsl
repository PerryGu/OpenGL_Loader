#version 440 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTextCoord;
layout(location = 3) in vec4 boneIds;
layout(location = 4) in vec4 boneWeights;
layout(location = 5) in vec4 instanceMatrix0;
layout(location = 6) in vec4 instanceMatrix1;
layout(location = 7) in vec4 instanceMatrix2;
layout(location = 8) in vec4 instanceMatrix3;

out vec3 FragPos;
out vec3 Normal;
out vec2 TextCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 bone_transforms[50];


void main()
{
	// Standard bone blending: weighted sum of bone transforms
	mat4 boneTransform = mat4(0.0);
	
	// Only blend bones that have non-zero weights and valid indices
	float totalWeight = 0.0;
	
	if (boneWeights.x > 0.0) {
		int boneIdx = int(boneIds.x);
		if (boneIdx >= 0 && boneIdx < 50) {
			boneTransform += bone_transforms[boneIdx] * boneWeights.x;
			totalWeight += boneWeights.x;
		}
	}
	if (boneWeights.y > 0.0) {
		int boneIdx = int(boneIds.y);
		if (boneIdx >= 0 && boneIdx < 50) {
			boneTransform += bone_transforms[boneIdx] * boneWeights.y;
			totalWeight += boneWeights.y;
		}
	}
	if (boneWeights.z > 0.0) {
		int boneIdx = int(boneIds.z);
		if (boneIdx >= 0 && boneIdx < 50) {
			boneTransform += bone_transforms[boneIdx] * boneWeights.z;
			totalWeight += boneWeights.z;
		}
	}
	if (boneWeights.w > 0.0) {
		int boneIdx = int(boneIds.w);
		if (boneIdx >= 0 && boneIdx < 50) {
			boneTransform += bone_transforms[boneIdx] * boneWeights.w;
			totalWeight += boneWeights.w;
		}
	}
	
	// If no bones affect this vertex (totalWeight == 0), use identity
	if (totalWeight < 0.001) {
		boneTransform = mat4(1.0);
	}
	
	// Apply bone transformation to vertex position (in model space)
	vec4 pos = boneTransform * vec4(aPos, 1.0);
	
	// Construct instance matrix from 4 vec4 columns (if instancing is used)
	mat4 instanceMatrix = mat4(
		instanceMatrix0,
		instanceMatrix1,
		instanceMatrix2,
		instanceMatrix3
	);
	
	// Use instance matrix if it's not identity (instanced rendering), otherwise use uniform model matrix
	// Check if instance matrix is being used by checking if it's not the identity matrix
	// For non-instanced models, instanceMatrix will be identity (1,0,0,0 / 0,1,0,0 / 0,0,1,0 / 0,0,0,1)
	// For instanced models, instanceMatrix will contain the actual transformation
	bool useInstancing = false;
	
	// Check if instance matrix is NOT identity (has non-identity values)
	// Identity matrix has: diagonal = 1.0, all other elements = 0.0
	if (abs(instanceMatrix[0][0] - 1.0) > 0.001 || abs(instanceMatrix[1][1] - 1.0) > 0.001 || 
	    abs(instanceMatrix[2][2] - 1.0) > 0.001 || abs(instanceMatrix[3][3] - 1.0) > 0.001) {
		useInstancing = true;
	} else if (abs(instanceMatrix[0][1]) > 0.001 || abs(instanceMatrix[0][2]) > 0.001 || abs(instanceMatrix[0][3]) > 0.001 ||
	           abs(instanceMatrix[1][0]) > 0.001 || abs(instanceMatrix[1][2]) > 0.001 || abs(instanceMatrix[1][3]) > 0.001 ||
	           abs(instanceMatrix[2][0]) > 0.001 || abs(instanceMatrix[2][1]) > 0.001 || abs(instanceMatrix[2][3]) > 0.001 ||
	           abs(instanceMatrix[3][0]) > 0.001 || abs(instanceMatrix[3][1]) > 0.001 || abs(instanceMatrix[3][2]) > 0.001) {
		useInstancing = true;
	}
	
	// Use instance matrix if instancing is active, otherwise use uniform model matrix
	mat4 finalModel = useInstancing ? instanceMatrix : model;
	
	// Transform normal
	Normal = mat3(transpose(inverse(finalModel))) * aNormal;

	// Final position: apply model/view/projection
	gl_Position = (projection * view * finalModel) * pos;

	TextCoord = aTextCoord;

	
	

}


