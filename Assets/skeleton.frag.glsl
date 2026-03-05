#version 330 core
out vec4 FragColor;

uniform vec3 color;
uniform bool isPoint; // True for joints (spheres), false for bones (lines)

void main() {
    if (isPoint) {
        // Map gl_PointCoord from [0,1] to [-1,1]
        vec2 coord = gl_PointCoord * 2.0 - 1.0;
        float r2 = dot(coord, coord);
        if (r2 > 1.0) discard; // Circular cutout
        
        // Fake 3D lighting (Sphere Impostor)
        float z = sqrt(1.0 - r2);
        vec3 normal = vec3(coord.x, -coord.y, z); // Invert Y for OpenGL
        vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Simple directional light
        float diff = max(dot(normal, lightDir), 0.0);
        
        // Add ambient and diffuse
        vec3 finalColor = color * (diff * 0.7 + 0.3);
        FragColor = vec4(finalColor, 1.0);
    } else {
        // Regular rendering for bone lines
        FragColor = vec4(color, 1.0);
    }
}
