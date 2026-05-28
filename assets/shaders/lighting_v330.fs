#version 330

// Inputs from the vertex shader
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
in vec4 fragLightPos;

// Final pixel output
out vec4 finalColor;

// Textures
uniform sampler2D texture0;
uniform sampler2D shadowMap; 

// Material properties and camera position
uniform vec4 colDiffuse;
uniform vec3 viewPos; 
uniform float ambientIntensity;

// Structure representing a dynamic light source
struct Light {
    int type;           // 0 = Directional, 1 = Point
    vec3 position;      
    vec3 direction;     
    vec4 color;         
    float intensity;    
    float radius;       
};

#define MAX_LIGHTS 4
uniform int lightCount;
uniform Light lights[MAX_LIGHTS];

// Calculates the shadow factor (0.0 = lit, 1.0 = fully shadowed)
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDirection) {
    // Perform perspective divide to get Normalized Device Coordinates [-1, 1]
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Map to [0, 1] for texture sampling
    projCoords = projCoords * 0.5 + 0.5;
    
    // Ignore fragments beyond the light's far plane
    if(projCoords.z > 1.0) return 0.0;

    // Raylib FBOs are flipped vertically, we invert the Y axis
    vec2 uv = vec2(projCoords.x, 1.0 - projCoords.y);

    // Apply a slight bias to prevent shadow acne (surface self-shadowing artifacts)
    float bias = max(0.005 * (1.0 - dot(normal, lightDirection)), 0.001);
    
    // Percentage-Closer Filtering (PCF) to soften the shadow edges
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, uv + vec2(x, y) * texelSize).r; 
            shadow += (projCoords.z - bias) > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0; // Average the samples
    
    return shadow;
}

void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 baseColor = (texelColor * fragColor * colDiffuse).rgb;

    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 result = ambientIntensity * baseColor; // Base ambient illumination

    // Evaluate the global shadow map for the primary directional light
    float globalShadow = 0.0;
    if (lightCount > 0 && lights[0].type == 0) {
        globalShadow = ShadowCalculation(fragLightPos, normal, normalize(-lights[0].direction));
    }

    // Accumulate the contribution of each dynamic light
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (i >= lightCount) break;

        vec3 lightDir;
        float attenuation = 1.0;
        float shadowMultiplier = 1.0; 

        if (lights[i].type == 0) {
            // Directional Light
            lightDir = normalize(-lights[i].direction);
            shadowMultiplier = 1.0 - globalShadow; 
        } else {
            // Point Light
            lightDir = normalize(lights[i].position - fragPosition);
            float distance = length(lights[i].position - fragPosition);
            attenuation = clamp(1.0 - (distance / lights[i].radius), 0.0, 1.0);
            attenuation *= attenuation; // Quadratic falloff
        }

        // Diffuse factor
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * lights[i].color.rgb * lights[i].intensity * attenuation;

        // Specular factor (Blinn-Phong)
        vec3 halfwayDir = normalize(lightDir + viewDir);  
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0); 
        vec3 specular = spec * lights[i].color.rgb * lights[i].intensity * attenuation * 0.3;

        // Add the modulated light contribution
        result += (diffuse + specular) * shadowMultiplier * baseColor;
    }

    finalColor = vec4(result, texelColor.a * fragColor.a * colDiffuse.a);
}