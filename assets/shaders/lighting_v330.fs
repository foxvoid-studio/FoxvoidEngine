#version 330

// Input vertex attributes (Received from the vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Output fragment color
out vec4 finalColor;

// Optional: Base texture and color provided by Raylib's material system
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Custom engine uniforms
// The position of the camera (needed to calculate specular reflections)
uniform vec3 viewPos; 

// Definition of our Light structure
struct Light {
    int type;           // 0 = Directional, 1 = Point
    vec3 position;      // Used only for Point lights
    vec3 direction;     // Used only for Directional lights
    vec4 color;         // Normalized RGBA color
    float intensity;    // Brightness multiplier
    float radius;       // Used only for Point lights (attenuation distance)
};

// We allow up to 4 lights simultaneously for performance. 
// You can safely increase this limit later if needed.
#define MAX_LIGHTS 4
uniform int lightCount;
uniform Light lights[MAX_LIGHTS];

void main()
{
    // Get the base color of the object (combining texture, vertex color, and material tint)
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 baseColor = (texelColor * fragColor * colDiffuse).rgb;

    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPosition);
    
    // Base ambient light (ensures that areas in complete shadow aren't pitch black)
    vec3 result = vec3(0.1, 0.1, 0.1) * baseColor;

    for (int i = 0; i < lightCount; i++) {
        vec3 lightDir;
        float attenuation = 1.0;

        if (lights[i].type == 0) {
            // Directional Light: The direction is constant for all pixels
            lightDir = normalize(-lights[i].direction);
        } else {
            // Point Light: The direction is from the pixel to the light source
            lightDir = normalize(lights[i].position - fragPosition);
            float distance = length(lights[i].position - fragPosition);
            
            // Simple linear falloff based on the light's radius
            attenuation = clamp(1.0 - (distance / lights[i].radius), 0.0, 1.0);
            
            // Square the attenuation for a more realistic (quadratic) light falloff
            attenuation *= attenuation;
        }

        // Diffuse shading (Lambertian reflectance)
        // Calculates how directly the light hits the surface
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * lights[i].color.rgb * lights[i].intensity * attenuation;

        // Specular shading (Blinn-Phong reflectance)
        // Calculates the shiny highlight reflecting into the camera
        vec3 halfwayDir = normalize(lightDir + viewDir);  
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0); // 32.0 is the shininess factor
        vec3 specular = spec * lights[i].color.rgb * lights[i].intensity * attenuation * 0.3; // 0.3 dampens the shine

        // Add this light's contribution to the total pixel color
        result += (diffuse + specular) * baseColor;
    }

    // Set the final pixel color, preserving the original alpha (transparency)
    finalColor = vec4(result, texelColor.a * fragColor.a * colDiffuse.a);
}
