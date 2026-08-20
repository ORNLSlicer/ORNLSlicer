#version 440

in vec4 vColor;
in vec3 fragPos;
in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 texcoord_uv;

in vec3 bary;
out vec4 fColor;

uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform sampler2D textureSamp;
uniform float ambientStrength;
uniform bool usingSolidWireframeMode;

void main()
{

    // Ambient
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(vWorldNormal);
    vec3 lightDir = normalize(lightPos - fragPos);
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 fillDir = normalize(vec3(-0.35, 0.25, 0.9));
    float keyDiff = max(dot(norm, lightDir), 0.0);
    float fillDiff = max(dot(norm, fillDir), 0.0);
    float facing = max(dot(norm, viewDir), 0.0);
    vec3 diffuse = ((0.7 * keyDiff) + (0.25 * fillDiff) + (0.12 * facing)) * lightColor;


    // Specular
    float specularStrength = 0.25;
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 48);
    vec3 specular = specularStrength * spec * lightColor;
    vec3 rim = 0.08 * pow(1.0 - facing, 2.0) * lightColor;


    float nearD = min(min(bary[0],bary[1]),bary[2]);

    //Dictates how bold/wide the edges of the wireframe are, the more negative
    //the less bold
    float edgeIntensityCoefficient = -25;

    //If we are not using solid wireframe mode, this equation equals 0
    //and no edges are rendered.
    float edgeIntensity =  exp2(edgeIntensityCoefficient*nearD) * float(usingSolidWireframeMode);

    vec3 result = clamp((ambient + diffuse + specular + rim) * vec3(vColor), 0.0, 1.0) * (1.0-edgeIntensity);
    fColor =  vec4(0.1,0.1,0.1,0) + vec4(result, vColor.a);


}
