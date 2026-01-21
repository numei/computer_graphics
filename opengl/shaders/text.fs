#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec3 uColor;
void main(){
    float a = texture(uTex, vUV).r;
    FragColor = vec4(uColor, a);
}
