#version 100
precision mediump float;

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 lightVP;

varying vec3 fragPosition;
varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragNormal;
varying vec4 fragLightPos;

void main() {
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    
    fragNormal = normalize(mat3(matNormal) * vertexNormal);
    fragLightPos = lightVP * vec4(fragPosition, 1.0);

    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
