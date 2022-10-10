#ifndef APPLICATION_H
#define APPLICATION_H


#include <SDL.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#if defined( OSX )
#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>
#include <OpenGL/glu.h>
// Apple's version of glut.h #undef's APIENTRY, redefine it
#define APIENTRY
#else
#include <GL/glu.h>
#endif
#include <stdio.h>
#include <string>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <algorithm>
#include <thread>
#include <deque>
#include <cuda.h>
#include <cudaGL.h> // CUDA OpenGL interop needed for cuGraphicsGLRegisterImage


#if defined(POSIX)
#include "unistd.h"
#endif

#ifndef _WIN32
#define APIENTRY
#endif

#ifndef _countof
#define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "FFmpegDemuxer.h"
#include "shader.h"
#include "ioHelper.h"
#include "glHelper.h"
#include "Pool.h"
#include "CameraVisibilityHelper.h"
#include "MeasureFPS.h"


class Application
{
public:
	Application(Options options, FpsMonitor* fpsMonitor, std::vector<InputCamera> inputCameras, std::vector<OutputCamera> outputCameras);

	virtual bool BInit();
	virtual bool BInitGL();

	virtual void Shutdown();

	void RunMainLoop();
	virtual bool HandleUserInput();
	virtual bool RenderFrame(bool nextVideoFrame, bool updateCurrentInputsToUse, std::string outputCameraName = "", int frameNr = 0);

	virtual void SetupCameras();
	virtual bool SetupStereoRenderTargets();
	virtual void SetupCompanionWindow();
	void SetupYUV420Textures(int texture_height, int luma_height);
	bool SetupRGBTextures();
	void SetupCUgraphicsResources();
	bool SetupDecodingPool();

	bool RenderTarget(bool nextVideoFrame, bool updateCurrentInputsToUse);
	virtual void RenderCompanionWindow();
	virtual void RenderScene(int i, bool isFirstInput);

	bool CreateAllShaders(float chroma_offset);
	void SaveCompanionWindowToYUV(int frameNr, std::string filename);

protected:

// SDL bookkeeping
	SDL_Window* m_pCompanionWindow;
	SDL_GLContext m_pContext;

// OpenGL bookkeeping

	GLuint m_unCompanionWindowVAO;
	GLuint m_glCompanionWindowIDVertBuffer;
	GLuint m_glCompanionWindowIDIndexBuffer;
	unsigned int m_uiCompanionWindowIndexSize;
	unsigned int m_uiControllerVertcount;

	struct VertexDataWindow
	{
		glm::vec2 position;
		glm::vec2 texCoord;

		VertexDataWindow(const glm::vec2& pos, const glm::vec2 tex) : position(pos), texCoord(tex) {	}
	};

	uint32_t m_nRenderWidth;
	uint32_t m_nRenderHeight;
	uint32_t companionWindowWidth;
	uint32_t companionWindowHeight;
	uint32_t cameraVisibilityWindowWidth;
	uint32_t cameraVisibilityWindowHeight;

	Options options;
	OutputCamera pcOutputCamera;
	std::vector<InputCamera> inputCameras;
	std::vector<OutputCamera> outputCameras;
	ShaderController shaders;
	FrameBufferController framebuffers;
	CameraVisibilityHelper cameraVisibilityHelper;
	CameraVisibilityWindow cameraVisibilityWindow;
	Pool pool;
	FpsMonitor* fpsMonitor;

	GLuint* textures_color = NULL;
	GLuint* textures_depth = NULL;

	// video decoding
	std::vector<CUgraphicsResource*> glGraphicsResources;
	std::vector<FFmpegDemuxer*> demuxers;
	std::vector<NvDecoder*> decoders;
	CUcontext* cuContext = NULL;

	// for rendering
	std::unordered_set<int> current_inputsToUse;
	std::unordered_set<int> next_inputsToUse;
	int currentVideoFrame = 0;
	float cameraSpeed = 0.01f;
	bool controlCameraVisibilityWindow = false;

	// some user input state
	bool leftMouseDown = false;
	bool middleMouseDown = false;
	float prev_mouse_pos_x = 0;
	float prev_mouse_pos_y = 0;

};

Application::Application(Options options, FpsMonitor* fpsMonitor, std::vector<InputCamera> inputCameras, std::vector<OutputCamera> outputCameras)
	: m_pCompanionWindow(NULL)
	, m_pContext(NULL)
	, options(options)
	, fpsMonitor(fpsMonitor)
	, inputCameras(inputCameras)
	, outputCameras(outputCameras) {
	cuContext = new CUcontext();
};

bool Application::BInit()
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
	{
		printf("%s - SDL could not initialize! SDL Error: %s\n", __FUNCTION__, SDL_GetError());
		return false;
	}

	int nWindowPosX = 20;
	int nWindowPosY = 20;
	Uint32 unWindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	//SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);

	m_pCompanionWindow = SDL_CreateWindow("OpenDIBR", nWindowPosX, nWindowPosY, options.SCR_WIDTH, options.SCR_HEIGHT, unWindowFlags);
	if (m_pCompanionWindow == NULL)
	{
		printf("%s - Window could not be created! SDL Error: %s\n", __FUNCTION__, SDL_GetError());
		return false;
	}

	m_pContext = SDL_GL_CreateContext(m_pCompanionWindow);
	if (m_pContext == NULL)
	{
		printf("%s - OpenGL context could not be created! SDL Error: %s\n", __FUNCTION__, SDL_GetError());
		return false;
	}

	glewExperimental = GL_TRUE;
	GLenum nGlewError = glewInit();
	if (nGlewError != GLEW_OK)
	{
		printf("%s - Error initializing GLEW! %s\n", __FUNCTION__, glewGetErrorString(nGlewError));
		return false;
	}
	glGetError(); // to clear the error caused deep in GLEW

	if (SDL_GL_SetSwapInterval(0) < 0) // 0 means vsync off, 1 means vsync on
	{
		printf("%s - Warning: Unable to set VSync! SDL Error: %s\n", __FUNCTION__, SDL_GetError());
		return false;
	}

	std::string strWindowTitle = "OpenDIBR";
	SDL_SetWindowTitle(m_pCompanionWindow, strWindowTitle.c_str());

	if (!BInitGL())
	{
		printf("%s - Unable to initialize OpenGL!\n", __FUNCTION__);
		return false;
	}
	return true;
}

bool Application::BInitGL()
{
	// the chroma data is stored "chroma_offset" rows below the luma data
	int luma_height = inputCameras[0].res_y;
	int luma_height_rounded = ((luma_height + 16 - 1) / 16) * 16; //round luma height up to multiple of 16
	int texture_height = luma_height_rounded + /*chroma height */luma_height / 2;
	float chroma_offset = float(luma_height_rounded - luma_height);
	SetupCameras(); // needs to go first
	SetupStereoRenderTargets();
	if (!CreateAllShaders(chroma_offset))
		return false;

	if (options.usePNGs) {
		if (!SetupRGBTextures()) {
			return false;
		}
	}
	else {
		SetupYUV420Textures(texture_height, luma_height);
	}
	SetupCompanionWindow();
	if (!options.usePNGs) {
		SetupCUgraphicsResources();
		if (!SetupDecodingPool()) {
			return false;
		}
	}
	return true;
}

void Application::Shutdown()
{
	if (m_pContext)
	{
		if (m_unCompanionWindowVAO != 0)
		{
			glDeleteVertexArrays(1, &m_unCompanionWindowVAO);
			glDeleteBuffers(1, &m_glCompanionWindowIDVertBuffer);
			glDeleteBuffers(1, &m_glCompanionWindowIDIndexBuffer);
		}
	}

	if (!options.isStatic) {
		pool.cleanup();
	}

	framebuffers.cleanup();

	if (!options.usePNGs) {
		for (auto& glGraphicsResource : glGraphicsResources) {
			ck(cuGraphicsUnregisterResource(*glGraphicsResource));
			delete glGraphicsResource;
		}
		glGraphicsResources.clear();
		for (auto& demuxer : demuxers) {
			delete demuxer;
		}
		demuxers.clear();
		for (auto& decoder : decoders) {
			delete decoder;
		}
		decoders.clear();

		// do this after decoders are cleared
		if (cuContext) {
			ck(cuCtxDestroy(*cuContext));
			delete cuContext;
		}
	}

	if (textures_color != NULL) {
		glDeleteTextures((GLsizei)inputCameras.size(), textures_color);
		delete[] textures_color;
	}
	if (textures_depth != NULL) {
		glDeleteTextures((GLsizei)inputCameras.size(), textures_depth);
		delete[] textures_depth;
	}


	if (m_pCompanionWindow)
	{
		SDL_DestroyWindow(m_pCompanionWindow);
		m_pCompanionWindow = NULL;
	}

	SDL_Quit();
}

bool Application::HandleUserInput()
{
	return false;
}

void Application::RunMainLoop()
{
	bool bQuit = false;

	SDL_StartTextInput();

	int frame = 0;
	bool updateUsedInputs = true;

	if (!options.asap) {
		// decode and play the images/videos at options.targetFps fps
		// e.g. the videos themselves are played at 30Hz while the application renders at 90Hz
		float ms_per_frame = 1000.0f / (float)options.targetFps;

		Uint64 startTime = SDL_GetPerformanceCounter();
		while (!bQuit)
		{
			// update the video frame (goal = 30Hz)
			updateUsedInputs = RenderFrame(true, updateUsedInputs);
			bQuit = bQuit | HandleUserInput();

			SpinUntilTargetTime(startTime, ms_per_frame);
			Uint64 endTime = SDL_GetPerformanceCounter();
			float passedTimeMs = (endTime - startTime) / (float)SDL_GetPerformanceFrequency() * 1000.0f;
			fpsMonitor->AddTime(passedTimeMs, frame);
			startTime = endTime;

			// keep rendering with the same video frame (goal = options.targetFps)
			for (int i = 0; i < options.targetFps / 30 - 1; i++) {
				Uint64 currentTime = SDL_GetPerformanceCounter();
				RenderFrame(false, false);
				bQuit = bQuit | HandleUserInput();
				SpinUntilTargetTime(currentTime, ms_per_frame);

				endTime = SDL_GetPerformanceCounter();
				passedTimeMs = (endTime - startTime) / (float)SDL_GetPerformanceFrequency() * 1000.0f;
				fpsMonitor->AddTime(passedTimeMs, frame);
				startTime = endTime;
			}
			frame++;
		}
	}
	else if (options.saveOutputImages) {
		for (int frame = 0; frame < options.outputNrFrames; frame++) {
			for (int i = 0; i < outputCameras.size(); i++) {
				pcOutputCamera = outputCameras[i];

				updateUsedInputs = RenderFrame(frame > 0 && i == 0, false, outputCameras[i].name, frame);
			}
		}
	}
	else {
		// decode and play the input videos as fast as possible
		Uint64 startTime = SDL_GetPerformanceCounter();
		while (!bQuit) {
			updateUsedInputs = RenderFrame(true, updateUsedInputs);
			bQuit = bQuit | HandleUserInput();

			Uint64 endTime = SDL_GetPerformanceCounter();
			float passedTimeMs = (endTime - startTime) / (float)SDL_GetPerformanceFrequency() * 1000.0f;
			fpsMonitor->AddTime(passedTimeMs, frame);
			startTime = endTime;
			frame++;
		}
	}


	SDL_StopTextInput();
}

bool Application::RenderFrame(bool nextVideoFrame, bool updateCurrentInputsToUse, std::string outputCameraName, int frameNr)
{
	bool shouldUpdateUsedInputs = RenderTarget(nextVideoFrame, updateCurrentInputsToUse);
	if (outputCameraName != "") {
		SaveCompanionWindowToYUV(frameNr, outputCameraName);
	}
	
	RenderCompanionWindow();

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

	return shouldUpdateUsedInputs;
}

bool Application::CreateAllShaders(float chroma_offset)
{
	return shaders.init(inputCameras[0], options, m_nRenderWidth, m_nRenderHeight, chroma_offset, pcOutputCamera);
}

void Application::SetupCameras()
{
	// TODO works?
	//pcOutputCamera = OutputCamera(inputCameras[inputCameras.size() / 2], options.SCR_WIDTH, options.SCR_HEIGHT, options.SCR_F, options.SCR_PP, options.SCR_NEAR_FAR);
	pcOutputCamera = options.viewport;

	cameraVisibilityHelper.init(inputCameras, &pcOutputCamera, options.maxNrInputsUsed);
	current_inputsToUse = cameraVisibilityHelper.updateInputsToUse();
	for (auto& c : current_inputsToUse) {
		next_inputsToUse.insert(c); // deep copy
	}
}

bool Application::SetupStereoRenderTargets()
{
	m_nRenderWidth = options.SCR_WIDTH;
	m_nRenderHeight = options.SCR_HEIGHT;

	companionWindowWidth = options.SCR_WIDTH;
	companionWindowHeight = options.SCR_HEIGHT;
	return true;
}

void Application::SetupCompanionWindow()
{
	std::vector<VertexDataWindow> vVerts;

	vVerts.push_back(VertexDataWindow(glm::vec2(-1, -1), glm::vec2(0, 0)));
	vVerts.push_back(VertexDataWindow(glm::vec2(1, -1), glm::vec2(1, 0)));
	vVerts.push_back(VertexDataWindow(glm::vec2(-1, 1), glm::vec2(0, 1)));
	vVerts.push_back(VertexDataWindow(glm::vec2(1, 1), glm::vec2(1, 1)));

	GLushort vIndices[] = { 0, 1, 3,   0, 3, 2 };
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

void Application::SetupYUV420Textures(int texture_height, int luma_height) {
	textures_color = new GLuint[inputCameras.size()];
	textures_depth = new GLuint[inputCameras.size()];
	glGenTextures((GLsizei)inputCameras.size(), textures_color);
	glGenTextures((GLsizei)inputCameras.size(), textures_depth);
	for (int i = 0; i < inputCameras.size(); i++) {
		// technically only need #threads * 2 textures
		// but for now, use 2 textures per input
		glBindTexture(GL_TEXTURE_2D, textures_color[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if (inputCameras[i].bitdepth_color > 8) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, inputCameras[0].res_x, texture_height, 0, GL_RED, GL_UNSIGNED_SHORT, 0);
		}
		else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, inputCameras[0].res_x, texture_height, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
		}

		glBindTexture(GL_TEXTURE_2D, textures_depth[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if (inputCameras[i].bitdepth_depth > 8) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, inputCameras[0].res_x, luma_height, 0, GL_RED, GL_UNSIGNED_SHORT, 0);
		}
		else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, inputCameras[0].res_x, luma_height, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
		}
	}
}

bool Application::SetupRGBTextures() {
	textures_color = new GLuint[inputCameras.size()];
	textures_depth = new GLuint[inputCameras.size()];
	glGenTextures((GLsizei)inputCameras.size(), textures_color);
	glGenTextures((GLsizei)inputCameras.size(), textures_depth);

	int width, height, nrChannels;
	for (int i = 0; i < inputCameras.size(); i++) {
		// technically only need #threads * 2 textures
		// but for now, use 2 textures per input
		glBindTexture(GL_TEXTURE_2D, textures_color[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if (inputCameras[i].bitdepth_color > 8) {
			unsigned short* data = stbi_load_16(inputCameras[i].pathColor.c_str(), &width, &height, &nrChannels, STBI_rgb);
			if (!data) {
				std::cout << "Error: failed to load texture " << inputCameras[i].pathColor << std::endl;
				return false;
			}
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16, inputCameras[0].res_x, inputCameras[0].res_y, 0, GL_RGB, GL_UNSIGNED_SHORT, data);
			stbi_image_free(data);
		}
		else {
			unsigned char* data = stbi_load(inputCameras[i].pathColor.c_str(), &width, &height, &nrChannels, STBI_rgb);
			if (!data) {
				std::cout << "Error: failed to load texture " << inputCameras[i].pathColor << std::endl;
				return false;
			}
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, inputCameras[0].res_x, inputCameras[0].res_y, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}

		glBindTexture(GL_TEXTURE_2D, textures_depth[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		if (inputCameras[i].bitdepth_depth > 8) {
			unsigned short* data = stbi_load_16(inputCameras[i].pathDepth.c_str(), &width, &height, &nrChannels, STBI_grey);
			if (!data) {
				std::cout << "Failed to load texture " << inputCameras[i].pathDepth << std::endl;
				return false;
			}
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, inputCameras[0].res_x, inputCameras[0].res_y, 0, GL_RED, GL_UNSIGNED_SHORT, data);
			stbi_image_free(data);
		}
		else {
			unsigned char* data = stbi_load(inputCameras[i].pathDepth.c_str(), &width, &height, &nrChannels, STBI_grey);
			if (!data) {
				std::cout << "Error: failed to load texture " << inputCameras[i].pathDepth << std::endl;
				return false;
			}
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, inputCameras[0].res_x, inputCameras[0].res_y, 0, GL_RED, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
	}
	return true;
}

void Application::SetupCUgraphicsResources() {
	ck(cuInit(0));

	CUdevice cuDevice = 0;
	ck(cuDeviceGet(&cuDevice, 0));
	char szDeviceName[80];
	ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
	std::cout << "GPU in use: " << szDeviceName << std::endl;
	ck(cuCtxCreate(cuContext, 0, cuDevice));

	ck(cuCtxSetCurrent(*cuContext));
	for (int i = 0; i < inputCameras.size(); i++) {
		// register OpenGL textures for Cuda interop
		CUgraphicsResource* glGraphicsResource_color = new CUgraphicsResource();
		CUgraphicsResource* glGraphicsResource_depth = new CUgraphicsResource();
		ck(cuGraphicsGLRegisterImage(glGraphicsResource_color, textures_color[i], GL_TEXTURE_2D, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
		ck(cuGraphicsGLRegisterImage(glGraphicsResource_depth, textures_depth[i], GL_TEXTURE_2D, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
		ck(cuGraphicsResourceSetMapFlags(*glGraphicsResource_color, CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD));
		ck(cuGraphicsResourceSetMapFlags(*glGraphicsResource_depth, CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD));
		glGraphicsResources.push_back(glGraphicsResource_color);
		glGraphicsResources.push_back(glGraphicsResource_depth);

		// initialize the LibAV demuxers
		FFmpegDemuxer* demuxer_color = new FFmpegDemuxer(inputCameras[i].pathColor.c_str(), i == 0);
		FFmpegDemuxer* demuxer_depth = new FFmpegDemuxer(inputCameras[i].pathDepth.c_str());
		demuxers.push_back(demuxer_color);
		demuxers.push_back(demuxer_depth);

		// initalize the Cuda Decoders
		NvDecoder* decoder_color = new NvDecoder(cuContext, glGraphicsResource_color, true, FFmpeg2NvCodecId(demuxer_color->GetVideoCodec()), i == 0);
		NvDecoder* decoder_depth = new NvDecoder(cuContext, glGraphicsResource_depth, false, FFmpeg2NvCodecId(demuxer_depth->GetVideoCodec()));
		decoders.push_back(decoder_color);
		decoders.push_back(decoder_depth);
	}
	ck(cuCtxPopCurrent(NULL));
}

bool Application::SetupDecodingPool() {

	if (options.StartingFrameNr > 0) {
		std::cout << "Decoding all frames up until frame " << options.StartingFrameNr << "..." << std::endl;
	}
	// decode until frame 'StartingFrameNr' of all input videos here
	for (int i = 0; i < demuxers.size(); i++) {
		for (int j = 0; j < options.StartingFrameNr + 2; j++) { // dev note: for some reason, demuxing and decoding needs to happen twice to get the first frame
			int nVideoBytes = 0;
			uint8_t* pVideo = NULL;
			if (!demuxers[i]->Demux(&pVideo, &nVideoBytes)) {
				std::cout << "Error: demuxing failed for input " << i / 2 << (i % 2 == 0 ? " color" : " depth") << std::endl;
				return false;
			}
			decoders[i]->Decode(pVideo, nVideoBytes);
		}
		// memcopy decoded image to CUGragpicsResources
		decoders[i]->HandlePictureDisplay(decoders[i]->picture_index);
	}

	if (!options.isStatic) {
		// setup thread pool to parallelize the decoding work
		pool.init((int)inputCameras.size(), demuxers, decoders, options.nrThreads);
		pool.startThreadPool();
		pool.startDemuxingFirstFrames(current_inputsToUse);
	}
		
	return true;
}

bool Application::RenderTarget(bool nextVideoFrame, bool updateCurrentInputsToUse)
{
	glEnable(GL_DEPTH_TEST);
	glViewport(0, 0, m_nRenderWidth, m_nRenderHeight);

	bool shouldUpdateUsedInputs = false;
	if (nextVideoFrame) {
		// recalculate which inputCameras need to be used for rendering the outputCamera
		next_inputsToUse = cameraVisibilityHelper.updateInputsToUse();
		for (auto& a : next_inputsToUse) {
			if (current_inputsToUse.find(a) == current_inputsToUse.end()) {
				shouldUpdateUsedInputs = true;
				break;
			}
		}
		/*for (int i : current_inputsToUse) {
			std::cout << i << " ";
		}
		std::cout << std::endl;*/
	}

	bool isFirstInput = true;
	shaders.updateOutputParams(pcOutputCamera);
	shaders.shader.setFloat("isFirstInput", 1.0f);
	for (int i = 0; i < inputCameras.size(); i++) {

		if ((!options.isStatic) && nextVideoFrame) {
			bool useForRenderingNextFrame = next_inputsToUse.find(i) != next_inputsToUse.end();
			pool.startDemuxingNextFrame(i, currentVideoFrame + 1, useForRenderingNextFrame);
		}
		bool useForRenderingCurrentFrame = current_inputsToUse.find(i) != current_inputsToUse.end();

		if (useForRenderingCurrentFrame) {

			shaders.updateInputParams(inputCameras[i]);

			if ((!options.isStatic) && nextVideoFrame) {
				std::tuple<int, int, int, int> tuple = pool.waitUntilInputFrameIsDecoded(i);
				pool.copyFromGPUToOpenGLTexture(std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple), std::get<3>(tuple));
			}

			RenderScene(i, isFirstInput);

			// prepare next iteration
			if (isFirstInput) {
				isFirstInput = false;
				shaders.shader.setFloat("isFirstInput", 0.0f);
			}
		}
	}

	if (nextVideoFrame) {
		currentVideoFrame++; // important for Pool
	}

	if (shouldUpdateUsedInputs) {
		current_inputsToUse.clear();
		for (auto& c : next_inputsToUse) {
			current_inputsToUse.insert(c); // deep copy
		}
	}

	return shouldUpdateUsedInputs;
}

void Application::RenderScene(int i, bool isFirstInput)
{
	if (isFirstInput) {
		// simple 3D warping
		framebuffers.renderTheFirstInputImage(0, textures_color[i], textures_depth[i]);
	}
	else {
		// copying between FBOs is necessary to prepare the blending
		shaders.copyShader.use();
		framebuffers.copyFramebuffer(0);

		// simple 3D warping + blending with the previous output image
		shaders.shader.use();
		framebuffers.renderNonFirstInputImage(0, textures_color[i], textures_depth[i]);
	}

}

void Application::RenderCompanionWindow()
{
	glDisable(GL_DEPTH_TEST);
	glViewport(0, 0, options.SCR_WIDTH, options.SCR_HEIGHT);

	glBindVertexArray(m_unCompanionWindowVAO);
	shaders.companionWindowShader.use();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, framebuffers.getColorTexture(0));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glDrawElements(GL_TRIANGLES, m_uiCompanionWindowIndexSize, GL_UNSIGNED_SHORT, 0);

	// draw input cameras
	if (options.showCameraVisibilityWindow) {
		glViewport(m_nRenderWidth - cameraVisibilityWindowWidth, 0, cameraVisibilityWindowWidth, cameraVisibilityWindowHeight);
		glEnable(GL_SCISSOR_TEST);
		glScissor(m_nRenderWidth - cameraVisibilityWindowWidth, 0, cameraVisibilityWindowWidth, cameraVisibilityWindowHeight);
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

void Application::SaveCompanionWindowToYUV(int frameNr, std::string outputCameraName) {
	unsigned char* image = new unsigned char[options.SCR_WIDTH * options.SCR_HEIGHT * 4];
	framebuffers.bindCurrentBuffer();
	glReadPixels(0, 0, options.SCR_WIDTH, options.SCR_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, image);
	if (options.usePNGs) {
		saveImage(image, options.SCR_WIDTH, options.SCR_HEIGHT, true, frameNr, options.outputPath + outputCameraName + ".png");
	}
	else {
		saveImage(image, options.SCR_WIDTH, options.SCR_HEIGHT, false, frameNr, options.outputPath + outputCameraName + ".yuv");
	}
	delete[] image;
	return;
}

#endif APPLICATION_H
