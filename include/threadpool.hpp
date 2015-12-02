#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

template <typename F>
struct ThreadPool {
private:
	std::vector<std::thread> threads;
	std::vector<F> vector;

	std::atomic_bool joined;
	std::atomic_int running;
	std::condition_variable idle_signal;
	std::condition_variable work_signal;
	std::mutex vector_mutex;

	void run() {
		++this->running;

		while (!this->joined) {
			F job;

			{
				std::unique_lock<std::mutex> vector_lock(this->vector_mutex);
				--this->running;

				if (this->vector.empty()) {
					this->idle_signal.notify_one();
				}

				// releases vector_lock, waits for job or join event
				this->work_signal.wait(vector_lock, [this] {
					// aquires vector_lock
					return !this->vector.empty() || this->joined;
				});
				// vector_lock is re-aquired

				if (this->joined) return;

				job = vector.back();
				this->vector.pop_back();
				++this->running;
			}

			// execute
			job();
		}
	}

public:
	ThreadPool(size_t n) : running(0), joined(false), threads(n) {
		for (auto& thread : this->threads) {
			thread = std::move(std::thread([this] { this->run(); }));
		}
	}

	~ThreadPool() {
		if (this->joined) return;
		this->join();
	}

	void push(const F f) {
		std::lock_guard<std::mutex> guard(this->vector_mutex);

		this->vector.push_back(f);
		this->work_signal.notify_one();
	}

	void join() {
		this->wait();
		this->joined = true;

		// wake any waiting threads
		this->work_signal.notify_all();

		// join joinable threads
		for (auto &x : this->threads) {
			if (!x.joinable()) continue;

			x.join();
		}
	}

	void wait() {
		if (this->running == 0) return;

		std::unique_lock<std::mutex> lock(this->vector_mutex);

		this->idle_signal.wait(lock, [this]() {
			return this->running == 0;
		});
	}
};
