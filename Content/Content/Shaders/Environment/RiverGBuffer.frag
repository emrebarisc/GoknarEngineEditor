#version 440 core

layout(location = 0) out vec3 position_GBuffer;
layout(location = 1) out vec4 normal_GBuffer;
layout(location = 2) out vec3 diffuse_GBuffer;
layout(location = 3) out vec4 specularAndPhong_GBuffer;
uniform sampler2D texture2;
uniform vec3 specularReflectance;
uniform float phongExponent;
uniform float translucency;

uniform float deltaTime;
uniform float elapsedTime;

in vec4 fragmentPositionWorldSpace;
in vec3 vertexNormal;
in vec2 textureUV; 
void main()
{
        vec4 texture2Color = texture(texture2, vec2(textureUV.x + elapsedTime * 0.25f, textureUV.y)); 
        diffuse_GBuffer = vec3(texture2Color); 
        specularAndPhong_GBuffer = vec4(specularReflectance, translucency); 
        position_GBuffer = fragmentPositionWorldSpace.xyz;
        normal_GBuffer = vec4(vertexNormal, phongExponent);

}