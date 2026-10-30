#version 410 core

in vec4 vColor;
in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec2 texcoord_uv;
noperspective in vec3 vBarycentric;

out vec4 fColor;

uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform float ambientStrength;
uniform bool usingSolidWireframeMode;
uniform bool preserveColorHue;

const vec3 kFillLightDirection = vec3(-0.350878, 0.250627, 0.902259);
const vec3 kSurfaceColorLift = vec3(0.1);
const vec3 kLuminanceWeights = vec3(0.2126, 0.7152, 0.0722);
const vec3 kWireframeEdgeColor = vec3(0.1);
const float kKeyLightWeight = 0.7;
const float kFillLightWeight = 0.25;
const float kViewFacingWeight = 0.12;
const float kSpecularStrength = 0.25;
const float kSpecularShininess = 48.0;
const float kRimLightStrength = 0.08;
const float kWireframeEdgeWidth = 1.0;
const float kMinimumEdgeWidth = 0.0001;

vec3 surfaceLighting(vec3 baseColor, vec3 worldNormal, vec3 worldPosition) {
    vec3 normal = normalize(worldNormal);
    vec3 lightDir = normalize(lightPos - worldPosition);
    vec3 viewDir = normalize(viewPos - worldPosition);

    vec3 ambient = ambientStrength * lightColor;

    float keyDiff = max(dot(normal, lightDir), 0.0);
    float fillDiff = max(dot(normal, kFillLightDirection), 0.0);
    float facing = max(dot(normal, viewDir), 0.0);
    vec3 diffuse =
        ((kKeyLightWeight * keyDiff) + (kFillLightWeight * fillDiff) + (kViewFacingWeight * facing)) * lightColor;

    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), kSpecularShininess);
    vec3 specular = kSpecularStrength * spec * lightColor;
    vec3 rim = kRimLightStrength * pow(1.0 - facing, 2.0) * lightColor;
    vec3 illumination = ambient + diffuse + specular + rim;
    vec3 litColor = clamp((illumination * baseColor) + kSurfaceColorLift, 0.0, 1.0);

    if (preserveColorHue) {
        // Preserve the original lighting's brightness, including its surface lift.
        // Fit the configured hue into that brightness without clipping individual channels.
        float luminance = clamp(dot(litColor, kLuminanceWeights), 0.0, 1.0);
        float low = min(baseColor.r, min(baseColor.g, baseColor.b));
        float high = max(baseColor.r, max(baseColor.g, baseColor.b));
        float baseChroma = high - low;
        if (baseChroma < 0.00001) { return vec3(luminance); }

        vec3 hue = (baseColor - vec3(low)) / baseChroma;
        float hueLuminance = dot(hue, kLuminanceWeights);
        float brightness = max(max(illumination.r, max(illumination.g, illumination.b)), 0.0);
        float chroma = min(baseChroma * brightness,
                           min(luminance / hueLuminance, (1.0 - luminance) / (1.0 - hueLuminance)));
        return (hue * chroma) + vec3(luminance - (chroma * hueLuminance));
    }

    return litColor;
}

float wireframeEdgeBlend(vec3 barycentric) {
    vec3 edgeWidth = max(fwidth(barycentric) * kWireframeEdgeWidth, vec3(kMinimumEdgeWidth));
    vec3 edgeDistance = smoothstep(vec3(0.0), edgeWidth, barycentric);

    return 1.0 - min(min(edgeDistance.x, edgeDistance.y), edgeDistance.z);
}

void main() {
    vec3 surfaceColor = surfaceLighting(vColor.rgb, vWorldNormal, vWorldPosition);

    // Use derivative-scaled barycentric edges so solid wireframe width is stable in screen space.
    float edgeBlend = wireframeEdgeBlend(vBarycentric) * float(usingSolidWireframeMode);
    vec3 result = mix(surfaceColor, kWireframeEdgeColor, edgeBlend);

    fColor = vec4(result, vColor.a);
}
