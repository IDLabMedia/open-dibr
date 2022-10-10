// simple shader for copying 1 texture

#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D previousFBOColorTex;

void main()
{
	FragColor = texture(previousFBOColorTex, TexCoords);
}