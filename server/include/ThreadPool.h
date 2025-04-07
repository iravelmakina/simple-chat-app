#pragma once
#include <queue>
#include <thread>


class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);

    void submit(const std::function<void()>& task);
    void shutdown();

    size_t activeThreads() const;

    ~ThreadPool();

private:
    std::queue<std::function<void()>> _taskQueue;
    std::vector<std::thread> _workers;

    std::condition_variable _cv;
    std::mutex _mutex;

    std::atomic<bool> _stopFlag{false};
    std::atomic<size_t> _activeThreads{0};

    void executionCycle();
};

