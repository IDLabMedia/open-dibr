// simple shader for copying textures

#version 330 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 FragAngleAndDepth;

in vec2 TexCoords;

uniform sampler2D previousFBOColorTex;
uniform sampler2D previousFBOAngleAndDepthTex;

void main()
{
	FragColor = texture(previousFBOColorTex, TexCoords);
	FragAngleAndDepth = texture(previousFBOAngleAndDepthTex, TexCoords).rg;
}