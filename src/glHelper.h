#ifndef GL_HELPER_H
#define GL_HELPER_H

#include "ioHelper.h"
#include "shader.h"


/*
* The ShaderController initializes the OpenGL shaders (in init()) and
* allows to set uniforms with updateInputParams()
*/
class ShaderController {
public:
	Shader shader;     // shader for 3D warping (vertex), deleting elongated triangles (geometry)
	                   // and blending the currently projected triangle mesh with the previous one (fragment).
	Shader copyShader; // shader for simply copying textures between FBOs
	Shader companionWindowShader;  // shader for simply copying textures from a FBO to the screen
	Shader cameraVisibilityShader; // shader to illustrate the positions of the cameras in a separate window


public:
	ShaderController() {
		cameraVisibilityShader = Shader();
		shader = Shader();
		copyShader = Shader();
		companionWindowShader = Shader();
	}

	bool init(InputCamera input, Options options, int out_width, int out_height, float chroma_offset, OutputCamera output) {

		std::string basePath = cmakelists_dir + "/src/";
		std::cout << "Reading GLSL files from " << basePath << std::endl;
		
		if (options.showCameraVisibilityWindow && !cameraVisibilityShader.init(
			(basePath + "cameras_vertex.fs").c_str(),
			(basePath + "cameras_frag.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "cameras_vertex.fs"
				<< " or " << basePath + "cameras_vertex.fs" << std::endl;
			return false;
		}
		if (!shader.init(
			(basePath + "vertex.fs").c_str(),
			(basePath + "fragment.fs").c_str(),
			(basePath + "geometry.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "vertex.fs"
				<< " or " << basePath + "fragment.fs"
				<< " or " << basePath + "geometry.fs" << std::endl;
			return false;
		}
		shader.use();
		shader.setFloat("out_width", (float)out_width);
		shader.setFloat("out_height", (float)out_height);
		shader.setFloat("chroma_offset", chroma_offset);

		
		float max_error_x = 1.0f / (1.0f / input.z_far + 0.5f / (std::pow(2, input.bitdepth_depth) - 1.0f) * (1.0f / input.z_near - 1.0f / input.z_far));
		float max_error = std::abs(1.0f / (1.0f / input.z_far) - max_error_x);
		float triangle_deletion_factor = max_error / std::pow(max_error_x - input.z_near, 2);
		shader.setFloat("triangle_deletion_factor", triangle_deletion_factor);
		shader.setFloat("triangle_deletion_margin", options.triangle_deletion_margin);

		shader.setFloat("depth_diff_threshold_fragment", options.depth_diff_threshold_fragment);
		shader.setFloat("image_border_threshold_fragment", options.image_border_threshold_fragment);
		shader.setInt("colorTex", 0);
		shader.setInt("depthTex", 1);
		shader.setInt("previousFBOColorTex", 2);
		shader.setInt("previousFBOAngleAndDepthTex", 3);

		shader.setFloat("convertYCbCrToRGB", options.saveOutputImages ? 0.0f : 1.0f);
		shader.setFloat("blendingThreshold", 0.001f + options.blendingFactor * 0.004f);
		shader.setFloat("isFirstInput", 1.0f);
		shader.setFloat("isYCbCr", options.usePNGs? 0.0f : 1.0f);
		shader.setFloat("width", float(input.res_x));
		shader.setFloat("height", float(input.res_y));
		shader.setFloat("projection_type", input.projection == Projection::Perspective ? 0.0f : (input.projection == Projection::Equirectangular ? 0.5f : 1.0f));
		if (input.projection == Projection::Equirectangular) {
			shader.setVec2("hor_range", input.hor_range);
			shader.setVec2("ver_range", input.ver_range);
		}
		else if (input.projection == Projection::Fisheye_equidistant) {
			shader.setFloat("fov", input.fov);
		}
		shader.setVec2("near_far", glm::vec2(input.z_near, input.z_far));
		shader.setVec3("inputCameraPos", input.pos);
		shader.setFloat("isVR", output.isVR? 1.0f : 0.0f);

		if (!copyShader.init(
			(basePath + "copy_vertex.fs").c_str(),
			(basePath + "copy_fragment.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "copy_vertex.fs"
				<< " or " << basePath + "copy_fragment.fs" << std::endl;
			return false;
		}
		copyShader.use();
		copyShader.setInt("previousFBOColorTex", 2);
		copyShader.setInt("previousFBOAngleAndDepthTex", 3);

		if (!companionWindowShader.init(
			(basePath + "copy_vertex.fs").c_str(),
			(basePath + "copy_fragment_1output.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "copy_vertex.fs"
				<< " or " << basePath + "copy_fragment_1output.fs" << std::endl;
			return false;
		}
		companionWindowShader.use();
		companionWindowShader.setInt("previousFBOColorTex", 0);

		return true;
	}

	void updateInputParams(InputCamera& input) {
		shader.use();
		shader.setVec2("near_far", glm::vec2(input.z_near, input.z_far));
		shader.setVec3("inputCameraPos", input.pos);
		shader.setMat4("model", input.model);
		if (input.projection == Projection::Perspective) {
			shader.setVec2("in_f", glm::vec2(input.focal_x, input.focal_y)); 
			shader.setVec2("in_pp", glm::vec2(input.principal_point_x, input.principal_point_y));
		}
	}

	void updateOutputParams(OutputCamera outputCamera) {
		shader.use();
		if (!outputCamera.isVR) {
			shader.setVec2("out_f", glm::vec2(outputCamera.focal_x, outputCamera.focal_y)); 
			shader.setVec2("out_near_far", glm::vec2(outputCamera.z_near, outputCamera.z_far));
			shader.setVec2("out_pp", glm::vec2(outputCamera.principal_point_x, outputCamera.principal_point_y));
		}
		shader.setMat4("view", outputCamera.view);
		shader.setVec3("outputCameraPos", glm::vec3(outputCamera.model[3])); 
	}
};

/*
* The FrameBufferController initializes the OpenGL FBOs, VAOs, VBOs, EBOs (in init()) and
* sets up the calls to glDrawElements().
* It has a system for constantly re-using the same 3 (per eye) FBOs , in order to save memory.
* 
* Example with n input cameras:
*   1. 3D warp the 1st input camera 
*         -> input  = 1 color texture + 1 depth texture
*         -> output = 1 color texture + 1 depth+angle texture, written to FBO_0 (angle is used for weighted blending)
*   2. copy the previous 2 outputs from FBO_0 to FBO_1
*         -> input  = 1 color texture + 1 depth+angle texture from FBO_0
*         -> output = 1 color texture + 1 depth+angle texture, written to FBO_1
*   3. 3D warp the 2nd input camera: see step 1., however now with output FBO_1. In this way, the previous intermediate output textures
*      are overwritten if the depth of the 2nd input camera is lower than the 1st, or if the depth is the same but the angle is smaller.
*   4. for 3rd, 4th, ... input cameras, repeat step 2. and 3.
*/
class FrameBufferController {
private:
	// FBOs:
	static const int nrFramebuffersPerEye = 3;
	int nrFramebuffers = 6;
	GLuint framebuffers[nrFramebuffersPerEye * 2];
	GLuint outputTexColors[nrFramebuffersPerEye * 2];
	GLuint outputTexAngleAndDepth[nrFramebuffersPerEye * 2];
	GLuint depthrenderbuffers[nrFramebuffersPerEye * 2];
	int index[2] = { 0 , 0 }; // index of framebuffer currently being rendered to (for each eye), in range [0, nrFramebuffers-1]

	// VBOs:
	GLuint VAO, VBO, EBO = 0;          // for 3D warping
	unsigned int quadVAO, quadVBO = 0; // for copying

	// some state:
	int nrIndices = 0;
	unsigned int* indices = NULL;
	float* texCoords = NULL;
	float initial_angle_and_depth[4] = { 10000.0f, 10000.0f, 10000.0f, 100000.0f };
	
	// for inpuatCameraVisibilityWindow
	bool showCameraVisibilityWindow = false;
	GLuint visibilityVAO, visibilityVBO, visibilityEBO = 0;
	unsigned int* inputCameraIndices = NULL;
	int nrInputCameraIndices = 0;
	float* inputCameraVertexPositions = NULL;
	int offset = 0;

public:

	FrameBufferController() {}
	
	void init(std::vector<InputCamera> inputCameras, unsigned int out_width, unsigned int out_height, Options options) {

		if (options.useVR) {
			nrFramebuffers = 2 * nrFramebuffersPerEye;
		}

		unsigned int in_width = (unsigned int)inputCameras[0].res_x;
		unsigned int in_height = (unsigned int)inputCameras[0].res_y;
		// source: http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/
		glGenFramebuffers(nrFramebuffers, framebuffers);
		glGenTextures(nrFramebuffers, outputTexColors);
		glGenTextures(nrFramebuffers, outputTexAngleAndDepth);
		glGenRenderbuffers(nrFramebuffers, depthrenderbuffers);
		for (int i = 0; i < nrFramebuffers; i++) {
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[i]);

			// Fragment shader output texture containing intermediary images
			glBindTexture(GL_TEXTURE_2D, outputTexColors[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, out_width, out_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outputTexColors[i], 0);

			// Fragment shader output texture containing intermediary depth
			glBindTexture(GL_TEXTURE_2D, outputTexAngleAndDepth[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, out_width, out_height, 0, GL_RG, GL_FLOAT, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, outputTexAngleAndDepth[i], 0);

			// Add a depth test buffer
			//GLuint depthrenderbuffer;
			//glGenRenderbuffers(1, &depthrenderbuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffers[i]);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, out_width, out_height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffers[i]);

			// Set the list of draw buffers.
			GLenum DrawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
			glDrawBuffers(2, DrawBuffers);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				throw std::runtime_error("glCheckFramebufferStatus incorrect");
			}

			glClearColor(options.backgroundColor.r, options.backgroundColor.g, options.backgroundColor.b, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			// clear the second draw buffer seperately
			glClearBufferfv(GL_COLOR, 1, initial_angle_and_depth);
		}

		// Setup the trianglemesh that will be drawn (shared by all input cameras)
		int triangleMeshWidth = in_width / options.triangleSizeInPixels;
		int triangleMeshHeight = in_height / options.triangleSizeInPixels;
		nrIndices = 6 * triangleMeshWidth * triangleMeshHeight; // 2 triangles per pixel, since pixel is in center of square
		indices = new unsigned int[nrIndices];
		unsigned int t = 0;
		for (int row = 0; row < (triangleMeshWidth + 1) * triangleMeshHeight; row += triangleMeshWidth + 1) {
			for (int col = 0; col < triangleMeshWidth; col++) {
				indices[t] = row + col;
				indices[t + 1] = row + triangleMeshWidth + col + 1;
				indices[t + 2] = row + col + 1;
				indices[t + 3] = row + col + 1;
				indices[t + 4] = row + triangleMeshWidth + col + 1;
				indices[t + 5] = row + triangleMeshWidth + col + 2;
				t += 6;
			}
		}

		// VBO with texture coordinates
		int nrVertices = (triangleMeshWidth + 1) * (triangleMeshHeight + 1);
		texCoords = new float[nrVertices * 2];
		t = 0;
		float width_f = float(triangleMeshWidth);
		float height_f = float(triangleMeshHeight);
		for (int row = 0; row < triangleMeshHeight + 1; row++) {
			for (int col = 0; col < triangleMeshWidth + 1; col++) {
				texCoords[t] = col / width_f;
				texCoords[t + 1] = row / height_f;
				t += 2;
			}
		}

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * nrVertices * 2, texCoords, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * nrIndices, indices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);


		// Setup a quad that fills the screen
		float quadVertices[] = {
			// positions (NDC)  // texCoords
			 1.0f,  1.0f,       1.0f, 1.0f,  // 0 top right
			 1.0f, -1.0f,       1.0f, 0.0f,  // 1 bottom right
			-1.0f,  1.0f,       0.0f, 1.0f,  // 3 top left 
			 1.0f, -1.0f,       1.0f, 0.0f,  // 1 bottom right
			-1.0f, -1.0f,       0.0f, 0.0f,  // 2 bottom left
			-1.0f,  1.0f,       0.0f, 1.0f   // 3 top left 
		};

		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

		// for inputCameraVisibilityWindow
		showCameraVisibilityWindow = options.showCameraVisibilityWindow;
		if (showCameraVisibilityWindow) {
			nrInputCameraIndices = 16 * ((int)inputCameras.size() + 1);
			offset = 5 * (int)inputCameras.size();
			inputCameraIndices = new unsigned int[nrInputCameraIndices];
			t = 0;
			for (int input = 0; input < (inputCameras.size() + 1); input++) {
				inputCameraIndices[t] = input * 5 + 0;
				inputCameraIndices[t + 1] = input * 5 + 1;
				inputCameraIndices[t + 2] = input * 5 + 0;
				inputCameraIndices[t + 3] = input * 5 + 2;
				inputCameraIndices[t + 4] = input * 5 + 0;
				inputCameraIndices[t + 5] = input * 5 + 3;
				inputCameraIndices[t + 6] = input * 5 + 0;
				inputCameraIndices[t + 7] = input * 5 + 4;
				inputCameraIndices[t + 8] = input * 5 + 1;
				inputCameraIndices[t + 9] = input * 5 + 2;
				inputCameraIndices[t + 10] = input * 5 + 2;
				inputCameraIndices[t + 11] = input * 5 + 4;
				inputCameraIndices[t + 12] = input * 5 + 4;
				inputCameraIndices[t + 13] = input * 5 + 3;
				inputCameraIndices[t + 14] = input * 5 + 3;
				inputCameraIndices[t + 15] = input * 5 + 1;
				t += 16;
			}
			std::vector<glm::vec3> temp;
			float s = 0.08f; // determines display size of the cameras in inputCameraVisibilityWindow
			for (auto& input : inputCameras) {
				temp.push_back(input.pos);
				temp.push_back(input.model * glm::vec4(-s, s, -s, 1));
				temp.push_back(input.model * glm::vec4(s, s, -s, 1));
				temp.push_back(input.model * glm::vec4(-s, -s, -s, 1));
				temp.push_back(input.model * glm::vec4(s, -s, -s, 1));
			}
			// output cam
			temp.push_back(glm::vec3(0));
			temp.push_back(glm::vec3(-s, s, -s));
			temp.push_back(glm::vec3(s, s, -s));
			temp.push_back(glm::vec3(-s, -s, -s));
			temp.push_back(glm::vec3(s, -s, -s));
			// to float array
			inputCameraVertexPositions = new float[temp.size() * 3];
			t = 0;
			for (auto& pos : temp) {
				inputCameraVertexPositions[t] = pos.x;
				inputCameraVertexPositions[t + 1] = pos.y;
				inputCameraVertexPositions[t + 2] = pos.z;
				t += 3;
			}

			glGenVertexArrays(1, &visibilityVAO);
			glGenBuffers(1, &visibilityVBO);
			glGenBuffers(1, &visibilityEBO);
			glBindVertexArray(visibilityVAO);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, visibilityEBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, nrInputCameraIndices * sizeof(unsigned int), inputCameraIndices, GL_STATIC_DRAW);

			glBindBuffer(GL_ARRAY_BUFFER, visibilityVBO);
			glBufferData(GL_ARRAY_BUFFER, temp.size() * 3 * sizeof(float), inputCameraVertexPositions, GL_STATIC_DRAW);
			// inputCamera position attribute
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
		}
	}
	
	GLuint getColorTexture(int eyeOffset) {
		return outputTexColors[index[eyeOffset] + (eyeOffset * 3)];
	}

	// simple 3D warping
	void renderTheFirstInputImage(int eyeOffset, GLuint image, GLuint depth) {
		index[eyeOffset] = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[index[eyeOffset] + (eyeOffset * 3)]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearBufferfv(GL_COLOR, 1, initial_angle_and_depth);
		glBindVertexArray(VAO);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, image);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depth);
		glDrawElements(GL_TRIANGLES, nrIndices, GL_UNSIGNED_INT, 0);
	}

	// copying between FBOs is necessary to prepare the blending
	void copyFramebuffer(int eyeOffset) {
		index[eyeOffset] = next(index[eyeOffset]);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[index[eyeOffset] + (eyeOffset * 3)]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearBufferfv(GL_COLOR, 1, initial_angle_and_depth);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, outputTexColors[previous(index[eyeOffset]) + (eyeOffset * 3)]);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, outputTexAngleAndDepth[previous(index[eyeOffset]) + (eyeOffset * 3)]);
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	// simple 3D warping + blending with the previous output image
	void renderNonFirstInputImage(int eyeOffset, GLuint image, GLuint depth) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[index[eyeOffset] + (eyeOffset * 3)]);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, image);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depth);
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, nrIndices, GL_UNSIGNED_INT, 0);
	}

	void bindCurrentBuffer() {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[index[0]]);
	}

	void drawInputCamera(int index) {
		glBindVertexArray(visibilityVAO);
		glDrawElementsBaseVertex(GL_LINES, 16, GL_UNSIGNED_INT, 0, 5 * index);
	}

	void drawOutputCamera() {
		glDrawElementsBaseVertex(GL_LINES, 16, GL_UNSIGNED_INT, 0, offset);
	}

	void cleanup() {
		glDeleteFramebuffers(nrFramebuffers, framebuffers);
		glDeleteTextures(nrFramebuffers, outputTexColors);
		glDeleteTextures(nrFramebuffers, outputTexAngleAndDepth);
		glDeleteRenderbuffers(nrFramebuffers, depthrenderbuffers);
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
		delete[] indices;
		delete[] texCoords;
		glDeleteVertexArrays(1, &quadVAO);
		glDeleteBuffers(1, &quadVBO);
		if (showCameraVisibilityWindow) {
			glDeleteVertexArrays(1, &visibilityVAO);
			glDeleteBuffers(1, &visibilityVBO);
			glDeleteBuffers(1, &visibilityEBO);
			delete[] inputCameraVertexPositions;
			delete[] inputCameraIndices;
		}
	}

private:
	int previous(int index) {
		return (index == 0) ? nrFramebuffersPerEye - 1 : index - 1;
	}
	int next(int index) {
		return (index == nrFramebuffersPerEye - 1) ? 0 : index + 1;
	}
};


#endif
