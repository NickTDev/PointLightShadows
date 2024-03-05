#version 450                          
out vec4 FragColor;

in vec3 WorldNormal;
in vec3 WorldPosition;
in vec2 uvCoords;

uniform sampler2D _Texture;
uniform bool _isPost;

void main() {
    if (_isPost) {
        vec4 color = texture(_Texture, uvCoords);
        vec3 greyScale = vec3(.5, .5, .5);
	    FragColor = vec4(vec3(dot(color.rgb, greyScale)), color.a);
    }
    else
        FragColor = texture(_Texture, uvCoords);
}