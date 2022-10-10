#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform float out_width;
uniform float out_height;
uniform float far;
uniform float debug2;
uniform float debug3;
uniform float use_inpainting;
//uniform float circumference_triangle_threshold_geometry;
uniform float circumference_threshold_near;
uniform float circumference_threshold_far;
uniform sampler2D finalColorTex;
uniform sampler2D finalAngleAndDepthTex;
//uniform float depth_diff_threshold_postprocess;
uniform sampler2D ERP2CassiniTex;
uniform sampler2D Cassini2ERPTex;
uniform vec3 backgroundColor;

void main()
{
	float maxDepth = 9990.0f;
	float depth11 = texture(finalAngleAndDepthTex, TexCoords).y;

	//  translate ERP coordinates to cassini coordinates
	vec2 cassiniTexCoords = texture(ERP2CassiniTex, TexCoords).xy;
	float stepX = 1.0f / out_width;
	float stepY = 1.0f / out_height;

	if(depth11 > maxDepth && use_inpainting > 0.5f && !(cassiniTexCoords.x < stepX && cassiniTexCoords.y < stepY)){
		float left = cassiniTexCoords.x - stepX;
		float right = cassiniTexCoords.x + stepX;
		float bottom = cassiniTexCoords.y - stepY;
		float top = cassiniTexCoords.y + stepY;
		vec2 leftTexCoords = vec2(0,0);
		vec2 rightTexCoords = vec2(0,0);
		vec2 bottomTexCoords = vec2(0,0);
		vec2 topTexCoords = vec2(0,0);
		float leftDepth = 10000.0f;
		float rightDepth = 10000.0f;
		float bottomDepth = 10000.0f;
		float topDepth = 10000.0f;
		bool foundLeft = false;
		bool foundRight = false;
		bool foundBottom = false;
		bool foundTop = false;
		bool leftIsInvalid = false;
		bool rightIsInvalid = false;
		bool bottomIsInvalid = false;
		bool topIsInvalid = false;

		// TODO delete this limit!!
		while(!(foundLeft && foundRight || foundBottom && foundTop)){
			if(!foundLeft){
				leftTexCoords = texture(Cassini2ERPTex, vec2(left, cassiniTexCoords.y)).xy;
				leftDepth = texture(finalAngleAndDepthTex, leftTexCoords).y;
				// leftTexCoords is not valid if equal to (0, 0)
				leftIsInvalid = leftTexCoords.x < stepX && leftTexCoords.y < stepY;
				if((left < 0) || ((!leftIsInvalid) && (leftDepth < maxDepth))){
					foundLeft = true;
				}
				else{
					left -= stepX;
				}
			}
			if(!foundRight){
				rightTexCoords = texture(Cassini2ERPTex, vec2(right, cassiniTexCoords.y)).xy;
				rightDepth = texture(finalAngleAndDepthTex, rightTexCoords).y;
				rightIsInvalid = rightTexCoords.x < stepX && rightTexCoords.y < stepY;
				if((right > 1) || ((!rightIsInvalid) && (rightDepth < maxDepth))){
					foundRight = true;
				}
				else{
					right += stepX;
				}
			}
			if(!foundBottom){
				bottomTexCoords = texture(Cassini2ERPTex, vec2(cassiniTexCoords.x, bottom)).xy;
				bottomDepth = texture(finalAngleAndDepthTex, bottomTexCoords).y;
				bottomIsInvalid = bottomTexCoords.x < stepX && bottomTexCoords.y < stepY;
				if((bottom < 0) || ((!bottomIsInvalid) && (bottomDepth < maxDepth))){
					foundBottom = true;
				}
				else{
					bottom -= stepY;
				}
			}
			if(!foundTop){
				topTexCoords = texture(Cassini2ERPTex, vec2(cassiniTexCoords.x, top)).xy;
				topDepth = texture(finalAngleAndDepthTex, topTexCoords).y;
				topIsInvalid = topTexCoords.x < stepX && topTexCoords.y < stepY;
				if((top > 1) || ((!topIsInvalid) && (topDepth < maxDepth))){
					foundTop = true;
				}
				else{
					top += stepY;
				}
			}
		}
		bool horizontalOk = foundLeft && foundRight && (left > 0 || right < 10);
		bool verticalOk = foundBottom && foundTop && (bottom > 0 || top < 1);
		bool verticalSmallerThanHorizontal = (right - left) * out_width > (top - bottom) * out_height;

		if(horizontalOk && !(verticalOk && verticalSmallerThanHorizontal)){
			vec4 leftColor = texture(finalColorTex, leftTexCoords);
			vec4 rightColor = texture(finalColorTex, rightTexCoords);

			// if the depth is similar, blend the two colors together
			//float threshold = circumference_triangle_threshold_geometry * (1.0f + 3.0f * max(leftDepth, rightDepth) / far);
			float threshold = circumference_threshold_far / 2.0f;
			if(abs(leftDepth - rightDepth) < threshold){
				// weighted blend based on distance (larger distance means smaller weight)
				float leftDist = cassiniTexCoords.x - left;
				float rightDist = right - cassiniTexCoords.x;
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
			vec4 bottomColor = texture(finalColorTex, bottomTexCoords);
			vec4 topColor = texture(finalColorTex, topTexCoords);

			// if the depth is similar, blend the two colors together
			//float threshold = circumference_triangle_threshold_geometry * (1.0f + 3.0f * max(bottomDepth, topDepth) / far);
			float threshold = circumference_threshold_far / 2.0f;
			if(abs(bottomDepth - topDepth) < threshold){
				// weighted blend based on distance (larger distance means smaller weight)
				float bottomDist = cassiniTexCoords.y - bottom;
				float topDist = top - cassiniTexCoords.y;
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
	}

	else {
		// TODO anti-aliasing stuff
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