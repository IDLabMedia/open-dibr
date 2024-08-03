#ifndef IOHELPER_H
#define IOHELPER_H


#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <gtx/string_cast.hpp>

#include <fstream>
#include <iostream>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "json.hpp"

/*
* InputCamera and OutputCamera are used to store camera intrinsics and extrinsics.
* readJson(): processes a .json file containing camera intrinsics and extrinsics
*             and stores the information as InputCamera objects.
*/

void omafToOpenGLPosition(glm::vec3& v) {
	v = glm::vec3(-v[1], v[2], -v[0]);
}

void omafToOpenGLRotation(glm::vec3& r) {
	r = glm::vec3(-r[1], r[0], -r[2]);
}

void colmapToOpenGLPosition(glm::vec3& v) {
	v = glm::vec3(v[0], -v[1], -v[2]);
}

void colmapToOpenGLRotation(glm::vec3& r) {
	r = glm::vec3(r[0], -r[1], -r[2]);
}

enum class AxialSystem {
	Omaf,
	Colmap,
	OpenGL,
};

enum class Projection {
	Perspective,
	Equirectangular,
	Fisheye_equidistant,
};

class InputCamera {
public:
	std::string pathColor;
	std::string pathDepth;
	glm::vec3 pos = glm::vec3();
	glm::vec3 rot = glm::vec3();
	glm::mat4 model = glm::mat4();
	glm::mat4 view = glm::mat4();
	int res_x = 0;
	int res_y = 0;
	float z_near = 0;
	float z_far = 0;
	unsigned int bitdepth_depth = 0;
	unsigned int bitdepth_color = 0;
	
	Projection projection = Projection::Perspective;

	// in case of perspective projection:
	float focal_x = 0;
	float focal_y = 0;
	float principal_point_x = 0;
	float principal_point_y = 0;
	// in case of equirectangular projection:
	glm::vec2 hor_range = glm::vec2();  // vector of 2 angles indicating the horizontal viewing angle (in radians)
	glm::vec2 ver_range = glm::vec2();  // vector of 2 angles indicating the vertical viewing angle (in radians)
	// in case of fisheye equidistant projection:
	float fov = 0;		   // in radians

	InputCamera() {}

	InputCamera(nlohmann::json params, std::string directory, AxialSystem axialSystem) {
		std::string k = ""; // useful for error handling
		try {
			k = "NameColor";
			if (params[k].get<std::string>() != "viewport") {
				pathColor = directory + params[k].get<std::string>();
				k = "NameDepth";
				pathDepth = directory + params[k].get<std::string>();
			}
			k = "Position";
			pos = glm::vec3(
				(float)params[k][0],
				(float)params[k][1],
				(float)params[k][2]
			);
			k = "Rotation";
			rot = glm::vec3(
				(float)params[k][0],
				(float)params[k][1],
				(float)params[k][2]
			);
			rot = glm::radians(rot);

			switch (axialSystem) {
			case AxialSystem::Omaf:
				omafToOpenGLPosition(/*out*/pos);
				omafToOpenGLRotation(/*out*/rot);
				break;
			case AxialSystem::Colmap:
				colmapToOpenGLPosition(/*out*/pos);
				colmapToOpenGLRotation(/*out*/rot);
				break;
			default:
				break;
			}

			k = "Projection";
			if (params[k] == "Perspective") {
				projection = Projection::Perspective;
			}
			else if (params[k] == "Equirectangular") {
				projection = Projection::Equirectangular;
			}
			else if (params[k] == "Fisheye_Equidistant") {
				projection = Projection::Fisheye_equidistant;
			}
			else {
				std::cout << "Error: unexpected projection " << params["Projection"] << " in JSON file for camera " << pathColor << ". Needs to be in [Perspective, Equirectangular, Fisheye_Equidistant]." << std::endl;
				exit(-1);
			}

			glm::mat4 inputTranslation = glm::translate(glm::mat4(1.0f), pos);
			glm::mat4 inputRx = glm::rotate(glm::mat4(1.0f), rot[0], glm::vec3(1.0f, 0.0f, 0.0f));
			glm::mat4 inputRy = glm::rotate(glm::mat4(1.0f), rot[1], glm::vec3(0.0f, 1.0f, 0.0f));
			glm::mat4 inputRz = glm::rotate(glm::mat4(1.0f), rot[2], glm::vec3(0.0f, 0.0f, 1.0f));
			glm::mat4 rotMat = inputRz * inputRy * inputRx;
			model = inputTranslation * rotMat;
			view = glm::inverse(model);

			k = "Resolution";
			res_x = (int)params[k][0];
			res_y = (int)params[k][1];
			k = "Depth_range";
			z_near = (float)params[k][0];
			z_far =  (float)params[k][1];
			k = "BitDepthDepth";
			bitdepth_depth = (unsigned int)params[k];
			k = "BitDepthColor";
			bitdepth_color = (unsigned int)params[k];

			if (bitdepth_color < 8 || bitdepth_color > 16) {
				std::cout << "Error: BitDepthColor = " << bitdepth_color << " for input camera " << pathColor << " should lie in [8,16]" << std::endl;
				exit(-1);
			}
			if (bitdepth_depth < 8 || bitdepth_depth > 16) {
				std::cout << "Error: BitDepthDepth = " << bitdepth_depth << " for input camera " << pathDepth << " should lie in [8,16]" << std::endl;
				exit(-1);
			}

			if (projection == Projection::Perspective) {
				k = "Focal";
				focal_x = (float)params[k][0];
				focal_y = (float)params[k][1];
				k = "Principle_point";
				principal_point_x = (float)params[k][0];
				principal_point_y = (float)params[k][1];

				hor_range = glm::vec2();
				ver_range = glm::vec2();
			}
			else if (projection == Projection::Equirectangular) {
				k = "Hor_range";
				hor_range = glm::vec2(
					(float)params[k][0],
					(float)params[k][1]
				);
				k = "Ver_range";
				ver_range = glm::vec2(
					(float)params[k][0],
					(float)params[k][1]
				);
				hor_range = glm::radians(hor_range);
				ver_range = glm::radians(ver_range);
			}
			else {
				hor_range = glm::vec2();
				ver_range = glm::vec2();
				k = "Fov";
				fov = (float)params[k];
				fov = glm::radians(fov);
			}
		}
		catch (nlohmann::json::exception e) {
			std::cout << "Error: error while parsing key \"" << k << "\" in the JSON file";
			if (k != "NameColor") {
				std::cout << " for camera " << params["NameColor"].get<std::string>();
			}
			std::cout << std::endl;
			exit(-1);
		}
	}
};

class OutputCamera {
public:
	std::string name;
	glm::vec3 pos = glm::vec3(0);
	glm::vec3 rot = glm::vec3(0);
	glm::mat4 startPosMat = glm::mat4(1);
	glm::mat4 startRotMat = glm::mat4(1);
	glm::mat4 startModel = glm::mat4(1);
	glm::mat4 model = glm::mat4(1);
	glm::mat4 view = glm::mat4(1);
	bool isVR = false;
	glm::mat4 projectionLeft = glm::mat4();
	glm::mat4 projectionRight = glm::mat4();
	int res_x = 0;
	int res_y = 0;
	Projection projection = Projection::Perspective;
	float focal_x = 0;
	float focal_y = 0;
	float principal_point_x = 0;
	float principal_point_y = 0;
	float z_near = 0;
	float z_far = 0;
	float FOV_x = 0; // radians
	float FOV_y = 0; // radians

	OutputCamera() {}

	// only used for VR mode
	OutputCamera(int width, int height, OutputCamera viewport, glm::mat4 projectionLeft, glm::mat4 projectionRight, float FOV_x, float FOV_y, float z_near, float z_far)
		: res_x(width)
		, res_y(height)
		, projectionLeft(projectionLeft)
		, projectionRight(projectionRight)
		, FOV_x(FOV_x)
		, FOV_y(FOV_y)
		, z_near(z_near)
		, z_far(z_far){
		isVR = true;
		pos = viewport.pos;
		rot = viewport.rot;
		startPosMat = viewport.startPosMat;
		startRotMat = viewport.startRotMat;
		startModel = viewport.startModel;
		model = viewport.model;
		view = viewport.view;
		projection = viewport.projection;
	}

	OutputCamera(nlohmann::json params, AxialSystem axialSystem) {
		std::string k = ""; // useful for error handling
		try {
			k = "NameColor";
			name = params[k].get<std::string>();
			k = "Position";
			pos = glm::vec3(
				(float)params[k][0],
				(float)params[k][1],
				(float)params[k][2]
			);
			
			k = "Rotation";
			rot = glm::vec3(
				(float)params[k][0],
				(float)params[k][1],
				(float)params[k][2]
			);
			rot = glm::radians(rot);

			switch (axialSystem) {
			case AxialSystem::Omaf:
				omafToOpenGLPosition(/*out*/pos);
				omafToOpenGLRotation(/*out*/rot);
				break;
			case AxialSystem::Colmap:
				colmapToOpenGLPosition(/*out*/pos);
				colmapToOpenGLRotation(/*out*/rot);
				break;
			default:
				break;
			}

			k = "Projection";
			if(params[k] != "Perspective") {
				std::cout << "Error: unexpected projection " << params[k] << " for an OutputCamera in json file, should be Perspective" << std::endl;
				return;
			}

			startPosMat = glm::translate(glm::mat4(1.0f), pos);
			glm::mat4 inputRx = glm::rotate(glm::mat4(1.0f), rot[0], glm::vec3(1.0f, 0.0f, 0.0f));
			glm::mat4 inputRy = glm::rotate(glm::mat4(1.0f), rot[1], glm::vec3(0.0f, 1.0f, 0.0f));
			glm::mat4 inputRz = glm::rotate(glm::mat4(1.0f), rot[2], glm::vec3(0.0f, 0.0f, 1.0f));
			startRotMat = inputRz * inputRy * inputRx;
			startModel = startPosMat * startRotMat;
			model = startModel;
			view = glm::inverse(model);

			k = "Resolution";
			res_x = (int)params[k][0];
			res_y = (int)params[k][1];
			k = "Focal";
			focal_x = (float)params[k][0];
			focal_y = (float)params[k][1];
			k = "Principle_point";
			principal_point_x = (float)params[k][0];
			principal_point_y = (float)params[k][1];

			k = "Depth_range";
			if (params.contains(k)) {
				z_near = (float)params[k][0];
				z_far = (float)params[k][1];
			}
			else {
				z_near = 0.1f;
				z_far = 1020.0f;
			}
		}
		catch (nlohmann::json::exception e) {
			std::cout << "Error: error while parsing key \"" << k << "\" in the JSON file";
			if (k != "NameColor") {
				std::cout << " for camera " << params["NameColor"].get<std::string>();
			}
			std::cout << std::endl;
			exit(-1);
		}
		FOV_x = 2.0f * atan(res_x / 2.0f / focal_x);
		FOV_y = 2.0f * atan(res_y / 2.0f / focal_y);
	}
};

bool readInputJson(std::string inputJsonPath, std::string directory, bool findViewport, /*out*/ OutputCamera& viewport, std::vector<InputCamera>& inputCameras) {
	std::ifstream file;
	file.open(inputJsonPath, std::ios::in);
	if (!file){
		std::cout << "Error: failed to open JSON file " << inputJsonPath << std::endl;
		return false;
	}

	// parse json file
	nlohmann::json j;
	try {
		file >> j;
	}
	catch (nlohmann::json::parse_error& e) {
		std::cout << e.what() << std::endl;
		std::cout << "Error: failed to parse JSON file " << inputJsonPath << ". Check for syntax errors." << std::endl;
		file.close();
		return false;
	}
	file.close();

	if (!j.contains("Axial_system")) {
		std::cout << "Error: the input JSON file should contain a key \"Axial_system\" with a value of \"OMAF\", \"COLMAP\" or \"OPENGL\"" << std::endl;
		return false;
	}
	std::string axialsSystemString = j["Axial_system"].get<std::string>();
	AxialSystem axialSystem = AxialSystem::Omaf;
	if(axialsSystemString == "OMAF") {
		axialSystem = AxialSystem::Omaf;
	}
	else if (axialsSystemString == "COLMAP") {
		axialSystem = AxialSystem::Colmap;
	}
	else if (axialsSystemString == "OPENGL") {
		axialSystem = AxialSystem::OpenGL;
	}
	else {
		std::cout << "Error: in input JSON, invalid value for key \"Axial_system\": should be one of \"OMAF\", \"COLMAP\" or \"OPENGL\", but got \""<< axialsSystemString << "\"" << std::endl;
		return false;
	}

	if (!j.contains("cameras")) {
		std::cout << "Error: the input JSON file should contain a key \"cameras\" with a list of cameras as value." << std::endl;
		return false;
	}

	// make an InputCamera object for each camera in the json
	bool foundViewport = false;
	for (int i = 0; i < j["cameras"].size(); i++) {
		std::string name = "";
		try {
			name = j["cameras"][i]["NameColor"].get<std::string>();
		}
		catch (const std::exception& e){
			std::cout << "Error: failed to read \"NameColor\" for camera "<< i+1 <<" in the JSON file." << std::endl;
			return false;
		}
		if (name == "viewport") {
			if (findViewport) {
				viewport = OutputCamera(j["cameras"][i], axialSystem);
			}
			foundViewport = true;
		}
		else {
			inputCameras.push_back(InputCamera(j["cameras"][i], directory, axialSystem));
		}
	}
	j.clear();

	if (findViewport && !foundViewport) {
		std::cout << "The input JSON file should contain a camera named \'viewport\'." << std::endl;
		return false;
	}

	return true;
}

bool readOutputJson(std::string inputJsonPath, /*out*/ std::vector<OutputCamera>& outputCameras, int &outputStartFrame, int &outputNrFrames) {
	std::ifstream file;
	file.open(inputJsonPath, std::ios::in);
	if (!file) {
		std::cout << "Error: failed to open JSON file " << inputJsonPath << std::endl;
		return false;
	}

	// parse json file
	nlohmann::json j;
	try {
		file >> j;
	}
	catch (nlohmann::json::parse_error & e) {
		std::cout << e.what() << std::endl;
		std::cout << "Error: failed to parse JSON file " << inputJsonPath << ". Check for syntax errors." << std::endl;
		file.close();
		return false;
	}
	file.close();

	std::vector<std::string> keys = {"Start_frame", "Number_of_frames", "cameras"};
	for (std::string key : keys) {
		if (!j.contains(key)) {
			std::cout << "Error: the output JSON file should contain a key \""<< key << "\"" << std::endl;
			return false;
		}
	}

	outputStartFrame = (int)j["Start_frame"];
	outputNrFrames = (int)j["Number_of_frames"];

	if (outputStartFrame < 0 || outputNrFrames < 0) {
		std::cout << "Error: \"Start_frame\" and \"Number_of_frames\" should be equal to or greater than 0." << std::endl;
		return false;
	}
	if (outputStartFrame >= outputNrFrames) {
		std::cout << "Error: \"Start_frame\" should be smaller than \"Number_of_frames\"." << std::endl;
		return false;
	}

	if (!j.contains("Axial_system")) {
		std::cout << "Error: the output JSON file should contain a key \"Axial_system\" with a value of \"OMAF\", \"COLMAP\" or \"OPENGL\"" << std::endl;
		return false;
	}
	std::string axialsSystemString = j["Axial_system"].get<std::string>();
	AxialSystem axialSystem = AxialSystem::Omaf;
	if (axialsSystemString == "OMAF") {
		axialSystem = AxialSystem::Omaf;
	}
	else if (axialsSystemString == "COLMAP") {
		axialSystem = AxialSystem::Colmap;
	}
	else if (axialsSystemString == "OPENGL") {
		axialSystem = AxialSystem::OpenGL;
	}
	else {
		std::cout << "Error: in output JSON, invalid value for key \"Axial_system\": should be one of \"OMAF\", \"COLMAP\" or \"OPENGL\", but got \"" << axialsSystemString << "\"" << std::endl;
		return false;
	}

	if (!j.contains("cameras")) {
		std::cout << "Error: the input JSON file should contain a key \"cameras\" with a list of cameras as value." << std::endl;
		return false;
	}

	// make an OutputCamera object for each camera in the json
	for (int i = 0; i < j["cameras"].size(); i++) {
		outputCameras.push_back(OutputCamera(j["cameras"][i], axialSystem));
	}
	j.clear();
	return true;
}

void saveImage(unsigned char* image, int width, int height, bool saveAsPNG, int frameNr, std::string outputPath) {

	if (saveAsPNG) {
		try {
			stbi_flip_vertically_on_write(1);
			stbi_write_png(outputPath.c_str(), width, height, 4, image, width * 4);
			std::cout << "wrote PNG to " << outputPath << std::endl;
		}
		catch (const std::exception & e) {
			std::cout << "could not write to " << outputPath << std::endl;
			throw e;
		}
	}
	else {
		unsigned char* y = new unsigned char[width * height];
		unsigned char* cb = new unsigned char[width * height];
		unsigned char* cr = new unsigned char[width * height];
		for (int row = 0; row < height; row++) {
			for (int col = 0; col < width; col++) {
				y[row * width + col] = image[(height - row - 1) * width * 4 + col * 4];
				cb[row * width + col] = image[(height - row - 1) * width * 4 + col * 4 + 1];
				cr[row * width + col] = image[(height - row - 1) * width * 4 + col * 4 + 2];
			}
		}

		std::cout << "writing YUV444p frame " << frameNr << " to " << outputPath << std::endl;
		std::fstream stream(outputPath, frameNr == 0 ? std::ios::out | std::ios::binary : std::ios::in | std::ios::out | std::ios::binary);
		if (stream.good()) {
			stream.seekp(frameNr * 3 * width * height);
			stream.write(reinterpret_cast<char*>(y), width* height);
			stream.write(reinterpret_cast<char*>(cb), width* height);
			stream.write(reinterpret_cast<char*>(cr), width* height);
		}
		else {
			std::cout << "could not write to " << outputPath << std::endl;
		}

		delete[] y;
		delete[] cb;
		delete[] cr;
	}
	return;
}


#endif

