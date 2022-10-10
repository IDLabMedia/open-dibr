// This geometry shader is necessary to delete elongated triangles, i.e. triangles that connect foreground to background objects

#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;


in vs_out
{
    vec2 TexCoord;
	float angle;
	float inputDepth;
	float outputDepth;
	vec4 worldPosition;
}vertices[];

out fs_in
{
    vec2 TexCoord;
	float angle;
	float outputDepth;
}frag;

uniform vec2 near_far;
uniform float max_triangle_size;

uniform sampler2D previousFBOAngleAndDepthTex;

uniform float isFirstInput;
uniform float depth_diff_threshold_fragment;


void main()
{
    // discard triangles that connect vertices with very different depth values
	float circumference_triangle = length(vertices[0].worldPosition-vertices[1].worldPosition) + length(vertices[0].worldPosition-vertices[2].worldPosition) + length(vertices[1].worldPosition-vertices[2].worldPosition);
	float smallest_depth = min(vertices[0].inputDepth, min(vertices[1].inputDepth, vertices[2].inputDepth));
	// the larger the depth, the more inaccurate the depth map is, so the threshold "max_circumference_triangle"
	// is loosened up as smallest_depth approaches the far plane
	bool condition1 = circumference_triangle < max_triangle_size + (smallest_depth / near_far[1]) * max_triangle_size * 10.0f;

	// discard triangles that fall outside the fisheye image of the input camera (if relevant)
	bool condition2 = !(vertices[0].worldPosition.w < 0 || vertices[1].worldPosition.w < 0 || vertices[2].worldPosition.w < 0);

	// pass the triangle to the fragment shader iff all conditions are true
	if(condition1 && condition2){ 
		for(int i = 0; i < 3; i++){
			frag.TexCoord = vertices[i].TexCoord;
			frag.angle = vertices[i].angle;
			frag.outputDepth = vertices[i].outputDepth;
			gl_Position = gl_in[i].gl_Position;
			EmitVertex();
		}
		EndPrimitive();
	}
}
