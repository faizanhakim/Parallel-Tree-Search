#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <functional>
#include <atomic>
#include <stdexcept>
#include <utility>

template <typename T>
class ThreadPool
{
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<int> available_threads;
    const int maxThreads;
    std::atomic<int> activeTasks{0};

public:
    explicit ThreadPool(size_t numThreads) : maxThreads(numThreads), stop(false), available_threads(numThreads)
    {
        for (size_t i = 0; i < numThreads; ++i)
        {
            workers.emplace_back([this]
                                 {
                while(true){
                    
                    std::function<void()> task;
                    
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this]{
                            return stop.load() || !tasks.empty();
                        });

                        if(stop.load() && tasks.empty()){
                            return;
                        }

                        if(!tasks.empty()){
                            task = std::move(tasks.front());
                            tasks.pop();
                        }
                    }

                    if(task){
                        available_threads--;
                        task();
                        activeTasks--;
                        available_threads++;
                    }

                } });
        }
    }

    ~ThreadPool()
    {
        stop = true;
        condition.notify_all();
        for (auto &worker : workers)
        {
            if (worker.joinable())
                worker.join();
        }
    }

    bool hasAvailableThread() const
    {
        return available_threads.load() > 0;
    }

    int getAvailableThreadCount() const
    {
        return available_threads.load();
    }

    int getActiveTaskCount() const
    {
        return activeTasks.load();
    }

    template <class F>
    void enqueue(F &&f)
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (stop.load())
            {
                throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
            }
            tasks.emplace(std::forward<F>(f));
            activeTasks++;
        }
        condition.notify_one();
    }
};