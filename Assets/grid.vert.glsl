#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;

void main()
{
    // Simple vertex shader for grid lines
    // Grid is rendered at Y=0 (XZ plane)
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
