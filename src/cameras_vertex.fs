#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view_project;

void main()
{
	gl_Position = view_project * vec4(aPos, 1.0f);
}