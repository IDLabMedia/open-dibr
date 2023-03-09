#ifndef VR_APPLICATION_H
#define VR_APPLICATION_H

#include <openvr.h>
#include "Application.h"


class VRApplication : public Application
{
public:

	VRApplication(Options options, FpsMonitor* fpsMonitor, std::vector<InputCamera> inputCameras);

	bool BInit();
	bool BInitGL();
	bool BInitCompositor();

	void Shutdown();

	bool HandleUserInput();
	bool RenderFrame(bool nextVideoFrame, std::string outputCameraName = "", int frameNr = 0);

	void SetupCameras();
	bool SetupStereoRenderTargets();
	void SetupCompanionWindow();

	void RenderCompanionWindow();
	void RenderScene(int i, bool isFirstInput);

	glm::mat4 GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye, float z_near, float z_far);
	glm::mat4 GetHMDMatrixPoseEye(vr::Hmd_Eye nEye);
	void UpdateHMDMatrixPose(glm::mat4& poseMat);
	glm::mat4 ConvertSteamVRMatrixToGlmMatrix(const vr::HmdMatrix34_t& matPose);


private: // OpenVR bookkeeping

	vr::IVRSystem* m_pHMD;
	vr::TrackedDevicePose_t m_rTrackedDevicePose[vr::k_unMaxTrackedDeviceCount];
	glm::mat4 m_rmat4DevicePose[vr::k_unMaxTrackedDeviceCount];

	struct ControllerInfo_t
	{
		vr::VRInputValueHandle_t m_source = vr::k_ulInvalidInputValueHandle;
		vr::VRActionHandle_t m_actionHaptic = vr::k_ulInvalidActionHandle;
	};

	enum EHand
	{
		Left = 0,
		Right = 1,
	};
	ControllerInfo_t m_rHand[2];
	vr::VRActionHandle_t m_actionDigitalInput = vr::k_ulInvalidActionHandle; // quit button
	vr::VRActionSetHandle_t m_actionsetDemo = vr::k_ulInvalidActionSetHandle;
	// move using trackpads (e.g. Vive controllers)
	vr::VRActionHandle_t m_actionAnalongInput_z = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t m_actionDigitalInput_z = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t m_actionAnalongInput_xy = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t m_actionDigitalInput_xy = vr::k_ulInvalidActionHandle;
	// move using joysticks (e.g. Oculus controllers)
	vr::VRActionHandle_t m_actionAnalongInput_z2 = vr::k_ulInvalidActionHandle;
	vr::VRActionHandle_t m_actionAnalongInput_xy2 = vr::k_ulInvalidActionHandle;

	std::string m_strPoseClasses;
	char m_rDevClassChar[vr::k_unMaxTrackedDeviceCount];
	
	glm::mat4 m_mat4ProjectionLeft;
	glm::mat4 m_mat4ProjectionRight;

private: // OpenGL bookkeeping

	glm::mat4 cameraOffset = glm::mat4(0);

	// inverse of position of HMD in player area at the start
	glm::mat4 inversePlayerAreaPosMat = glm::mat4(1);

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
VRApplication::VRApplication(Options options, FpsMonitor* fpsMonitor, std::vector<InputCamera> inputCameras)
	: Application(options, fpsMonitor, inputCameras, std::vector<OutputCamera>())
	, m_pHMD(NULL)
	, m_strPoseClasses("") 
{
	memset(m_rDevClassChar, 0, sizeof(m_rDevClassChar));
};

bool VRApplication::BInit()
{
	// Loading the SteamVR Runtime
	vr::EVRInitError eError = vr::VRInitError_None;
	m_pHMD = vr::VR_Init(&eError, vr::VRApplication_Scene);

	if (eError != vr::VRInitError_None)
	{
		m_pHMD = NULL;
		char buf[1024];
		snprintf(buf, sizeof(buf), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VR_Init Failed", buf, NULL);
		return false;
	}

	if (!BInitCompositor())
	{
		printf("%s - Failed to initialize VR Compositor!\n", __FUNCTION__);
		return false;
	}

	// Setup the desired input from the handheld VR controllers
	if (vr::VRInput()->SetActionManifestPath((cmakelists_dir + "/openvr_actions.json").c_str())) {
		std::cout << "VRInput()->SetActionManifestPath failed." << std::endl;
	}

	if (vr::VRInput()->GetActionHandle("/actions/demo/in/quit", &m_actionDigitalInput)) {
		std::cout << "VRInput()->GetActionHandle failed." << std::endl;
	}

	if (vr::VRInput()->GetActionHandle("/actions/demo/in/moving_z", &m_actionAnalongInput_z)) {
		std::cout << "VRInput()->GetActionHandle failed." << std::endl;
	}
	if (vr::VRInput()->GetActionHandle("/actions/demo/in/moving_xy", &m_actionAnalongInput_xy)) {
		std::cout << "VRInput()->GetActionHandle failed." << std::endl;
	}

	if (vr::VRInput()->GetActionHandle("/actions/demo/in/pressed_left_trackpad", &m_actionDigitalInput_z)) {
		std::cout << "VRInput()->GetActionHandle failed." << std::endl;
	}
	if (vr::VRInput()->GetActionHandle("/actions/demo/in/pressed_right_trackpad", &m_actionDigitalInput_xy)) {
		std::cout << "VRInput()->GetActionHandle failed." << std::endl;
	}

	if (vr::VRInput()->GetActionHandle("/actions/demo/in/moving_joystick_z", &m_actionAnalongInput_z2)) {
		std::cout << "VRInput()->GetActionHandle failed." << std::endl;
	}
	if (vr::VRInput()->GetActionHandle("/actions/demo/in/moving_joystick_xy", &m_actionAnalongInput_xy2)) {
		std::cout << "VRInput()->GetActionHandle failed." << std::endl;
	}

	if (vr::VRInput()->GetActionSetHandle("/actions/demo", &m_actionsetDemo)) {
		std::cout << "VRInput()->GetActionSetHandle failed." << std::endl;
	}

	if (vr::VRInput()->GetInputSourceHandle("/user/hand/left", &m_rHand[Left].m_source)) {
		std::cout << "VRInput()->GetInputSourceHandle failed." << std::endl;
	}

	if (vr::VRInput()->GetInputSourceHandle("/user/hand/right", &m_rHand[Right].m_source)) {
		std::cout << "VRInput()->GetInputSourceHandle failed." << std::endl;
	}

	return Application::BInit();
}

bool VRApplication::BInitGL()
{
	if (!Application::BInitGL()) {
		return false;
	}
	framebuffers.init(inputCameras, m_nRenderWidth, m_nRenderHeight, options);
	return true;
}

bool VRApplication::BInitCompositor()
{
	vr::EVRInitError peError = vr::VRInitError_None;

	if (!vr::VRCompositor())
	{
		printf("Compositor initialization failed. See log file for details\n");
		return false;
	}

	return true;
}

void VRApplication::Shutdown()
{
	if (m_pHMD)
	{
		vr::VR_Shutdown();
		m_pHMD = NULL;
	}

	Application::Shutdown();
}

bool VRApplication::HandleUserInput()
{
	SDL_Event sdlEvent;
	bool bRet = false;
	glm::vec3 movement(0);


	while (SDL_PollEvent(&sdlEvent) != 0)
	{
		if (sdlEvent.type == SDL_QUIT)
		{
			bRet = true;
		}
		else if (sdlEvent.type == SDL_KEYDOWN)
		{
			if (sdlEvent.key.keysym.sym == SDLK_ESCAPE)
			{
				bRet = true;
			}
			else if (sdlEvent.key.keysym.sym == SDLK_v)
			{
				cameraSpeed += 0.001f;
			}
			else if (sdlEvent.key.keysym.sym == SDLK_c)
			{
				cameraSpeed = std::max(cameraSpeed - 0.001f, 0.001f);
			}
			else if (sdlEvent.key.keysym.sym == SDLK_n)
			{
				options.blendingFactor = std::min(options.blendingFactor + 1, 10);
				std::cout << "changed blending_factor to " << options.blendingFactor << std::endl;
				shaders.shader.use();
				shaders.shader.setFloat("blendingThreshold", 0.001f + options.blendingFactor * 0.004f);
			}
			else if (sdlEvent.key.keysym.sym == SDLK_b)
			{
				options.blendingFactor = std::max(options.blendingFactor - 1, 0);
				std::cout << "changed blending_factor to " << options.blendingFactor << std::endl;
				shaders.shader.use();
				shaders.shader.setFloat("blendingThreshold", 0.001f + options.blendingFactor * 0.004f);
			}
			else if (sdlEvent.key.keysym.sym == SDLK_h)
			{
				options.triangle_deletion_margin += 2;
				std::cout << "changed triangle_deletion_margin to " << options.triangle_deletion_margin << std::endl;
				shaders.shader.use();
				shaders.shader.setFloat("triangle_deletion_margin", options.triangle_deletion_margin);
			}
			else if (sdlEvent.key.keysym.sym == SDLK_g)
			{
				options.triangle_deletion_margin = std::max(options.triangle_deletion_margin - 2, 1.0f);
				std::cout << "changed triangle_deletion_margin to " << options.triangle_deletion_margin << std::endl;
				shaders.shader.use();
				shaders.shader.setFloat("triangle_deletion_margin", options.triangle_deletion_margin);
			}
			else if (options.showCameraVisibilityWindow && sdlEvent.key.keysym.sym == SDLK_r) {
				controlCameraVisibilityWindow = !controlCameraVisibilityWindow;
				if (controlCameraVisibilityWindow) std::cout << "now controlling the small window in the bottom right corner" << std::endl;
				else std::cout << "now controlling the main window" << std::endl;
			}
		}
		else if (controlCameraVisibilityWindow) {
			if (sdlEvent.type == SDL_MOUSEBUTTONDOWN && sdlEvent.button.button == SDL_BUTTON_LEFT) {
				leftMouseDown = true;
				prev_mouse_pos_x = (float)sdlEvent.motion.x;
			}
			else if (sdlEvent.type == SDL_MOUSEBUTTONUP && sdlEvent.button.button == SDL_BUTTON_LEFT)
			{
				leftMouseDown = false;
			}
			else if (sdlEvent.type == SDL_MOUSEBUTTONDOWN && sdlEvent.button.button == SDL_BUTTON_MIDDLE) {
				middleMouseDown = true;
				prev_mouse_pos_x = (float)sdlEvent.motion.x;
			}
			else if (sdlEvent.type == SDL_MOUSEBUTTONUP && sdlEvent.button.button == SDL_BUTTON_MIDDLE)
			{
				middleMouseDown = false;
			}
			else if (leftMouseDown && sdlEvent.type == SDL_MOUSEMOTION)
			{
				float deltaX = (float)sdlEvent.motion.x - prev_mouse_pos_x;

				prev_mouse_pos_x = (float)sdlEvent.motion.x;

				cameraVisibilityWindow.angle -= deltaX * 0.005f;
			}
			else if (middleMouseDown && sdlEvent.type == SDL_MOUSEMOTION)
			{
				float deltaX = (float)sdlEvent.motion.x - prev_mouse_pos_x;

				prev_mouse_pos_x = (float)sdlEvent.motion.x;

				cameraVisibilityWindow.angle -= deltaX * 0.005f;
			}
			else if (sdlEvent.type == SDL_MOUSEWHEEL)
			{
				if (sdlEvent.wheel.y > 0) // scroll up
				{
					cameraVisibilityWindow.radius -= 0.05f * abs(cameraVisibilityWindow.radius);
				}
				else if (sdlEvent.wheel.y < 0) // scroll down
				{
					cameraVisibilityWindow.radius += 0.05f * abs(cameraVisibilityWindow.radius);
				}
			}
		}
	}


	// Process SteamVR action state
	// UpdateActionState is called each frame to update the state of the actions themselves. The application
	// controls which action sets are active with the provided array of VRActiveActionSet_t structs.
	vr::VRActiveActionSet_t actionSet = { 0 };
	actionSet.ulActionSet = m_actionsetDemo;
	vr::VRInput()->UpdateActionState(&actionSet, sizeof(actionSet), 1);

	// quit
	vr::InputDigitalActionData_t actionData;
	vr::VRInput()->GetDigitalActionData(m_actionDigitalInput, &actionData, sizeof(actionData), vr::k_ulInvalidInputValueHandle);
	if (actionData.bActive && actionData.bState) {
		bRet = true; // quit
	}

	// move using the trackpads (E.g. Vive controllers)
	vr::VRInput()->GetDigitalActionData(m_actionDigitalInput_z, &actionData, sizeof(actionData), vr::k_ulInvalidInputValueHandle);
	bool pressed_to_move_z = actionData.bActive && actionData.bState;
	vr::VRInput()->GetDigitalActionData(m_actionDigitalInput_xy, &actionData, sizeof(actionData), vr::k_ulInvalidInputValueHandle);
	bool pressed_to_move_xy = actionData.bActive && actionData.bState;

	vr::InputAnalogActionData_t analogData;
	vr::VRInput()->GetAnalogActionData(m_actionAnalongInput_z, &analogData, sizeof(analogData), vr::k_ulInvalidInputValueHandle);
	if (pressed_to_move_z && analogData.bActive) {
		movement.z -= cameraSpeed * analogData.y;
	}

	vr::InputAnalogActionData_t analogData_xy;
	vr::VRInput()->GetAnalogActionData(m_actionAnalongInput_xy, &analogData_xy, sizeof(analogData_xy), vr::k_ulInvalidInputValueHandle);
	if (pressed_to_move_xy && analogData_xy.bActive) {
		movement.x = cameraSpeed * analogData_xy.x;
		movement.y = cameraSpeed * analogData_xy.y;
	}

	// move using the joysticks (e.g. Oculus controllers)
	vr::InputAnalogActionData_t analogData2;
	vr::VRInput()->GetAnalogActionData(m_actionAnalongInput_z2, &analogData2, sizeof(analogData2), vr::k_ulInvalidInputValueHandle);
	if (analogData2.bActive) {
		movement.z -= cameraSpeed * analogData2.y;
	}

	vr::InputAnalogActionData_t analogData_xy2;
	vr::VRInput()->GetAnalogActionData(m_actionAnalongInput_xy2, &analogData_xy2, sizeof(analogData_xy2), vr::k_ulInvalidInputValueHandle);
	if (analogData_xy2.bActive) {
		movement.x = cameraSpeed * analogData_xy2.x;
		movement.y = cameraSpeed * analogData_xy2.y;
	}

	const Uint8* state = SDL_GetKeyboardState(NULL);
	if (state[SDL_SCANCODE_W]) { // UP
		movement.y += cameraSpeed;
	}
	if (state[SDL_SCANCODE_S]) { // DOWN
		movement.y -= cameraSpeed;
	}
	if (state[SDL_SCANCODE_D]) { // RIGHT
		movement.x += cameraSpeed;
	}
	if (state[SDL_SCANCODE_A]) { // LEFT
		movement.x -= cameraSpeed;
	}
	if (state[SDL_SCANCODE_Z]) { // BACKWARD
		movement.z += cameraSpeed;
	}
	if (state[SDL_SCANCODE_Q]) { // FORWARD
		movement.z -= cameraSpeed;
	}

	if (movement != glm::vec3(0)) {
		cameraOffset[3] += pcOutputCamera.model * glm::vec4(movement, 0);
	}

	return bRet;
}

bool VRApplication::RenderFrame(bool nextVideoFrame, std::string outputCameraName, int frameNr)
{
	bool shouldUpdateUsedInputs = false;
	if (m_pHMD)
	{
		shouldUpdateUsedInputs = RenderTarget(nextVideoFrame);
		RenderCompanionWindow();

		vr::Texture_t leftEyeTexture = {(void*)(uintptr_t)framebuffers.getColorTexture(vr::Eye_Left), vr::TextureType_OpenGL, vr::ColorSpace_Gamma};
		vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);
		vr::Texture_t rightEyeTexture = { (void*)(uintptr_t)framebuffers.getColorTexture(vr::Eye_Right), vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
		vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);
	}

	// SwapWindow
	{
		SDL_GL_SwapWindow(m_pCompanionWindow);
	}

	// Clear
	{
		// We want to make sure the glFinish waits for the entire present to complete, not just the submission
		// of the command. So, we do a clear here right here so the glFinish will wait fully for the swap.
		glClearColor(options.backgroundColor.r, options.backgroundColor.g, options.backgroundColor.b, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	// update HMD pose stored in pcOutputCamera.model and view
	glm::mat4 playerAreaPosMat = glm::mat4(1);
	UpdateHMDMatrixPose(playerAreaPosMat);
	pcOutputCamera.model = pcOutputCamera.startPosMat * pcOutputCamera.startRotMat * inversePlayerAreaPosMat * playerAreaPosMat + cameraOffset;
	pcOutputCamera.view = glm::inverse(pcOutputCamera.model);

	return shouldUpdateUsedInputs;
}

void VRApplication::SetupCameras()
{
	float z_near = 0.01f;
	float z_far = 1010.0f;

	// VR output camera: projection and eye pos matrices
	m_mat4ProjectionLeft = GetHMDMatrixProjectionEye(vr::Eye_Left, z_near, z_far) * GetHMDMatrixPoseEye(vr::Eye_Left);
	m_mat4ProjectionRight = GetHMDMatrixProjectionEye(vr::Eye_Right, z_near, z_far) * GetHMDMatrixPoseEye(vr::Eye_Right);

	float left = 0.f, right = 0.f, top = 0.f, bottom = 0.f;
	m_pHMD->GetProjectionRaw(vr::Eye_Left, &left, &right, &top, &bottom);
	float FOV_x = std::abs(std::atan(right)) + std::abs(std::atan(left));
	float FOV_y = std::abs(std::atan(top)) + std::abs(std::atan(bottom));
	std::cout << "FOV_x = " << glm::degrees(FOV_x) << " degrees, FOV_y = " << glm::degrees(FOV_y) << " degrees" << std::endl;

	// VR output camera: position and model
	pcOutputCamera = OutputCamera(m_nRenderWidth, m_nRenderHeight, options.viewport, m_mat4ProjectionLeft, m_mat4ProjectionRight, FOV_x, FOV_y, z_near, z_far);
	// store the relative position of the headset w.r.t. the player area in pcOutputCamera.model[3]
	glm::mat4 poseMat = glm::mat4(1);
	UpdateHMDMatrixPose(poseMat);
	glm::mat4 playerAreaPosMat = glm::mat4(1);
	playerAreaPosMat[3] = glm::vec4(poseMat[3][0], poseMat[3][1], poseMat[3][2], poseMat[3][3]);
	inversePlayerAreaPosMat = glm::inverse(playerAreaPosMat);

	cameraVisibilityHelper.init(inputCameras, &pcOutputCamera, options.maxNrInputsUsed);
	current_inputsToUse = cameraVisibilityHelper.updateInputsToUse();
	for (auto& c : current_inputsToUse) {
		next_inputsToUse.insert(c); // deep copy
	}
}

bool VRApplication::SetupStereoRenderTargets()
{
	if (!m_pHMD)
		return false;

	m_pHMD->GetRecommendedRenderTargetSize(&m_nRenderWidth, &m_nRenderHeight);

	std::cout << "Resolution VR texture (per eye) = " << m_nRenderWidth << " x " << m_nRenderHeight << std::endl;

	// give the companion window the same aspect ratio as m_nRenderWidth x m_nRenderHeight
	float aspect = float(m_nRenderWidth) / float(m_nRenderHeight);
	if (aspect < 1) {
		companionWindowWidth = options.SCR_HEIGHT * aspect;
		companionWindowHeight = options.SCR_HEIGHT;
	}
	else {
		companionWindowWidth = options.SCR_WIDTH;
		companionWindowHeight = options.SCR_WIDTH / aspect;
	}

	SDL_SetWindowSize(m_pCompanionWindow, companionWindowWidth, companionWindowHeight);
	std::cout << "Set companion window size to " << companionWindowWidth << " x " << companionWindowHeight << std::endl;

	return true;
}

void VRApplication::SetupCompanionWindow()
{
	if (!m_pHMD)
		return;

	std::vector<VertexDataWindow> vVerts;

	// left eye verts
	/*vVerts.push_back(VertexDataWindow(glm::vec2(-1, -1), glm::vec2(0, 0)));
	vVerts.push_back(VertexDataWindow(glm::vec2(0, -1), glm::vec2(1, 0)));
	vVerts.push_back(VertexDataWindow(glm::vec2(-1, 1), glm::vec2(0, 1)));
	vVerts.push_back(VertexDataWindow(glm::vec2(0, 1), glm::vec2(1, 1)));*/
	
	// let the left eye fill the whole screen instead
	vVerts.push_back(VertexDataWindow(glm::vec2(-1, -1), glm::vec2(0, 0)));
	vVerts.push_back(VertexDataWindow(glm::vec2(1, -1), glm::vec2(1, 0)));
	vVerts.push_back(VertexDataWindow(glm::vec2(-1, 1), glm::vec2(0, 1)));
	vVerts.push_back(VertexDataWindow(glm::vec2(1, 1), glm::vec2(1, 1)));

	// right eye verts
	vVerts.push_back(VertexDataWindow(glm::vec2(0, -1), glm::vec2(0, 0)));
	vVerts.push_back(VertexDataWindow(glm::vec2(1, -1), glm::vec2(1, 0)));
	vVerts.push_back(VertexDataWindow(glm::vec2(0, 1), glm::vec2(0, 1)));
	vVerts.push_back(VertexDataWindow(glm::vec2(1, 1), glm::vec2(1, 1)));

	GLushort vIndices[] = { 0, 1, 3,   0, 3, 2,   4, 5, 7,   4, 7, 6 };
	m_uiCompanionWindowIndexSize = _countof(vIndices);

	glGenVertexArrays(1, &m_unCompanionWindowVAO);
	glBindVertexArray(m_unCompanionWindowVAO);

	glGenBuffers(1, &m_glCompanionWindowIDVertBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_glCompanionWindowIDVertBuffer);
	glBufferData(GL_ARRAY_BUFFER, vVerts.size() * sizeof(VertexDataWindow), &vVerts[0], GL_STATIC_DRAW);

	glGenBuffers(1, &m_glCompanionWindowIDIndexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_glCompanionWindowIDIndexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_uiCompanionWindowIndexSize * sizeof(GLushort), &vIndices[0], GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VertexDataWindow), (void*)offsetof(VertexDataWindow, position));

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexDataWindow), (void*)offsetof(VertexDataWindow, texCoord));

	glBindVertexArray(0);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	if (options.showCameraVisibilityWindow) {
		cameraVisibilityWindowWidth = companionWindowWidth / 3;
		cameraVisibilityWindowHeight = companionWindowHeight / 3;
		cameraVisibilityWindow.init(cameraVisibilityWindowWidth, cameraVisibilityWindowHeight, inputCameras);
	}
}

void VRApplication::RenderScene(int i, bool isFirstInput)
{
	shaders.shader.setMat4("project", pcOutputCamera.projectionLeft);
	for (vr::EVREye eye : {vr::EVREye::Eye_Left, vr::EVREye::Eye_Right}) {
		if (eye == vr::EVREye::Eye_Right) {
			shaders.shader.setMat4("project", pcOutputCamera.projectionRight);
		}
		if (isFirstInput) {
			framebuffers.renderTheFirstInputImage(eye, textures_color[i], textures_depth[i]);
		}
		else {
			shaders.copyShader.use();
			framebuffers.copyFramebuffer(eye);

			shaders.shader.use();
			framebuffers.renderNonFirstInputImage(eye, textures_color[i], textures_depth[i]);
		}
	}
}

void VRApplication::RenderCompanionWindow()
{
	glDisable(GL_DEPTH_TEST);
	glViewport(0, 0, companionWindowWidth, companionWindowHeight);

	glBindVertexArray(m_unCompanionWindowVAO);
	shaders.companionWindowShader.use();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, framebuffers.getColorTexture(vr::Eye_Left));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glDrawElements(GL_TRIANGLES, m_uiCompanionWindowIndexSize / 2, GL_UNSIGNED_SHORT, 0);

	// render right eye (second half of index array )
	/*glBindTexture(GL_TEXTURE_2D, framebuffers.getColorTexture(vr::Eye_Right));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glDrawElements(GL_TRIANGLES, m_uiCompanionWindowIndexSize / 2, GL_UNSIGNED_SHORT, (const void*)(uintptr_t)(m_uiCompanionWindowIndexSize));*/

	// draw input cameras
	if (options.showCameraVisibilityWindow) {
		glViewport(companionWindowWidth - cameraVisibilityWindowWidth, 0, cameraVisibilityWindowWidth, cameraVisibilityWindowHeight);
		glEnable(GL_SCISSOR_TEST);
		glScissor(companionWindowWidth - cameraVisibilityWindowWidth, 0, cameraVisibilityWindowWidth, cameraVisibilityWindowHeight);
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);
		glm::mat4 view_project = cameraVisibilityWindow.ViewProject();
		shaders.cameraVisibilityShader.use();
		shaders.cameraVisibilityShader.setMat4("view_project", view_project);
		for (int i = 0; i < inputCameras.size(); i++) {
			shaders.cameraVisibilityShader.setVec3("color", (current_inputsToUse.find(i) != current_inputsToUse.end()) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0));
			framebuffers.drawInputCamera(i);
		}
		shaders.cameraVisibilityShader.setMat4("view_project", view_project * pcOutputCamera.model);
		shaders.cameraVisibilityShader.setVec3("color", glm::vec3(0, 1, 1));
		framebuffers.drawOutputCamera();
	}
}

glm::mat4 VRApplication::GetHMDMatrixProjectionEye(vr::Hmd_Eye nEye, float z_near, float z_far)
{
	if (!m_pHMD)
		return glm::mat4(0);

	vr::HmdMatrix44_t mat = m_pHMD->GetProjectionMatrix(nEye, z_near, z_far);

	return glm::mat4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
		mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
		mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
	);
}

glm::mat4 VRApplication::GetHMDMatrixPoseEye(vr::Hmd_Eye nEye)
{
	if (!m_pHMD)
		return glm::mat4(1);

	vr::HmdMatrix34_t matEyeRight = m_pHMD->GetEyeToHeadTransform(nEye);
	glm::mat4 matrixObj = glm::mat4(
		matEyeRight.m[0][0], matEyeRight.m[1][0], matEyeRight.m[2][0], 0.0,
		matEyeRight.m[0][1], matEyeRight.m[1][1], matEyeRight.m[2][1], 0.0,
		matEyeRight.m[0][2], matEyeRight.m[1][2], matEyeRight.m[2][2], 0.0,
		matEyeRight.m[0][3], matEyeRight.m[1][3], matEyeRight.m[2][3], 1.0f
	);

	return glm::inverse(matrixObj);
}

void VRApplication::UpdateHMDMatrixPose(glm::mat4& poseMat)
{
	if (!m_pHMD)
		return;

	vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0);

	m_strPoseClasses = "";
	for (int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice)
	{
		if (m_rTrackedDevicePose[nDevice].bPoseIsValid)
		{
			m_rmat4DevicePose[nDevice] = ConvertSteamVRMatrixToGlmMatrix(m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking);
			if (m_rDevClassChar[nDevice] == 0)
			{
				switch (m_pHMD->GetTrackedDeviceClass(nDevice))
				{
				case vr::TrackedDeviceClass_Controller:        m_rDevClassChar[nDevice] = 'C'; break;
				case vr::TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
				case vr::TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
				case vr::TrackedDeviceClass_GenericTracker:    m_rDevClassChar[nDevice] = 'G'; break;
				case vr::TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
				default:                                       m_rDevClassChar[nDevice] = '?'; break;
				}
			}
			m_strPoseClasses += m_rDevClassChar[nDevice];
		}
	}

	if (m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
	{
		poseMat = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
	}
}

glm::mat4 VRApplication::ConvertSteamVRMatrixToGlmMatrix(const vr::HmdMatrix34_t& matPose)
{
	glm::mat4 matrixObj(
		matPose.m[0][0], matPose.m[1][0], matPose.m[2][0], 0.0,
		matPose.m[0][1], matPose.m[1][1], matPose.m[2][1], 0.0,
		matPose.m[0][2], matPose.m[1][2], matPose.m[2][2], 0.0,
		matPose.m[0][3], matPose.m[1][3], matPose.m[2][3], 1.0f
	);
	return matrixObj;
}

#endif VR_APPLICATION_H
