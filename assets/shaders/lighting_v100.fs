#version 100

precision mediump float;

// Input from vertex shader (Old syntax: varying instead of in)
varying vec3 fragPosition;
varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragNormal;

// texture2D is used instead of texture in GLSL 100
uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3 viewPos;

struct Light {
    int type;
    vec3 position;
    vec3 direction;
    vec4 color;
    float intensity;
    float radius;
};

#define MAX_LIGHTS 4
uniform int lightCount;
uniform Light lights[MAX_LIGHTS];

void main()
{
    vec4 texelColor = texture2D(texture0, fragTexCoord);
    vec3 baseColor = (texelColor * fragColor * colDiffuse).rgb;

    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    
    vec3 result = vec3(0.1, 0.1, 0.1) * baseColor;

    // WebGL 1.0 strict loop requirement: loop conditions MUST use constant expressions.
    // We loop up to MAX_LIGHTS and break dynamically.
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (i >= lightCount) {
            break;
        }

        vec3 lightDir;
        float attenuation = 1.0;

        // In GLSL 100, we must be careful with array indexing, but since 'i' is the loop variable, it is allowed.
        if (lights[i].type == 0) {
            lightDir = normalize(-lights[i].direction);
        } else {
            lightDir = normalize(lights[i].position - fragPosition);
            float distance = length(lights[i].position - fragPosition);
            attenuation = clamp(1.0 - (distance / lights[i].radius), 0.0, 1.0);
            attenuation *= attenuation;
        }

        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * lights[i].color.rgb * lights[i].intensity * attenuation;

        vec3 halfwayDir = normalize(lightDir + viewDir);  
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0); 
        vec3 specular = spec * lights[i].color.rgb * lights[i].intensity * attenuation * 0.3;

        result += (diffuse + specular) * baseColor;
    }

    // GLSL 100 uses the built-in gl_FragColor variable
    gl_FragColor = vec4(result, texelColor.a * fragColor.a * colDiffuse.a);
}
