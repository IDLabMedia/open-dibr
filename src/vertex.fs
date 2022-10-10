// 3D warping

#version 330 core
layout (location = 0) in vec2 aTexCoords;

out vs_out
{
    vec2 TexCoord;
	float angle;
	float inputDepth;
	float outputDepth;
	vec4 worldPosition;
}vertex;

// input camera parameters
uniform float width;
uniform float height;
uniform vec2 near_far;
uniform vec3 inputCameraPos;
uniform float projection_type;
uniform mat4 model;
uniform vec2 hor_range;  // for equirectangular unprojection
uniform vec2 ver_range;  // for equirectangular unprojection
uniform float fov;       // for fisheye equidistant unprojection
uniform vec2 in_f;       // for perspective unprojection
uniform vec2 in_pp;      // for perspective unprojection

// output camera parameters
uniform vec3 outputCameraPos;
uniform mat4 view;
uniform float out_width;
uniform float out_height;
uniform vec2 out_f;
uniform vec2 out_pp;
uniform vec2 out_near_far;
uniform float isVR;
uniform mat4 project;    // only used in VR mode

uniform sampler2D depthTex;


void main()
{
	// the texture coordinate of the current pixel in the input image
	vertex.TexCoord = aTexCoords;

	// get the depth value in [0,1]
	float depth = texture(depthTex, aTexCoords).x;

	// convert to depth in [near, far] (in meters) by scaling with near and far planes
	depth = 1.0 / (1.0f / near_far[1] + depth * ( 1.0f / near_far[0] - 1.0f / near_far[1]));
	depth = min(depth, 1000.0f);
	vertex.inputDepth = depth;

	// unproject to find the worldPosition of the current pixel
	vec4 worldPosition;
	// perspective unprojection
	if(projection_type < 0.2f){
		if(depth > 0){
			float x = (aTexCoords.x * width - in_pp.x) / in_f.x * depth;
			float y = ((1.0f-aTexCoords.y) * height - (in_pp.y + 2.0f * (height * 0.5f - in_pp.y))) / in_f.y * depth;

			vec4 localPosition = vec4(x,y,-depth,1.0f);
			worldPosition = model * localPosition;
		}
		else {
			worldPosition = vec4(0,0,-10,1);
		}
		worldPosition = worldPosition / worldPosition.w;
	}
	// equirectangular unprojection
	else if (projection_type < 0.7f) {
		float phi = hor_range.y - (hor_range.y - hor_range.x) * aTexCoords.x ;
		float theta = ver_range.y - (ver_range.y - ver_range.x) * aTexCoords.y;
		float x = -cos(theta) * sin(phi) * depth;
		float y = sin(theta) * depth;
		float z = -cos(theta) * cos(phi) * depth;

		worldPosition = model * vec4(x, y, z, 1.0f); 
		worldPosition = worldPosition / worldPosition.w;
	}
	// fisheye equidistant unprojection
	else {
		vec2 coords = vec2(2.0f * aTexCoords.x - 1.0f, 2.0f * aTexCoords.y - 1.0f);
		// r in [0,1]
		float r = length(coords);
		// theta in [0, fov/2], phi in [-pi/2 , pi/2]
		float theta = r * fov * 0.5f;
		vec2 coords_norm = r > 0 ? coords / r : vec2(0,0);
		float x = depth * sin(theta) * coords_norm.x;// instead of depth * sin(theta) * sin(phi);
		float y = -depth * sin(theta) * coords_norm.y;// instead of depth * sin(theta) * cos(phi);
		float z = -depth * cos(theta);
 
		if(r < 1.0f){
			worldPosition = model * vec4(x,y,z,1);
			worldPosition = worldPosition / worldPosition.w;
		}
		else {
			worldPosition = vec4(0,0,0,-1);
		}
	}
	vertex.worldPosition = worldPosition;
	
	// project onto the output image
	vec4 viewPosition = view * worldPosition;
	viewPosition = viewPosition / viewPosition.w;
	vertex.outputDepth = length(viewPosition.xyz);

	if(isVR > 0.5f){
		 gl_Position = project * viewPosition;
	}
	else if(viewPosition.z < 0){
		float u = -viewPosition.x / viewPosition.z * out_f.x + out_pp.x;
		float v = -viewPosition.y / viewPosition.z * out_f.y + (out_pp.y + 2.0f * (out_height * 0.5f - out_pp.y));
		float normalised_depth = (-viewPosition.z - out_near_far.x) / (out_near_far.y - out_near_far.x);
		gl_Position = vec4(2.0f * u / out_width - 1.0f, 2.0f * v / out_height - 1.0f, normalised_depth, 1.0f);
	}
	else {
		gl_Position = vec4(0,0,-10,1);
	}

	// give priority to vertices of which the angle 
	// between points (outputCamera, worldPosition, inputCamera) is smaller
	vec3 PO = outputCameraPos - worldPosition.xyz;
	vec3 PI = inputCameraPos - worldPosition.xyz;
	// calculate the cosine of the angle between the points (outputCamera, worldPosition, inputCamera) and add 1
	vertex.angle = acos(dot(PO, PI) / length(PO) / length(PI)) / 3.15f;  // divide by 3.15 to scale to [0, 1)
	//vertex.angle = min(max(vertex.angle, 0), 1);
}