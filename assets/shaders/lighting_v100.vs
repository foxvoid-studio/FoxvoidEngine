#version 100

// Declare precision for WebGL compatibility
precision mediump float;

// Input vertex attributes (Old syntax: attribute instead of in)
attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;

// Input uniform matrices
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

// Output to fragment shader (Old syntax: varying instead of out)
varying vec3 fragPosition;
varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragNormal;

void main()
{
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
