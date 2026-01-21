#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uOrtho;
out vec2 vUV;
void main(){
    vUV = aUV;
    gl_Position = uOrtho * vec4(aPos.xy, 0.0, 1.0);
}
