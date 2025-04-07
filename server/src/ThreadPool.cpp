#include <iostream>

#include "ThreadPool.h"


ThreadPool::ThreadPool(const size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        _workers.emplace_back(&ThreadPool::executionCycle, this);
    }
}


void ThreadPool::submit(const std::function<void()> &task) {
    std::lock_guard<std::mutex> lock(_mutex);
    _taskQueue.push(task);
    _cv.notify_one();
}


void ThreadPool::shutdown() {
    _stopFlag = true;
    _cv.notify_all();

    for (std::thread &worker: _workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}


size_t ThreadPool::activeThreads() const {
    return _activeThreads;
}


ThreadPool::~ThreadPool() {
    if (!_stopFlag) {
        shutdown();
    }
}


void ThreadPool::executionCycle() {
    while (true) {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this] { return _stopFlag || !_taskQueue.empty(); });

        if (_stopFlag && _taskQueue.empty()) {
            return;
        }

        ++_activeThreads;
        auto task = std::move(_taskQueue.front());
        _taskQueue.pop();
        lock.unlock();
        task();
        --_activeThreads;
    }
}
