#version 330 core

out vec4 FragColor;

uniform vec3 gridColor;

void main()
{
    // Simple fragment shader for grid lines
    // Color is set via uniform
    FragColor = vec4(gridColor, 1.0);
}
