#version 450                          
out vec4 FragColor;

in vec3 WorldNormal;
in vec3 WorldPosition;
in vec2 uvCoords;
in mat3 TBN;

struct PointLight{
    float radius;
    vec3 position;
    float intensity;
    vec3 color;
    int isOn;
};
struct DirectionLight{
    vec3 direction;
    float intensity;
    vec3 color;
    int isOn;
};
struct SpotLight{
    float radius;
    vec3 direction;
	float intensity;
	vec3 color;
	vec3 position;
	float minAngle;
	float maxAngle;
	int isOn;
};
struct Material{
    vec3 color;
    float ambientK;
	float diffuseK;
	float specularK;
    float shininess;
};

#define MAX_LIGHTS 8
uniform PointLight _PointLights[MAX_LIGHTS];
uniform DirectionLight _DirLight[MAX_LIGHTS];
uniform SpotLight _SpotLight[MAX_LIGHTS];
uniform Material _Material;
uniform vec3 camPos;

uniform sampler2D _FloorTexture;
uniform sampler2D _ObjectTexture;
uniform float _Time;

uniform sampler2D _ObjectNormalMap;
uniform float _NormalIntensity;
uniform bool _UseTexture2;

uniform sampler2D _ShadowMap;
uniform samplerCube _PointShadowMap;
uniform float _MinBias;
uniform float _MaxBias;
uniform float _FarPlane;

float calcShadow(sampler2D shadowMap, vec4 lightSpacePos, float minBias, float maxBias, vec3 normal);
float calcPointShadow(vec3 fragPos, vec3 normal);

void main(){      
    vec3 normal = normalize(WorldNormal);
    vec3 finalLight = vec3(0.0);

    //Calculate normal
    if (!_UseTexture2) {
        normal = texture(_ObjectNormalMap, uvCoords).rgb;
        normal = normal * 2.0 - 1.0;
        normal = TBN * normal;
        normal = normalize(mix(WorldNormal, normal, _NormalIntensity));
    }

    //Point Light
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (_PointLights[i].isOn == 1) {
            float d = distance(_PointLights[i].position, WorldPosition);
            float UEIntensity = clamp((1 - pow(clamp((d / _PointLights[i].radius), 0.0, 1.0), 4)), 0.0, 1.0);
            
            //Ambient Light
            vec3 ambientLight = _Material.ambientK * _PointLights[i].intensity * _PointLights[i].color;
            
            //Diffuse Light
            vec3 directionLight = normalize(_PointLights[i].position - WorldPosition);
            vec3 diffuseLight = _Material.diffuseK * (clamp(dot(directionLight, normal), 0.0f, 100.0f)) * _PointLights[i].intensity * _PointLights[i].color;
            
            //Specular Light (Blinn Phong)
            vec3 directionCamera = normalize(camPos - WorldPosition);
            vec3 halfVector = normalize(directionCamera + directionLight);
            vec3 specularLight = _Material.specularK * pow(dot(normal, halfVector), _Material.shininess) * _PointLights[i].intensity * _PointLights[i].color;
            
            //Final light
            float shadow = calcPointShadow(WorldPosition, normal);

            finalLight += (ambientLight + (diffuseLight + specularLight) * (1.0 - shadow)) * UEIntensity;
        }
    }

    //Directional Light
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (_DirLight[i].isOn == 1) {
            //Ambient Light
            vec3 ambientLight = _Material.ambientK * _DirLight[i].intensity * _DirLight[i].color;
            
            //Diffuse Light
            vec3 directionLight = -normalize(_DirLight[i].direction);
            vec3 diffuseLight = _Material.diffuseK * (clamp(dot(directionLight, normal), 0.0f, 100.0f)) * _DirLight[i].intensity * _DirLight[i].color;
            
            //Specular Light (Blinn Phong)
            vec3 directionCamera = normalize(camPos - WorldPosition);
            vec3 halfVector = normalize(directionCamera + directionLight);
            vec3 specularLight = _Material.specularK * pow(dot(normal, halfVector), _Material.shininess) * _DirLight[i].intensity * _DirLight[i].color;

            //Final light
            finalLight += ambientLight + diffuseLight + specularLight;
        }
    }

    //Spot Light
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (_SpotLight[i].isOn == 1) {
            //Linear Attenuation
            float d = distance(_SpotLight[i].position, WorldPosition);
            float UEIntensity = clamp((1 - pow((d / _SpotLight[i].radius), 4)), 0.0, 1.0);
            
            //Angular Attenuation
            vec3 newDir = normalize(_SpotLight[i].direction);
            vec3 dirToFrag = normalize((WorldPosition - _SpotLight[i].position));
            float angle = dot(newDir, dirToFrag);
            float maxAng = cos(radians(_SpotLight[i].maxAngle));
            float minAng = cos(radians(_SpotLight[i].minAngle));
            float AngIntensity = clamp(((angle - maxAng) / (minAng - maxAng)), 0.0, 1.0);
        
            //Ambient Light
            vec3 ambientLight = _Material.ambientK * _SpotLight[i].intensity * _SpotLight[i].color;
        
            //Diffuse Light
            vec3 directionLight = normalize(_SpotLight[i].position - WorldPosition);
            vec3 diffuseLight = _Material.diffuseK * (clamp(dot(directionLight, normal), 0.0f, 100.0f)) * _SpotLight[i].intensity * _SpotLight[i].color;
        
            //Specular Light (Blinn Phong)
            vec3 directionCamera = normalize(camPos - WorldPosition);
            vec3 halfVector = normalize(directionCamera + directionLight);
            vec3 specularLight = _Material.specularK * pow(dot(normal, halfVector), _Material.shininess) * _SpotLight[i].intensity * _SpotLight[i].color;
        
            //Final light
            finalLight += (ambientLight + diffuseLight + specularLight) * AngIntensity * UEIntensity;
        }
    }

    //Multiply final light by material color
    //finalLight *= _Material.color;

    if (_UseTexture2)
        FragColor = texture(_FloorTexture, uvCoords) * vec4(finalLight, 1.0);
    else
        FragColor = texture(_ObjectTexture, uvCoords) * vec4(finalLight, 1.0);
}

float calcShadow(sampler2D shadowMap, vec4 lightSpacePos, float minBias, float maxBias, vec3 normal) {
    //With Percentage Closer Filtering
    vec3 sampleCoord = lightSpacePos.xyz / lightSpacePos.w;
    sampleCoord = sampleCoord * 0.5 + 0.5;
    
    float bias = max(maxBias * (1.0 - dot(normal, (lightSpacePos.xyz - WorldPosition))), minBias);
    float myDepth = sampleCoord.z - bias;

    float totalShadow = 0;
    vec2 texelOffset = 1.0 / textureSize(shadowMap, 0);

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 uv = sampleCoord.xy + vec2(x * texelOffset.x, y * texelOffset.y);
            totalShadow += step(texture(_ShadowMap, uv).r, myDepth);
        }
    }

    return totalShadow / 9.0f;
}

float calcPointShadow(vec3 fragPos, vec3 normal) {
    float shadow = 0.0;
    float bias   = max(_MaxBias * (1.0 - dot(normal, WorldPosition)), _MinBias);
    int samples  = 20;
    float viewDistance = length(camPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / _FarPlane)) / 25.0;  

    //PCF
    vec3 sampleOffsetDirections[20] = vec3[]
    (
       vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
       vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
       vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
       vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
       vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
    ); 

    vec3 fragToLight = fragPos - _PointLights[0].position; 
    float currentDepth = length(fragToLight);  

    for(int i = 0; i < samples; ++i)
    {
        float closestDepth = texture(_PointShadowMap, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
        closestDepth *= _FarPlane;   // undo mapping [0;1]
        if(currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);  

    return shadow;
}