#ifndef POOL_H
#define POOL_H


#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <unordered_set>


/*
* Pool manages the thread pool.
* 
* Each thread executes update_loop(), where they continually receive packets from 
* the demuxers and send the packet to the decoders.
* 
* Threads process data popped from the input_queue and push the results to the output_queue.
* This ensures the video frames are decoded in the correct order.
* Additionally, the threads occasionally consult memcpy_array and demux_array before continuing.
* The mutexes and condition variables are used to prevent race conditions.
*/
class Pool {
	int nrThreads = 2; // should be at least 2 to prevent deadlock
	std::vector<std::thread> pool;
	std::mutex input_queue_mutex;
	std::mutex output_queue_mutex;
	std::mutex memcpy_array_mutex;
	std::mutex demux_array_mutex;
	std::condition_variable_any input_condition;
	std::condition_variable_any output_condition;
	std::condition_variable_any memcpy_condition;
	std::condition_variable_any demux_condition;
	std::vector<std::tuple<int, int, bool>> input_queue; // (inputIndex in range [0, nrImages*2-1], frame number, useForRendering)
	std::vector<std::tuple<int, int>> output_queue; // (inputIndex, avframeIndex, startFromBeginning) with inputIndex in range [0, nrImages*2-1]
	std::vector<bool> memcpy_array; // indicates which decoders are free to start decoding the next frame
	std::vector<int> demux_array; // indicates which demuxers are free to start demuxing the next frame
	bool terminate_pool = false;
	int nrImages = 0;
	std::vector<FFmpegDemuxer*> demuxers;
	std::vector<NvDecoder*> decoders;

public:

	Pool() {}

	void init(int nrImages, std::vector<FFmpegDemuxer*> demuxers, std::vector<NvDecoder*> decoders, int nrThreads) {
		this->nrImages = nrImages;
		this->demuxers = demuxers;
		this->decoders = decoders;
		this->nrThreads = nrThreads;
		this->memcpy_array = std::vector<bool>(decoders.size(), true);
		this->demux_array = std::vector<int>(demuxers.size(), 0);
	}

	void startThreadPool() {
		for (int i = 0; i < nrThreads; i++) {
			pool.push_back(std::thread(&Pool::update_loop, this, i));
		}
	}

	void startDemuxingFirstFrames(std::unordered_set<int> inputsToUse) {
		{
			std::lock_guard<std::mutex> lock(input_queue_mutex);
			for (int i = 0; i < nrImages; i++) {
				bool useForRendering = inputsToUse.find(i) != inputsToUse.end();
				input_queue.push_back(std::tuple<int, int, bool>(2*i, 0, useForRendering));  // decode first frame of ith color(if i even) or depth (if i odd) image 
				input_queue.push_back(std::tuple<int, int, bool>(2*i+1, 0, useForRendering));  // decode first frame of ith color(if i even) or depth (if i odd) image 
			}
		} // to unlock
		input_condition.notify_all();
	}

	void startDemuxingNextFrame(int inputIndex, int frameNr, bool useForRendering) {
		{
			std::lock_guard<std::mutex> lock(input_queue_mutex);
			input_queue.push_back(std::tuple<int, int, bool>(2 * inputIndex, frameNr, useForRendering));      // decode next frame of ith color image
			input_queue.push_back(std::tuple<int, int, bool>(2 * inputIndex + 1, frameNr, useForRendering));  // decode next frame of ith color image
		} // to unlock
		input_condition.notify_all();
	}

	std::tuple<int, int, int, int> waitUntilInputFrameIsDecoded(int inputIndex) {
		int index0;
		int index1;
		std::tuple<int, int> output0;
		std::tuple<int, int> output1;
		{
			// check if color and depth are decoded
			std::unique_lock<std::mutex> lock(output_queue_mutex);
			output_condition.wait(lock, [this, inputIndex, &index0, &index1]() mutable {
				bool foundColor = false;
				bool foundDepth = false;
				for (int i = 0; i < output_queue.size(); i++) {
					if (std::get<0>(output_queue[i]) == 2 * inputIndex) {
						foundColor = true;
						index0 = i;
					}
					else if (std::get<0>(output_queue[i]) == 2 * inputIndex + 1) {
						foundDepth = true;
						index1 = i;
					}
					if (foundColor && foundDepth) return true;
				}
				return false;
				});
			output0 = output_queue[index0];
			output1 = output_queue[index1];
			output_queue.erase(output_queue.begin() + (index0 > index1 ? index0 : index1));
			output_queue.erase(output_queue.begin() + (index0 > index1 ? index1 : index0));
		} // release lock
		output_condition.notify_all();
		return std::tuple<int, int, int, int>(std::get<0>(output0), std::get<1>(output0), std::get<0>(output1), std::get<1>(output1));
	}

	void copyFromGPUToOpenGLTexture(int inputIndex0, int decoded_picture_index0, int inputIndex1, int decoded_picture_index1) {
		decoders[inputIndex0]->HandlePictureDisplay(decoded_picture_index0);
		decoders[inputIndex1]->HandlePictureDisplay(decoded_picture_index1);
		// signal the thread pool that these decoders are now free to decode the next frame
		{
			std::lock_guard<std::mutex> lock(memcpy_array_mutex);
			memcpy_array[inputIndex0] = true;
			memcpy_array[inputIndex1] = true;
		}
		memcpy_condition.notify_all();
	}

	void update_loop(int threadIndex) {

		int inputIndex = -2;
		int frameNr = -1;

		while (!terminate_pool) {
			int index = 0;
			std::unique_lock<std::mutex> lock(input_queue_mutex);
			input_condition.wait(lock, [this, inputIndex, &index, frameNr]() mutable {
				if (terminate_pool) return true;
				if (input_queue.empty()) return false;
				if (std::get<1>(input_queue.front()) == frameNr && std::get<0>(input_queue.front()) == inputIndex + 1) {
					if (input_queue.size() == 1) {
						return false;
					}
					else {
						index = 1;
					}
				}
				return true;
				});
			if (terminate_pool) {
				lock.unlock();
				input_condition.notify_all();
				break;
			}
			inputIndex = std::get<0>(input_queue[index]);
			frameNr = std::get<1>(input_queue[index]);
			bool useForRendering = std::get<2>(input_queue[index]);
			input_queue.erase(input_queue.begin() + index);
			lock.unlock();
			input_condition.notify_all();
			
			{
				std::unique_lock<std::mutex> lock2(demux_array_mutex);
				demux_condition.wait(lock2, [this, inputIndex, frameNr]() {return demux_array[inputIndex] == frameNr || terminate_pool; });
			}
			demux_condition.notify_all();
			if (terminate_pool) {
				break;
			}

			int nVideoBytes = 0;
			uint8_t* pVideo = NULL;
			if (!demux(inputIndex, nVideoBytes, pVideo)) {
				break;
			}

			if (useForRendering) {
				{
					std::unique_lock<std::mutex> lock2(memcpy_array_mutex);
					memcpy_condition.wait(lock2, [this, inputIndex]() {return memcpy_array[inputIndex] || terminate_pool; });
					memcpy_array[inputIndex] = false;
				}
				memcpy_condition.notify_all();
			}
			if (terminate_pool) {
				break;
			}

			int decoded_picture_index = -1;
			if (nVideoBytes) {
				decoders[inputIndex]->Decode(pVideo, nVideoBytes);
				decoded_picture_index = decoders[inputIndex]->picture_index;
			}

			// signal that other threads can use demuxers[inputIndex] from now on
			{
				std::lock_guard<std::mutex> lock(demux_array_mutex);
				demux_array[inputIndex]++;
			}
			demux_condition.notify_all();


			if (useForRendering) {
				// let main thread know the decoding is done
				{
					std::lock_guard<std::mutex> lock3(output_queue_mutex);
					output_queue.push_back(std::tuple<int,int>(inputIndex, decoded_picture_index));
				} // release lock
				output_condition.notify_all();
			}
		}
	}

	void cleanup() {
		terminate_pool = true;
		// wake up all threads.
		input_condition.notify_all();
		memcpy_condition.notify_all();
		output_condition.notify_all();
		for (std::thread& thread : pool)
		{
			thread.join();
		}
		pool.clear();
	}

private:

	bool demux(int inputIndex, int & nVideoBytes, uint8_t* & pVideo) {
		
		if (!demuxers[inputIndex]->Demux(&pVideo, &nVideoBytes)) {
			if (!nVideoBytes) {
				std::cout << "nVideoBytes = " << nVideoBytes << ", breaking" << std::endl;
				return true;
			}
			std::cout << "Demuxing failed for input " << inputIndex / 2 << (inputIndex % 2 == 0? " color" : " depth") << std::endl;
			return false;
		}
		return true;
	}

};

#endif