#version 100
precision mediump float;

varying vec3 fragPosition;
varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragNormal;
varying vec4 fragLightPos;

uniform sampler2D texture0;
uniform sampler2D shadowMap;

uniform vec4 colDiffuse;
uniform vec3 viewPos; 
uniform float ambientIntensity;

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

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDirection) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if(projCoords.z > 1.0) return 0.0;

    // Raylib FBOs are flipped vertically, we invert the Y axis
    vec2 uv = vec2(projCoords.x, 1.0 - projCoords.y);

    // Hard shadow sampling for WebGL 1.0 performance
    float closestDepth = texture2D(shadowMap, uv).r; 
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDirection)), 0.001);
    
    return (currentDepth - bias) > closestDepth ? 1.0 : 0.0;
}

void main() {
    vec4 texelColor = texture2D(texture0, fragTexCoord);
    vec3 baseColor = (texelColor * fragColor * colDiffuse).rgb;

    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 result = ambientIntensity * baseColor;

    float globalShadow = 0.0;
    if (lightCount > 0 && lights[0].type == 0) {
        globalShadow = ShadowCalculation(fragLightPos, normal, normalize(-lights[0].direction));
    }

    // WebGL 1.0 loop unrolling requirement
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (i >= lightCount) break;

        vec3 lightDir;
        float attenuation = 1.0;
        float shadowMultiplier = 1.0; 

        if (lights[i].type == 0) {
            lightDir = normalize(-lights[i].direction);
            shadowMultiplier = 1.0 - globalShadow; 
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

        result += (diffuse + specular) * shadowMultiplier * baseColor;
    }

    gl_FragColor = vec4(result, texelColor.a * fragColor.a * colDiffuse.a);
}
