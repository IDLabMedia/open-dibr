#ifndef CXXOPTS_NO_EXCEPTIONS
#define CXXOPTS_NO_EXCEPTIONS
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <glm.hpp>
#include <map>
#include <string>
#include "cxxopts.hpp"
#include "ioHelper.h"
#include "AppDecUtils.h"


// From CMAKE preprocessor
std::string cmakelists_dir = CMAKELISTS_SOURCE_DIR;


class Options {
public:
	// all Option members are set through the command line args

	std::string inputPath;             // path to folder that contains the light field dataset
	std::string inputJsonPath;         // path to the .json with the input light field camera parameters
	std::string outputPath = "";       // path to folder to write the .yuv files with the output videos
	std::string outputJsonPath = "";   // path to the .json with the output light field camera parameters  
	std::string fpsCsvPath = "";       // .csv file to where the milliseconds each frame takes to render are written

	std::vector<InputCamera> inputCameras;
	OutputCamera viewport;
	std::vector<OutputCamera> outputCameras;

	unsigned int SCR_WIDTH = 1920;                    // width in pixels of the SDL window
	unsigned int SCR_HEIGHT = 1080;                   // height in pixels of the SDL window

	glm::vec3 backgroundColor = glm::vec3(0.5f, 0.5f, 0.5f); // glClearColor
	float cameraSpeed = 0.01f;

	bool saveOutputImages = false;  // if true, the output cameras from the "inputJsonPath" file are rendered one by one and the results are saved to disk as .yuv files
	int outputNrFrames = 1;
	int StartingFrameNr = 0;        // the number of the video frame that will be shown first
	
	bool useVR = false;

	bool usePNGs = false;           // if true, use png files as input for color and depth instead of mp4 videos
	bool isStatic = false;          // if true, stops decoding after frame StartingFrameNr
	
	int nrThreads = 2;              // the number of threads in the thread pool. Only useful if isStatic == false.
	int triangleSizeInPixels = 1;   // the resolution of the triangle mesh (1 is best, 2 is 4 times less triangles, etc. 
	int maxNrInputsUsed = -1;       // determine the upper limit of inputs that can be used at the same time
	int blendingFactor = 0;         // the higher, the more blending there is between input color images
	bool showCameraVisibilityWindow = false;
	
	int targetFps = 90;
	bool useFpsMonitor = false;
	bool asap = false;              // this will (decode and) play the video frames as fast as possible

	// some tunable shader uniforms:
	float triangle_deletion_margin = 10.0f;        // used in geometry shader for the threshold for stretched triangle deletion
	float depth_diff_threshold_fragment = 0.05f;   // threshold used in geometry shader to determine which elongated triangles should be deleted
												   // for now fixed at 0.05m
	float image_border_threshold_fragment = 0.0f;  // used in fragment shader, expressed in nr pixels
												   // determines the width of the border along the input images where pixels are blended


public:

	Options(){}

	Options(int argc, char* argv[]) {

		cxxopts::Options options("OpenDIBR", "A real-time depth-image-based renderer");
		options.add_options()
			("h,help", "Print help")
			;
		options.add_options("Input videos/images")
			("i,input_dir", "Path to the folder that contains the light field images/videos", cxxopts::value<std::string>())
			("j,input_json", "Path to the .json file with the input light field camera parameters", cxxopts::value<std::string>())
			;
		options.add_options("VR")
			("vr", "Render the output to a VR headset")
			;
		options.add_options("Dynamic vs. static")
			("static", "The input light field consists of PNGs, or of videos where only the \'--framenr\' frame needs to be decoded")
			("frame_nr", "The frame that needs to be shown if the input light field consists of videos and option \'--static\' is set", cxxopts::value<int>()->default_value("0"))
			;
		options.add_options("Settings to improve performance")
			("t", "Number of threads for the thread pool that decodes the videos. Should be >= 2. Recommended: #CPUcores - 1", cxxopts::value<int>()->default_value("2"))
			("asap", "Decode and play the image/video frames as soon as possible (basically disabling the Vsync@90Hz)")
			("max_nr_inputs", "The maximum number of input images/videos that will be processed per frame (-1 if all need to be processed)", cxxopts::value<int>()->default_value("-1"))
			("show_inputs", "This setting will display the positions and rotations of the input and output cameras on screen, as well as which inputs are used to render the current frame.")
			("mesh_subdivisions", "The detail level of the triangle meshes, full resolution if 0, 1/2 resolution if 1, 1/3 resolution if 2, etc. Must lie in [0,5]", cxxopts::value<int>()->default_value("0"))
			("target_fps", "The target application fps in case of video inputs. Needs to be a multiple of 30, which is the assumed framerate of the videos.", cxxopts::value<int>()->default_value("90"))
			;
		options.add_options("Saving to disk")
			// save to disk
			("p,output_json", "Path to the .json file with the camera parameters for which the output image needs to be saved to disk", cxxopts::value<std::string>())
			("o,output_dir", "Path to the folder where the output will be saved", cxxopts::value<std::string>())
			("fps_csv", "Path to the .csv file to write the time needed to render each frame to", cxxopts::value<std::string>())
			;
		options.add_options("Settings to improve quality")
			("blending_factor", "The higher this factor, the more blending between inputs there is, as an int in [0,10]", cxxopts::value<int>()->default_value("1"))
			("triangle_deletion_margin", "The higher this value, the less strict the threshold for deletion of stretched triangles.", cxxopts::value<float>()->default_value("10.0"))
			;
		options.add_options("Output camera settings")
			// output camera
			("background", "The RGB color of the background, as 3 ints in [0,255] (default: 128,128,128)", cxxopts::value<std::vector<int>>())
			("cam_speed", "The speed at which the GUI camera moves around (default: 0.01f)", cxxopts::value<float>()->default_value("0.01f"))
			;

		cxxopts::ParseResult result = options.parse(argc, argv);
		// print help if necessary
		if (argc < 2 || result.count("help"))
		{
			std::cout << options.help({ "Input videos/images" , "VR", "Dynamic vs. static", "Saving to disk", "Settings to improve quality", "Settings to improve performance" , "Output camera settings" }) << std::endl;
			exit(0);
		}
		// filter out common errors in the user - provided files and paths
		if (!inputAndOutputFilesOK(result)) {
			exit(-1);
		}
		
		if (result.count("background")) {
			std::vector<int> b = result["background"].as<std::vector<int>>();
			if (b.size() > 0 && b.size() != 3) {
				std::cout << "Error: -b or --background needs to be followed by 3 ints, e.g. \"-b 128 128 128\"" << std::endl;
				exit(-1);
			}
			else if (b.size() == 3) {
				backgroundColor = glm::vec3(b[0] / 255.0f, b[1] / 255.0f, b[2] / 255.0f);
			}
			if (backgroundColor.x < 0 || backgroundColor.x > 1 || backgroundColor.y < 0 || backgroundColor.y > 1 || backgroundColor.z < 0 || backgroundColor.z > 1) {
				std::cout << "Error: -b or --background needs to be followed by 3 ints that lie within [0,255]" << std::endl;
				exit(-1);
			}
		}

		if (result.count("cam_speed")) {
			cameraSpeed = result["cam_speed"].as<float>();
			if (cameraSpeed <= 0) {
				std::cout << "Error: --cam_speed needs to be > 0" << std::endl;
				exit(-1);
			}
		}

		if (result.count("vr")) {
			if (saveOutputImages) {
				std::cout << "Error: cannot use VR mode (--vr) if the output needs to be saved to disk. "
					<< "Either remove options - o/--output_dir and -p/--output_json or remove --vr from the command line." << std::endl;
				exit(-1);
			}
			useVR = true;
		}
		if (usePNGs || result.count("static")) {
			isStatic = true;
			outputNrFrames = 1;
		}
		if (result.count("frame_nr") && result["frame_nr"].as<int>() != 0) {
			if (usePNGs) {
				std::cout << "Option --frame_nr " << result["frame_nr"].as<int>() << " will be ignored since the inputs are png files, i.e. there is only one frame per input" << std::endl;
			}
			else if (!result.count("static")) {
				std::cout << "Option --frame_nr " << result["frame_nr"].as<int>() << " will be ignored if --static is not defined on the command line" << std::endl;
			}
			StartingFrameNr = result["frame_nr"].as<int>();
		}

		if (result.count("max_nr_inputs")) {
			maxNrInputsUsed = result["max_nr_inputs"].as<int>();
			if (maxNrInputsUsed < 1) {
				maxNrInputsUsed = (int)inputCameras.size();
			}
			std::cout << "max_nr_inputs set to " << maxNrInputsUsed << std::endl;
		}
		if (result.count("show_inputs")) {
			showCameraVisibilityWindow = true;
		}
		
		if (result.count("mesh_subdivisions")) {
			int mesh_subdivisions = result["mesh_subdivisions"].as<int>();
			if (mesh_subdivisions < 0 || mesh_subdivisions > 5) {
				std::cout << "Option --mesh_subdivisions should be an int in [0,5]" << std::endl;
				exit(-1);
			}
			triangleSizeInPixels = mesh_subdivisions + 1;
			if (SCR_WIDTH % triangleSizeInPixels != 0 || SCR_HEIGHT % triangleSizeInPixels != 0) {
				std::cout << "Error: the width (="<< SCR_WIDTH << ") and height (=" << SCR_HEIGHT << ") of the output camera need to be divisible by mesh_subdivisions+1 (=" << triangleSizeInPixels << ")" << std::endl;
				exit(-1);
			}
		}
		if (result.count("blending_factor")) {
			blendingFactor = result["blending_factor"].as<int>();
			if (blendingFactor < 0 || blendingFactor > 10) {
				std::cout << "Option --blending_factor should be an int in [0,10]" << std::endl;
				exit(-1);
			}
		}
		if (result.count("triangle_deletion_margin")) {
			triangle_deletion_margin = result["triangle_deletion_margin"].as<float>();
			if (triangle_deletion_margin < 1) {
				std::cout << "Option --triangle_deletion_margin should be at least 1" << std::endl;
				exit(-1);
			}
		}
		//target_fps
		if (result.count("target_fps")) {
			targetFps = result["target_fps"].as<int>();
			if (targetFps < 30 || (targetFps % 30 != 0)) {
				std::cout << "Option --targetFps should be a multiple of 30" << std::endl;
				exit(-1);
			}
		}

		if (result.count("t")) {
			if (isStatic) {
				std::cout << "Option --t is ignored when the input dataset contains PNGs or --static is provided" << std::endl;
			}
			nrThreads = result["t"].as<int>();
			if (nrThreads < 2) {
				std::cout << "Error: option -t should be equal to or greater than 2" << std::endl;
				exit(-1);
			}
		}
		if (result.count("asap")) {
			if (useVR) {
				std::cout << "Option --asap does not work when --vr is present on the command line, since SteamVR imposes a Vsync (e.g. HTC Vive (Pro) @90Hz)" << std::endl;
			}
			else {
				asap = true;
			}
		}
		if (saveOutputImages || (isStatic && !useVR)) {
			asap = true;
		}
		if (!asap) {
			std::cout << "Target fps of the application set to " << targetFps << " and target fps of the videos set to 30fps" << std::endl;
		}
	}
private:
	bool dirExists(const std::string path){
		struct stat info;

		if (stat(path.c_str(), &info) != 0)
			return false;
		else if (info.st_mode & S_IFDIR)
			return true;
		else
			return false;
	}

	bool fileExists(const std::string path) {
		struct stat buffer;
		return (stat(path.c_str(), &buffer) == 0);
	}

	std::string getFolderFromFile(const std::string file) {
		size_t strpos = file.find_last_of("/\\");
		if (strpos == std::string::npos) {
			return ".";
		}
		return file.substr(0, strpos + 1);
	}

	// filter out common errors in the user-provided files and paths
	bool inputAndOutputFilesOK(cxxopts::ParseResult result) {
		// input_json and input_dir are required
		if (result.count("input_json"))
		{
			inputJsonPath = result["input_json"].as<std::string>();
		}
		else {
			std::cout << "Missing required argument -j or --input_json" << std::endl;
			return false;
		}
		if (result.count("input_dir"))
		{
			inputPath = result["input_dir"].as<std::string>();
		}
		else {
			std::cout << "Missing required argument -i or --input_dir" << std::endl;
			return false;
		}

		// some optional output options
		if (result.count("output_json"))
		{
			outputJsonPath = result["output_json"].as<std::string>();
		}
		if (result.count("output_dir"))
		{
			outputPath = result["output_dir"].as<std::string>();
		}
		if (result.count("fps_csv"))
		{
			fpsCsvPath = result["fps_csv"].as<std::string>();
		}
		// check if inputJsonPath and outputJsonPath are existing files
		if (!fileExists(inputJsonPath)) {
			std::cout << "Error: could not open file " << inputJsonPath << std::endl;
			return false;
		}
		if (outputJsonPath != "" && !fileExists(outputJsonPath)) {
			std::cout << "Error: could not open file " << outputJsonPath << std::endl;
			return false;
		}
		// check if inputPath and outputPath and the folder that contains fpsCsvPath are existing folders
		if (!dirExists(inputPath)) {
			std::cout << "Error: could not find folder " << inputPath << std::endl;
			return false;
		}
		if (outputPath != "" && !dirExists(outputPath)) {
			std::cout << "Error: could not find folder " << outputPath << std::endl;
			return false;
		}
		if (fpsCsvPath != "") {
			std::string s = getFolderFromFile(fpsCsvPath);
			if (s == "" || !dirExists(s)) {
				std::cout << "Error: could not find folder that would contain " << fpsCsvPath << std::endl;
				return false;
			}
			useFpsMonitor = true;
		}
		// check if outputPath is provided if outputJsonPath is
		if (outputJsonPath != "" && outputPath == "") {
			std::cout << "Error: -o or --output_dir is required if -p or --output_json is defined" << outputPath << std::endl;
			return false;
		}
		// and check the opposite
		if (outputPath != "" && outputJsonPath == "") {
			std::cout << "Error: -p or --output_json is required if -o or --output_dir is defined" << outputPath << std::endl;
			return false;
		}

		// make sure inputPath ends with a \\ or /
		size_t strpos = inputPath.find_last_of("/\\");
		if (strpos != inputPath.size()-1) {
			inputPath = inputPath + "/";
		}
		// idem outputPath
		if (outputPath != "") {
			size_t strpos = outputPath.find_last_of("/\\");
			if (strpos != outputPath.size() - 1) {
				outputPath = outputPath + "/";
			}
		}
		// read in inputJsonPath
		if (!readInputJson(inputJsonPath, inputPath, outputJsonPath == "", /*out*/ viewport, inputCameras)) {
			return false;
		}
		if (inputCameras.size() == 0) {
			std::cout << "Error: the JSON did not contain any input cameras" << std::endl;
			return false;
		}
		if (outputJsonPath != "") {
			// read in outputJsonPath
			if (!readOutputJson(outputJsonPath, /*out*/outputCameras, StartingFrameNr, outputNrFrames)) {
				return false;
			}
			if (outputCameras.size() == 0) {
				std::cout << "Error: the output JSON did not contain any output cameras" << std::endl;
				return false;
			}
			viewport = outputCameras[0];
		}
		if (inputCameras[0].res_x % 4 != 0 || inputCameras[0].res_y % 4 != 0) {
			std::cout << "Error: the resolution of the cameras should be a multiple of 4 along both dimensions (for OpenGL)" << std::endl;
			return false;
		}

		SCR_WIDTH = viewport.res_x;
		SCR_HEIGHT = viewport.res_y;
		if (SCR_WIDTH < 1 || SCR_WIDTH > 8192 || SCR_HEIGHT < 1 || SCR_HEIGHT > 8192) {
			std::cout << "Error: --width and --height need to be within [1, 8192]" << std::endl;
			return false;
		}

		// check if .mp4 or .png files are provided, and if all inputs have the same type
		std::string inputFileType = inputCameras[0].pathColor.substr(inputCameras[0].pathColor.size() - 3, 3);
		for (InputCamera input : inputCameras) {
			std::string fileTypes[2] = { input.pathColor.substr(input.pathColor.size() - 3, 3), input.pathDepth.substr(input.pathDepth.size() - 3, 3) };
			for (std::string fileType : fileTypes) {
				if (fileType != inputFileType) {
					std::cout << "Error: all input cameras in the JSON need to have the same file type, i.e. the names need to end with .mp4 or .png" << std::endl;
					return false;
				}
			}
		}
		if (inputFileType == "png" || inputFileType == "PNG") {
			usePNGs = true;
		}
		else if (inputFileType == "mp4" || inputFileType == "MP4") {
			usePNGs = false;
			ShowDecoderCapability();
		}
		else {
			std::cout << "Error: all input cameras in the JSON need to be either png or mp4 files, i.e. the names need to end with .mp4 or .png" << std::endl;
			return false;
		}
		// check if all inputs and outputs have the same resolution and projection (and hor_range, ver_range, fov if relevant)
		int input_width = inputCameras[0].res_x;
		int input_height = inputCameras[0].res_y;
		Projection input_proj = inputCameras[0].projection;
		glm::vec2 input_hor_range = inputCameras[0].hor_range;
		glm::vec2 input_ver_range = inputCameras[0].ver_range;
		float input_fov = inputCameras[0].fov;
		for (InputCamera input : inputCameras) {
			if (input.res_x != input_width || input.res_y != input_height) {
				std::cout << "Error: ALL input cameras in the JSON file need to have the same resolution" << std::endl;
				return false;
			}
			if (input.projection != input_proj) {
				std::cout << "Error: ALL input cameras in the JSON file need to have the same Projection" << std::endl;
				return false;
			}
			if (input_proj == Projection::Equirectangular && (input.hor_range != input_hor_range || input.ver_range != input_ver_range)) {
				std::cout << "Error: ALL input cameras in the JSON file need to have the same Hor_range and Ver_range" << std::endl;
				return false;
			}
			if (input_proj == Projection::Fisheye_equidistant && (input.fov != input_fov)) {
				std::cout << "Error: ALL input cameras in the JSON file need to have the same Fov" << std::endl;
				return false;
			}
		}
		if (outputCameras.size() > 1) {
			int output_width = outputCameras[0].res_x;
			int output_height = outputCameras[0].res_y;
			for (OutputCamera output : outputCameras) {
				if (output.res_x != output_width || output.res_y != output_height) {
					std::cout << "Error: ALL output cameras in the JSON file need to have the same resolution" << std::endl;
					return false;
				}
			}
		}

		// check if the ouput images need to be saved to disk
		if (outputPath != "" && outputCameras.size() > 0) {
			saveOutputImages = true;
			SCR_WIDTH = outputCameras[0].res_x;
			SCR_HEIGHT = outputCameras[0].res_y;
			if (useFpsMonitor) {
				std::cout << "Error: writing to to csv file (--fps_csv) when -o/--output_dir and -p/--output_json are defined, is not supported" << std::endl;
				return false;
			}
		}
		return true;
	}
};


#include "Application.h"
#include "VRApplication.h"
#include "PCApplication.h"


int main(int argc, char* argv[]){

	Options options = Options(argc, argv);

	FpsMonitor fpsMonitor(options.useVR);

	if (options.useVR) {
		VRApplication pMainApplication(options, &fpsMonitor, options.inputCameras);
		if (!pMainApplication.BInit())
		{
			pMainApplication.Shutdown();
			return 1;
		}
		pMainApplication.RunMainLoop();
		pMainApplication.Shutdown();
	}
	else {
		PCApplication pMainApplication(options, &fpsMonitor, options.inputCameras, options.outputCameras);
		if (!pMainApplication.BInit())
		{
			pMainApplication.Shutdown();
			return 1;
		}
		pMainApplication.RunMainLoop();
		pMainApplication.Shutdown();
	}

	if (options.useFpsMonitor) {
		fpsMonitor.WriteToCSVFile(options.fpsCsvPath, options.isStatic);
	}
	return 0;
}
