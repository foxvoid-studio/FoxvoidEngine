#version 330

// Input vertex attributes (Provided automatically by Raylib)
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Input uniform values (Provided automatically by Raylib)
uniform mat4 mvp;       // Model-View-Projection matrix
uniform mat4 matModel;  // Model (World) matrix
uniform mat4 matNormal; // Normal matrix (Inverse transpose of Model matrix)

// Output vertex attributes (Sent to the fragment shader)
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

void main() {
    // Transform the vertex position from Local Space to World Space
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));

    // Pass texture coordinates and colors directly to the fragment shader
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    // Transform the normal vector to World Space safely
    // (Using matNormal prevents issues if the object is scaled non-uniformly)
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));

    // Calculate the final 2D screen position of the vertex
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
