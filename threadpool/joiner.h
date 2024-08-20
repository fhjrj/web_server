#ifndef JOIN_H_
#define JOIN_H_

#include <thread>

class join_threads {
	std::vector<std::thread>& threads;
public:
	explicit join_threads(std::vector<std::thread>& threads_) : threads(threads_) {}
	~join_threads() {
		for (unsigned long i = 0; i < threads.size(); i++) {
			if (threads[i].joinable()) {
				threads[i].join();
			}
		}
	}

};

#endif