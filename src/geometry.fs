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

uniform sampler2D previousFBOAngleAndDepthTex;

uniform float isFirstInput;
uniform float depth_diff_threshold_fragment;

uniform float triangle_deletion_factor;
uniform float triangle_deletion_margin; 


void main()
{
    // discard triangles that connect vertices with very different depth values
	float largest_depth_diff = max(abs(vertices[0].inputDepth-vertices[1].inputDepth), max( abs(vertices[0].inputDepth-vertices[2].inputDepth), abs(vertices[1].inputDepth-vertices[2].inputDepth)));
	float largest_depth = max(vertices[0].inputDepth, max(vertices[1].inputDepth, vertices[2].inputDepth));

	float estimated_error = triangle_deletion_factor * (largest_depth - near_far[0]) * (largest_depth - near_far[0]);

	bool condition1 = largest_depth_diff < triangle_deletion_margin * estimated_error + 0.01f;


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
