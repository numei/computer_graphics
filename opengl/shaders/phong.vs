#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;
out vec4 vLightSpacePos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMat;
uniform mat4 uLightVP;

void main() {
    vec4 world = uModel * vec4(aPos,1.0);
    vWorldPos = world.xyz;

    vNormal = normalize(uNormalMat * aNormal);
    vUV = aUV;
    
    vLightSpacePos = uLightVP * world;
    gl_Position = uProj * uView * world;
}
