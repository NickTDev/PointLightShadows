//Code Provide by OpenGL
//https://learnopengl.com/Advanced-Lighting/Shadows/Point-Shadows

#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 _Model;

void main()
{
    gl_Position = _Model * vec4(aPos, 1.0);
} 