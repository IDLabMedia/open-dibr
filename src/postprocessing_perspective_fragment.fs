#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform float out_width;
uniform float out_height;
uniform float far;
uniform float debug2;
uniform float debug3;
uniform float use_inpainting;
uniform sampler2D finalColorTex;
uniform sampler2D finalAngleAndDepthTex;
//uniform float depth_diff_threshold_postprocess;
//uniform float circumference_triangle_threshold_geometry;
uniform float circumference_threshold_near;
uniform float circumference_threshold_far;
uniform float isErp;
uniform vec3 backgroundColor;


void main()
{
	float maxDepth = 9990.0f;
	float depth11 = texture(finalAngleAndDepthTex, TexCoords).y;
	float stepX = 1.0f / out_width;
	float stepY = 1.0f / out_height;

	if(depth11 > maxDepth && use_inpainting > 0.5f){	// simple 2-way, depth-based inpainting
		float left = TexCoords.x - stepX;
		float right = TexCoords.x + stepX;
		float bottom = TexCoords.y - stepY;
		float top = TexCoords.y + stepY;
		float leftDepth = 10000.0f;
		float rightDepth = 10000.0f;
		float bottomDepth = 10000.0f;
		float topDepth = 10000.0f;
		bool foundLeft = false;
		bool foundRight = false;
		bool foundBottom = false;
		bool foundTop = false;


		//while(!(foundLeft && foundRight || foundBottom && foundTop)){
		while(!(foundLeft && foundRight)){
			if(!foundLeft){
				leftDepth = texture(finalAngleAndDepthTex, vec2(left, TexCoords.y)).y;
				if((left < 0) || (leftDepth < maxDepth)){
					foundLeft = true;
				}
				else{
					left -= stepX;
				}
			}
			if(!foundRight){
				rightDepth = texture(finalAngleAndDepthTex, vec2(right, TexCoords.y)).y;
				if((right > 1) || (rightDepth < maxDepth)){
					foundRight = true;
				}
				else{
					right += stepX;
				}
			}
			if(!foundBottom){
				bottomDepth = texture(finalAngleAndDepthTex, vec2(TexCoords.x, bottom)).y;
				if((bottom < 0) || (bottomDepth < maxDepth)){
					foundBottom = true;
				}
				else{
					bottom -= stepY;
				}
			}
			if(!foundTop){
				topDepth = texture(finalAngleAndDepthTex, vec2(TexCoords.x, top)).y;
				if((top > 1) || (topDepth < maxDepth)){
					foundTop = true;
				}
				else{
					top += stepY;
				}
			}
		}
		bool horizontalOk = foundLeft && foundRight && (left > 0 || right < 1);
		bool verticalOk = foundBottom && foundTop && (bottom > 0 || top < 1);
		//bool verticalSmallerThanHorizontal = (right - left) * out_width > (top - bottom) * out_height;

		//if(horizontalOk && !(verticalOk && verticalSmallerThanHorizontal)){
		if(horizontalOk){
			vec4 leftColor = texture(finalColorTex, vec2(left, TexCoords.y));
			vec4 rightColor = texture(finalColorTex, vec2(right, TexCoords.y));

			// if the depth is similar, blend the two colors together
			//float threshold = circumference_triangle_threshold_geometry * (1.0f + 3.0f * max(leftDepth, rightDepth) / far);
			float threshold = circumference_threshold_far / 2.0f;
			if(abs(leftDepth - rightDepth) < threshold){
				// weighted blend based on distance (larger distance means smaller weight)
				float leftDist = TexCoords.x - left;
				float rightDist = right - TexCoords.x;
				FragColor = (leftColor * rightDist + rightColor * leftDist) / (leftDist + rightDist);
			}
			else if(right > 1 || (left > 0 && leftDepth > rightDepth)){
				FragColor = leftColor; 
			}
			else {
				FragColor = rightColor;
			}
		}
		else if(verticalOk){
			vec4 bottomColor = texture(finalColorTex, vec2(TexCoords.x, bottom));
			vec4 topColor = texture(finalColorTex, vec2(TexCoords.x, top));

			// if the depth is similar, blend the two colors together
			//float threshold = circumference_triangle_threshold_geometry * (1.0f + 3.0f * max(bottomDepth, topDepth) / far);
			float threshold = circumference_threshold_far / 2.0f;
			if(abs(bottomDepth - topDepth) < threshold){
				// weighted blend based on distance (larger distance means smaller weight)
				float bottomDist = TexCoords.y - bottom;
				float topDist = top - TexCoords.y;
				FragColor = (bottomColor * topDist + topColor * bottomDist) / (bottomDist + topDist);
			}
			else if(top > 1 || (bottom > 0 && bottomDepth > topDepth)){ // use color of point with highest depth
				FragColor = bottomColor;  
			}
			else {
				FragColor = topColor;  
			}
		}
		else {
			discard;
		}
	} // END OF INPAINTING 
	else{
		vec4 color11 = texture(finalColorTex, TexCoords + vec2(0.0f, 0.0f));
		FragColor = color11;

		// ---- a little bit of anti-aliasing ---- //
		float depth01 = texture(finalAngleAndDepthTex, TexCoords + vec2(0.0f, -stepY)).y;
		float depth10 = texture(finalAngleAndDepthTex, TexCoords + vec2(-stepX, 0.0f)).y;
	
		float depth12 = texture(finalAngleAndDepthTex, TexCoords + vec2(stepX, 0.0f)).y;
		float depth21 = texture(finalAngleAndDepthTex, TexCoords + vec2(0.0f, stepY)).y;

		float dist01 = abs(depth01 - depth11);
		float dist10 = abs(depth10 - depth11);
		float dist12 = abs(depth12 - depth11);
		float dist21 = abs(depth21 - depth11);
	
		//float threshold = depth_diff_threshold_postprocess * (1.0f + 3.0f * depth11 / far);
		float threshold = circumference_threshold_near + (depth11 / far) * (circumference_threshold_far - circumference_threshold_near);

		vec4 color01 = texture(finalColorTex, TexCoords + vec2(0.0f, -stepY));
		vec4 color10 = texture(finalColorTex, TexCoords + vec2(-stepX, 0.0f));
		vec4 color12 = texture(finalColorTex, TexCoords + vec2(stepX, 0.0f));
		vec4 color21 = texture(finalColorTex, TexCoords + vec2(0.0f, stepY));
		if(dist01 > threshold || dist21 > threshold){
			FragColor = (color10 + color11 + color12) / 3.0f;
			if(debug2>0.5f){ // press D
				FragColor = vec4(1,0,0,1);
			}
		}
		if(dist10 > threshold || dist12 > threshold){
			FragColor = (color01 + color11 + color21) / 3.0f;
			if(debug2>0.5f){
				FragColor = vec4(1,0,0,1);
			}
		}

		if(debug3 > 0.5f){ // press F
			// disable postprocessing
			FragColor = color11;
		}
	}
}

	// TODO delete
	//float max_dist = max(max(max(dist01, dist10), dist12), dist21);
	//if(max_dist > threshold){
	//	vec4 color00 = texture(previousFBOColorTex, TexCoords + vec2(-stepX, -stepY));
	//	vec4 color01 = texture(previousFBOColorTex, TexCoords + vec2(0.0f, -stepY));
	//	vec4 color02 = texture(previousFBOColorTex, TexCoords + vec2(stepX, -stepY));
	//	vec4 color10 = texture(previousFBOColorTex, TexCoords + vec2(-stepX, 0.0f));
		
	//	vec4 color12 = texture(previousFBOColorTex, TexCoords + vec2(stepX, 0.0f));
	//	vec4 color20 = texture(previousFBOColorTex, TexCoords + vec2(-stepX, stepY));
	//	vec4 color21 = texture(previousFBOColorTex, TexCoords + vec2(0.0f, stepY));
	//	vec4 color22 = texture(previousFBOColorTex, TexCoords + vec2(stepX, stepY));

	//	FragColor = (color00 + color01 + color02 + color10 + color11 + color12 + color20 + color21 + color22) / 9.0f;
		
	//	if(debug2>0.5f){ // press D
			//FragColor = vec4(1,0,0,1);
	//		FragColor = (0.1f*color00 + 0.2f*color01 + 0.1f*color02 + 0.2f*color10 + 1.0f*color11 + 0.2f*color12 + 0.1f*color20 + 0.2f*color21 + 0.1f*color22) / 2.2f;
	//	}
	//}