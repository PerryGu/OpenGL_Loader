#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;
uniform float pointSize; // Point size for sphere impostor joints (linked to skeleton line width setting)

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    gl_PointSize = pointSize; // Dynamic point size based on skeleton line width setting
}
