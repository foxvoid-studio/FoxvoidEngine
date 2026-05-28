#version 330

// Input vertex attributes provided by Raylib
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Input uniform matrices
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 lightVP;

// Outputs to the fragment shader
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out vec4 fragLightPos;

void main() {
    // Transform vertex to world space
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    
    // Transform normal to world space using the Normal Matrix
    fragNormal = normalize(mat3(matNormal) * vertexNormal);
    
    // Project the vertex position from the perspective of the directional light
    fragLightPos = lightVP * vec4(fragPosition, 1.0);

    // Calculate final screen-space vertex position
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
