#version 450                          
layout (location = 0) in vec3 vPos;  
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUv;
layout (location = 3) in vec3 vTangent;

uniform mat4 _Model;
uniform mat4 _View;
uniform mat4 _Projection;

out vec3 WorldNormal;
out vec3 WorldPosition;
out vec2 uvCoords;
out mat3 TBN;

void main(){    
    WorldPosition = vec3(_Model * vec4(vPos,1));
    WorldNormal = transpose(inverse(mat3(_Model))) * vNormal;
    uvCoords = vUv;
    //Calculating TBN
    vec3 vBiTangent = cross(vNormal, vTangent);
    TBN = mat3(
		vTangent.x, vTangent.y, vTangent.z,
	    vBiTangent.x, vBiTangent.y, vBiTangent.z,
		vNormal.x, vNormal.y, vNormal.z );
    TBN = transpose(inverse(mat3(_Model))) * TBN;
    gl_Position = _Projection * _View * _Model * vec4(vPos,1);
}