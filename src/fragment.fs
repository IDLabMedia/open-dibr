// blending of current output image with the previous (if !isFirstInput)

#version 330 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 FragAngleAndDepth;

in fs_in
{
    vec2 TexCoord;
	float angle;
	float outputDepth;
}frag;

uniform float width;
uniform float height;
uniform float out_width;
uniform float out_height;
uniform float chroma_offset;

uniform float blendingThreshold;
uniform float image_border_threshold_fragment;
uniform float depth_diff_threshold_fragment;

uniform sampler2D colorTex;
uniform sampler2D previousFBOColorTex;
uniform sampler2D previousFBOAngleAndDepthTex;

uniform float isFirstInput;
uniform float isYCbCr;
uniform float convertYCbCrToRGB;


void main()
{
	// SAMPLE colorTex
	// -------------------------------------
	if(isYCbCr > 0.5f){
		// color tex is YUV NV12
		vec2 texcoord_Y = vec2(frag.TexCoord.x, frag.TexCoord.y * height/(height*1.5f + chroma_offset));
		float Cb_x = (floor(floor(frag.TexCoord.x * width) / 2.0f) * 2.0f + 0.5f) / width;
		float Cr_x = (floor(floor(frag.TexCoord.x * width) / 2.0f) * 2.0f + 1.5f) / width;
		float Cb_Cr_y = (floor(floor(frag.TexCoord.y * height) / 2.0f) + 0.5f + height + chroma_offset) / (height * 1.5f + chroma_offset);
	
		float Y = texture(colorTex, texcoord_Y).r;
		float Cb = texture(colorTex, vec2(Cb_x, Cb_Cr_y)).r;
		float Cr = texture(colorTex, vec2(Cr_x, Cb_Cr_y)).r;

		FragColor = vec4(Y, Cb, Cr, 1);

		if(convertYCbCrToRGB > 0.5f){
			// CONVERT TO RGB
			// --------------
			float r = Y + 1.370705*(Cr - 128.0f / 255.0f);
			float g = Y - 0.698001 *(Cr - 128.0f / 255.0f) - 0.337633*(Cb - 128.0f / 255.0f);
			float b = Y + 1.732446*(Cb - 128.0f / 255.0f);
			FragColor = vec4(r, g, b, 1);
		}
	}
	else {
		// color tex is RGB
		FragColor = texture(colorTex, frag.TexCoord);
	}
	
	FragAngleAndDepth = vec2(frag.angle, frag.outputDepth);


	// LET THE FragAngle INCREASE WHEN CLOSE TO THE BORDER OF THE INPUT IMAGE
	float col = frag.TexCoord.x * width;
	float row = frag.TexCoord.y * height;

	float distance_to_image_border = min(min(min(col, width-col), row), height-row);
	if(distance_to_image_border < image_border_threshold_fragment){
		FragAngleAndDepth.x = FragAngleAndDepth.x + 4.0f * blendingThreshold * (1.0f-distance_to_image_border / image_border_threshold_fragment);
	}

	if(isFirstInput < 0.5f){
		// BLENDING 
		vec2 TexCoordPreviousTex = vec2(gl_FragCoord.x / out_width, gl_FragCoord.y / out_height);
		vec4 previous_color = texture(previousFBOColorTex, TexCoordPreviousTex);
		vec2 previous_angle_and_depth = texture(previousFBOAngleAndDepthTex, TexCoordPreviousTex).xy;
		float blendfactor = 0.0f; // weight of the current input image
		
		// IF DEPTH WAY LARGER THAN THAT OF PREVIOUS INPUT, DISCARD
		float current_depth = frag.outputDepth;
		float previous_depth = previous_angle_and_depth.y;
		if(current_depth > previous_depth + 0.05f){
			discard;
		}
		// IF DEPTH WAY SMALLER THAN THAT OF PREVIOUS INPUT, OVERWRITE
		// ELSE, BLEND BASED ON VIEWING ANGLE
		else if(previous_depth < current_depth + depth_diff_threshold_fragment){
			// BLEND WITH PREVIOUS INPUT IMAGE BASED ON VIEWING ANGLE
			float current_angle = frag.angle;
			float previous_angle = previous_angle_and_depth.x; // can be slightly off due to floating point errors
			// smallest angle is better
			float difference = previous_angle - current_angle;
			blendfactor = difference / (2.0f * blendingThreshold) + 0.5f;
			blendfactor = max(min(blendfactor, 1.0f), 0.0f);
			FragColor = blendfactor * FragColor + (1.0f - blendfactor) * previous_color;
		
			FragAngleAndDepth = blendfactor * FragAngleAndDepth + (1.0f - blendfactor) * previous_angle_and_depth;
		}
		else {
			// overwriting
			blendfactor = 1.0f;
		}
	}
}