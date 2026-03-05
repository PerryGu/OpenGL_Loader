#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform vec3 topColor;
uniform vec3 bottomColor;

void main() {
    // Mix from bottomColor (y=0) to topColor (y=1)
    // TexCoords.y goes from 0 (bottom) to 1 (top)
    FragColor = vec4(mix(bottomColor, topColor, TexCoords.y), 1.0);
}
