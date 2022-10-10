#ifndef MEASURE_FPS_H
#define MEASURE_FPS_H


#include <string>
#include <chrono>
#include <vector>
#include <iostream>
#include <fstream>

void SpinUntilTargetTime(Uint64 startTime, float targetTime) {
	float passedTimeMs = (SDL_GetPerformanceCounter() - startTime) / (float)SDL_GetPerformanceFrequency() * 1000.0f;
	while (passedTimeMs < targetTime) {
		passedTimeMs = (SDL_GetPerformanceCounter() - startTime) / (float)SDL_GetPerformanceFrequency() * 1000.0f;
	}
}

/*
* FpsMonitor keeps track of the achieved framerates during rendering and
* can write the results to a CSV file.
*/
class FpsMonitor {
private:
	// containers for holding the framerate data:
	std::vector<float> msPerFrame;
	std::vector<int> videoFrameNrs;

	// some state:
	bool isVR;
	Uint64 prevTime = 0;

public:
	FpsMonitor(bool isVR) : isVR(isVR) {};

	// only used if isStatic is false
	void AddTime(float timeToAdd, int videoFrameNr) {
		msPerFrame.push_back(timeToAdd);
		videoFrameNrs.push_back(videoFrameNr);
	}

	void WriteToCSVFile(std::string path, bool isStatic) {
		
		bool useVideoFrameNr = videoFrameNrs.size() == msPerFrame.size();
		std::ofstream csvFile;
		csvFile.open(path);

		// write the first row
		if (useVideoFrameNr) {
			csvFile << ((isStatic)? "Frame nr," : "Video frame nr,");
		}
		csvFile << "Milliseconds per frame\n";

		// write the other rows
		for (int i = 0; i < msPerFrame.size(); i++) {
			if (useVideoFrameNr) {
				csvFile << std::to_string(videoFrameNrs[i]) + ',';
			}
			csvFile << std::to_string(msPerFrame[i]) + '\n';
		}
		csvFile.close();
		std::cout << "Wrote to " << path << std::endl;
	}

};

#endif