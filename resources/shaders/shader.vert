#version 440

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 uv;

out vec4 vColor;
out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec2 texcoord_uv;
noperspective out vec3 vBarycentric;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 stackingAxis;
uniform float overhangAngle;
uniform bool usingOverhangMode;
uniform bool renderingPartObject;

const vec4 kOverhangColor = vec4(1.0, 0.0, 0.0, 1.0);
const float kPi = 3.14159265358979323846;
const float kHalfPi = kPi / 2.0;

vec3 triangleBarycentricCoordinate() {
    int vertexInTriangle = gl_VertexID % 3;

    return vec3(vertexInTriangle == 0 ? 1.0 : 0.0,
                vertexInTriangle == 1 ? 1.0 : 0.0,
                vertexInTriangle == 2 ? 1.0 : 0.0);
}

float anglePastPerpendicularToStackingAxis(float normalAlignment) {
    return acos(clamp(normalAlignment, -1.0, 1.0)) - kHalfPi;
}

void main() {
    vec4 worldPosition = model * vec4(position, 1.0);
    vWorldPosition = worldPosition.xyz;
    vColor = color;
    vWorldNormal = normalize(transpose(inverse(mat3(model))) * normal);
    gl_Position = projection * view * worldPosition;
    texcoord_uv = uv;

    // Emit per-triangle barycentric coordinates for fragment-shader edge detection.
    vBarycentric = triangleBarycentricCoordinate();

    // Overhang mode highlights part faces that point below the stacking axis past the configured angle.
    vec3 normalizedStackingAxis = normalize(stackingAxis);
    float normalAlignment = dot(normalizedStackingAxis, vWorldNormal);
    if (normalAlignment < 0.0 && usingOverhangMode && renderingPartObject) {
        float faceAngle = anglePastPerpendicularToStackingAxis(normalAlignment);

        if (faceAngle > overhangAngle) {
            vColor = kOverhangColor;
        }
    }
}
