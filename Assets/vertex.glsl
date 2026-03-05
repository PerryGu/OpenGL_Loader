#version 440 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTextCoord;
layout(location = 3) in vec4 boneIds;
layout(location = 4) in vec4 boneWeights;

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
	
	// Transform normal
	Normal = mat3(transpose(inverse(model))) * aNormal;

	// Final position: apply model/view/projection
	gl_Position = (projection * view * model) * pos;

	TextCoord = aTextCoord;

	
	

}


