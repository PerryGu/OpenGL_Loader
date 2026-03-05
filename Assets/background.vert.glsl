#version 330 core
layout (location = 0) in vec2 aPos;

out vec2 TexCoords;

void main() {
    // Convert NDC to 0-1 range for gradient calculation
    TexCoords = aPos * 0.5 + 0.5;
    // Put it at the very back of the depth range (0.999 is near the far plane)
    gl_Position = vec4(aPos, 0.999, 1.0);
}
