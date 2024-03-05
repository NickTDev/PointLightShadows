#version 450                          
layout (location = 0) in vec3 vPos;  
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUv;

out vec3 WorldNormal;
out vec3 WorldPosition;
out vec2 uvCoords;

void main(){    
    WorldPosition = vPos;
    WorldNormal = vNormal;
    uvCoords = vUv;
    gl_Position = vec4(vPos,1);
}