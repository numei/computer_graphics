#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;
in vec4 vLightSpacePos;

out vec4 FragColor;

uniform vec3 uViewPos;
uniform vec3 uMatDiffuse;
uniform bool uHasDiffuse;
uniform bool uUseAlphaTest;
uniform float uAlphaCutoff;
uniform sampler2D uDiffuseMap;

uniform vec3 uLightDir;        // direction FROM surface toward light (unit)
uniform vec3 uLightColor;
uniform float uLightIntensity;

uniform sampler2D uShadowMap;

float ShadowCalculation(vec4 lightSpacePos, vec3 normal, vec3 lightDir)
{
    // perspective divide -> NDC
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    // NDC -> [0,1]
    projCoords = projCoords * 0.5 + 0.5;

    // outside shadow map: no shadow
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;

    // better bias: smaller base and normal-dependent
    float bias = max(0.0005 * (1.0 - dot(normal, lightDir)), 0.00005);

    // PCF 3x3
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x,y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return clamp(shadow, 0.0, 1.0);
}

void main()
{
    vec3 baseColor = uMatDiffuse;
    float alpha = 1.0;
    if (uHasDiffuse)
    {
        vec4 t = texture(uDiffuseMap, vUV);
        baseColor = t.rgb;
        alpha = t.a;
    }
    if (uUseAlphaTest && alpha < uAlphaCutoff) discard;

    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir); // we use uLightDir as direction FROM fragment to light
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    // reduce ambient so that shadows & diffuse are visible
    vec3 ambient = 0.06 * baseColor * uLightColor;
    vec3 diffuse = diff * baseColor * uLightColor;
    vec3 specular = spec * vec3(1.0) * uLightColor * 0.5;

    // shadow from depth map (vLightSpacePos must be provided by vertex shader)
    float shadow = ShadowCalculation(vLightSpacePos, N, L);

    vec3 color = ambient + (1.0 - shadow) * (diffuse + specular) * uLightIntensity;

    // simple gamma
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, alpha);
}
