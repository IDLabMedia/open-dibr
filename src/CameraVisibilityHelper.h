#ifndef INPUTCAMERA_VISIBILITY_HELPER_H
#define INPUTCAMERA_VISIBILITY_HELPER_H


#include "ioHelper.h"
#include <unordered_set>

enum OutputCameraType {
	PERSPECTIVE, ERP180, ERP360
};

// CameraVisibilityWindow takes care of the window in the
// bottom right corner when --show_inputs is on the command line.
// The window renders the InputCameras and OutputCamera, with
// green InputCameras indicating that they are in Application::current_inputsToUse
class CameraVisibilityWindow {
private:
	glm::vec3 center = glm::vec3();
	float fov = 60.0f; 
	glm::mat4 projection = glm::mat4();

public:
	float radius = 3.0f;
	float angle = 0.0f;

	CameraVisibilityWindow() {}

	void init(int windowWidth, int windowHeight, std::vector<InputCamera> inputs) {
		projection = glm::perspective(glm::radians(fov), (float)windowWidth / (float)windowHeight, 0.01f, 500.0f);

		// initialize camera to look at center of inputs
		center = glm::vec3(0);
		float max_z = -1000.0f;
		for (auto& input : inputs) {
			center += input.pos;
			if (input.pos.z > max_z) {
				max_z = input.pos.z;
			}
		}
		center /= float(inputs.size());
		radius = 0;
		for (auto& input : inputs) {
			float r = glm::distance(center, input.pos);
			if (r > radius) {
				radius = r;
			}
		}
		radius *= 3.0f;

	}

	glm::mat4 ViewProject() {
		glm::vec3 cameraPos = center + radius * glm::vec3(sin(angle), 0, cos(angle));
		glm::mat4 view = glm::lookAt(cameraPos, center, glm::vec3(0.0f, 1.0f, 0.0f));
		return projection * view;
	}
};

// Takes care of which InputCameras need to be used
// to contruct the output image based on the
// position and rotation of the OutputCamera
class CameraVisibilityHelper {
private:
	std::vector<InputCamera> inputCameras;
	OutputCamera* outputCamera = NULL;
	int maxNrInputsUsed = 0;
	std::unordered_set<int> inputsToUse; // indices of InputCameras to be used to render the next output image
	std::vector<glm::vec4> pointsThatShouldBeSeen;

public:
	CameraVisibilityHelper() {}

	void init(std::vector<InputCamera> inputCameras, OutputCamera* outputCamera, int maxNrInputsUsed) {
		this->inputCameras = inputCameras;
		this->outputCamera = outputCamera;
		this->maxNrInputsUsed = maxNrInputsUsed;

		if (inputCameras.size() <= maxNrInputsUsed) {
			// All InputCameras can be used
			for (int i = 0; i < inputCameras.size(); i++) {
				inputsToUse.insert(i);
			}
		}
		else {
			// Only a subset of all InputCameras can be used.
			// calculatePointsThatShouldBeSeen() prepares for updateInputsToUsePerspective()
			calculatePointsThatShouldBeSeen(inputCameras[0].z_near + 3.0f, outputCamera->FOV_x, outputCamera->FOV_y);
		}
	}

	std::unordered_set<int> updateInputsToUse() {
		if (inputCameras.size() <= maxNrInputsUsed) {
			return inputsToUse;
		}
		if (inputCameras[0].projection == Projection::Equirectangular && inputCameras[0].hor_range.y - inputCameras[0].hor_range.x > 3.14f) {
			// 360 degree cameras see everything, so make a choice based on closest InputCamera
			updateInputsToUseByDistance();
		}
		else {
			updateInputsToUseByViewingAngles();
		}
		return inputsToUse;
	}

private:
	void calculatePointsThatShouldBeSeen(float depth, float FOV_x, float FOV_y) {
		// Here we define 5 points in the axial system of the output camera,
		// all at zdepth = depth:
		// 1 point in the middle of the image, i.e. center of the view
		// 1 point in each corner of the image, i.e. the extrimities of the view
		// The goal is to find InputCameras so that each point is seen.
		// Why? To minimize disocclusions, i.e. large areas in the output image
		// that none of the inputsToUse see

		float x_right = std::tan(FOV_x / 2.0f);
		float x_left = -x_right;
		float y_top = std::tan(FOV_y / 2.0f);
		float y_bottom = -y_top;

		pointsThatShouldBeSeen.clear();
		pointsThatShouldBeSeen.push_back(glm::vec4(0, 0, -depth, 1)); // forward
		pointsThatShouldBeSeen.push_back(glm::vec4(x_left * depth, y_top * depth, -depth, 1));     // top left
		pointsThatShouldBeSeen.push_back(glm::vec4(x_right * depth, y_bottom * depth, -depth, 1)); // bottom right 
		pointsThatShouldBeSeen.push_back(glm::vec4(x_right * depth, y_top * depth, -depth, 1));    // top right
		pointsThatShouldBeSeen.push_back(glm::vec4(x_left * depth, y_bottom * depth, -depth, 1));  // bottom left
	}

	// the closer the returned float is to 1, the closer the input is to seeing the point
	// returns a value in [0,1]
	float inputCameraSeesPoint(InputCamera input, glm::vec3 point) {
		// get the position of P in the axial system of the InputCamera
		glm::vec3 IP = glm::vec3(input.view * glm::vec4(point, 1));

		// --- PERSPECTIVE ---
		if (input.projection == Projection::Perspective) {
			// check if P is behind the near plane of the InputCamera
			if (IP.z > -0.1f) {
				//return false;
				return 0.0f;
			}
			// In essence, we will project P onto the image plane of the InputCamera
			// If the projection is not in [-1,1], P is outside the image.
			// The heuristics will be 1 if the projection is inside the image or
			// will go to zero the further the projection is from the edge of the image.
			IP = -IP / IP.z;
			float heuristicx = 1;
			float heuristicy = 1;

			if (IP.y < -(input.res_y - input.principal_point_y) / input.focal_y) {
				heuristicy = -(input.res_y - input.principal_point_y) / input.focal_y / IP.y;
			}
			else if (IP.y > input.principal_point_y / input.focal_y) {
				heuristicy = input.principal_point_y / input.focal_y / IP.y;
			}
			if (IP.x < -input.principal_point_x / input.focal_x) {
				heuristicx = -input.principal_point_x / input.focal_x / IP.x;
			}
			else if (IP.x > (input.res_x - input.principal_point_x) / input.focal_x) {
				heuristicx = (input.res_x - input.principal_point_x) / input.focal_x / IP.x;
			}
			return (heuristicx + heuristicy) / 2.0f;
		}
		// --- EQUIRECTANGULAR ---
		else if (input.projection == Projection::Equirectangular) {
			float angle = dot(glm::normalize(IP), glm::vec3(0, 0, -1)); // actually cosine of angle
			if (angle < 0) {
				return 1 + angle;
			}
			return 1;
		}
		// --- FISHEYE_EQUIDISTANT ---
		else {
			float angle = dot(glm::normalize(IP), glm::vec3(0, 0, -1)); // actually cosine of angle
			float fov_angle = cos(input.fov);
			if (angle < fov_angle) {
				return 1 + angle - fov_angle;
			}
			return 1;
		}
		return 1;
	}

	void updateInputsToUseByViewingAngles() {


		inputsToUse.clear();
		// fill anglesToForwardPoint with the (cosine of the) angle between vectors PO and PI
		// with P = forward point pointsThatShouldBeSeen[0], O = OutputCamera, I = InputCamera
		std::vector<std::tuple<float, int>> anglesToForwardPoint;
		glm::vec3 P = outputCamera->model * pointsThatShouldBeSeen[0];
		glm::vec3 PO = glm::normalize(glm::vec3(outputCamera->model[3]) - P);
		for (int i = 0; i < inputCameras.size(); i++) {
			// check if point lies in the field of view of the input camera
			if (inputCameraSeesPoint(inputCameras[i], P) > 0.99f) {
				glm::vec3 PI = glm::normalize(inputCameras[i].pos - P);
				float angle = acos(std::min(dot(PO, PI), 1.0f));
				anglesToForwardPoint.push_back(std::tuple<float, int>(angle, i));
			}
			else {
				// default the (cosine of the) angle to 1
				anglesToForwardPoint.push_back(std::tuple<float, int>(1.0f, i));
			}

		}

		// sort anglesToForwardPoint from smallest angle to largest
		std::stable_sort(anglesToForwardPoint.begin(), anglesToForwardPoint.end(), [](std::tuple<float, int> a, std::tuple<float, int> b) {
			return std::get<0>(a) < std::get<0>(b);
		});

		// for each corner (i.e. the rest of pointsThatShouldBeSeen), try to find an InputCamera that sees it
		for (int i = 1; i < pointsThatShouldBeSeen.size(); i++) {
			float best_heuristic = 0.0f;
			int best_index = -1;
			bool foundInputCameraThatSeesP = false;
			glm::vec3 P = outputCamera->model * pointsThatShouldBeSeen[i];
			for (auto& angle_index_tuple : anglesToForwardPoint) {
				float heuristic = inputCameraSeesPoint(inputCameras[std::get<1>(angle_index_tuple)], P);
				if (heuristic == 1) {
					inputsToUse.insert(std::get<1>(angle_index_tuple));
					foundInputCameraThatSeesP = true;
					break;
				}
				if (heuristic > best_heuristic + 0.0001f) {
					best_heuristic = heuristic;
					best_index = std::get<1>(angle_index_tuple);
				}
			}


			if (!foundInputCameraThatSeesP && best_index != -1) {
				// use the InputCamera that is closest to beeing able to see the point
				inputsToUse.insert(best_index);
			}
			if (inputsToUse.size() == maxNrInputsUsed) {
				break;
			}
		}

		// now, inputsToUse can have up to 4 indices of InputCameras to use to render the output
		// so if inputsToUse.size() < maxNrInputsUsed, we can add some more
		int i = 0;
		while (inputsToUse.size() < maxNrInputsUsed) {
			// unordered_set will not store duplicates
			inputsToUse.insert(std::get<1>(anglesToForwardPoint[i]));
			i++;
		}
	}

	void updateInputsToUseByDistance() {
		inputsToUse.clear();
		std::vector<int> indices(inputCameras.size());
		std::iota(indices.begin(), indices.end(), 0); // init indices = {0, 1, 2, ..., N}
		// sort indices from smallest to largest distance between InputCamera[index] and OutputCamera
		std::sort(indices.begin(), indices.end(), [this](int a, int b) {
			glm::vec3 outputPos = glm::vec3(outputCamera->model[3]);
			return glm::length(inputCameras[a].pos - outputPos) < glm::length(inputCameras[b].pos - outputPos);
		});
		for (int i = 0; i < maxNrInputsUsed; i++) {
			inputsToUse.insert(indices[i]);
		}
	}
};

#endif
