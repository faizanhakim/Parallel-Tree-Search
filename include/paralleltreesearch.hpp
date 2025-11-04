#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <functional>
#include <atomic>
#include <memory>
#include <optional>
#include <cassert>

#include "treenode.hpp"

// Tunables (can be tweaked from benchmark by recompiling)
#ifndef PAR_CUTOFF_DEPTH
#define PAR_CUTOFF_DEPTH 4
#endif

#ifndef MAX_PAR_CHILDREN
#define MAX_PAR_CHILDREN 2
#endif

// A parallel tree search that keeps the existing file structure and public API.
// Key improvements:
//  - Cooperative cancellation via a single atomic 'found' flag.
//  - Task granularity control via cutoff depth and limited fan-out.
//  - No reliance on per-node 'visited' (trees don't need it), which reduces contention.
//  - Bounded task creation; remaining children are processed sequentially to keep locality high.
//  - Correct nodesVisited counting.
template <typename T>
class ParallelTreeSearch
{
private:
    struct Task {
        std::shared_ptr<TreeNode<T>> node;
        int depth;
    };

    const size_t threadCount;
    std::mutex mx;
    std::condition_variable cv;
    std::deque<Task> tasks;

    std::atomic<bool> stop{false};
    std::atomic<bool> found{false};
    std::atomic<size_t> inflight{0};
    std::atomic<size_t> nodesVisited{0};

    std::shared_ptr<TreeNode<T>> resultNode{nullptr};
    std::vector<std::thread> workers;

    // Pop a task (LIFO for locality)
    bool pop_task(Task& out) {
        std::unique_lock<std::mutex> lk(mx);
        if (tasks.empty()) return false;
        out = std::move(tasks.back());
        tasks.pop_back();
        return true;
    }

    // Push a task
    void push_task(Task t) {
        {
            std::lock_guard<std::mutex> lk(mx);
            tasks.emplace_back(std::move(t));
        }
        cv.notify_one();
    }

    // Sequential DFS used at/after cutoff to avoid spawning tiny tasks
    void sequential_search(const std::shared_ptr<TreeNode<T>>& node, int depth, const T& target) {
        if (!node || found.load(std::memory_order_relaxed)) return;
        nodesVisited.fetch_add(1, std::memory_order_relaxed);
        if (node->data == target) {
            resultNode = node;
            found.store(true, std::memory_order_relaxed);
            return;
        }
        if (node->children.empty()) return;
        for (auto& c : node->children) {
            if (found.load(std::memory_order_relaxed)) return;
            sequential_search(c, depth + 1, target);
        }
    }

    void worker_loop(const T& target) {
        while (true) {
            if (found.load(std::memory_order_relaxed)) {
                // Drain: allow inflight to drop to 0
                Task dummy;
                if (inflight.load(std::memory_order_relaxed) == 0) break;
            }

            Task task;
            {
                std::unique_lock<std::mutex> lk(mx);
                cv.wait(lk, [&]{
                    return stop.load() || found.load() || !tasks.empty();
                });
                if (stop.load()) break;
                if (tasks.empty()) {
                    if (found.load() && inflight.load()==0) break;
                    else continue;
                }
                task = std::move(tasks.back());
                tasks.pop_back();
            }

            // Execute task
            if (found.load(std::memory_order_relaxed)) {
                if (inflight.fetch_sub(1, std::memory_order_relaxed) == 1) {
                    cv.notify_all();
                }
                continue;
            }

            auto node = task.node;
            if (node) {
                nodesVisited.fetch_add(1, std::memory_order_relaxed);
                if (node->data == target) {
                    resultNode = node;
                    found.store(true, std::memory_order_relaxed);
                } else if (!node->children.empty()) {
                    // If below cutoff: spawn limited parallelism; rest sequential
                    if (task.depth < PAR_CUTOFF_DEPTH) {
                        size_t spawned = 0;
                        for (size_t i = 0; i < node->children.size(); ++i) {
                            if (found.load(std::memory_order_relaxed)) break;
                            auto& child = node->children[i];
                            if (spawned < MAX_PAR_CHILDREN) {
                                inflight.fetch_add(1, std::memory_order_relaxed);
                                push_task(Task{child, task.depth + 1});
                                ++spawned;
                            } else {
                                // Process remaining children sequentially to reduce queue pressure
                                sequential_search(child, task.depth + 1, target);
                                if (found.load(std::memory_order_relaxed)) break;
                            }
                        }
                    } else {
                        // At/after cutoff: run sequentially
                        sequential_search(node, task.depth, target);
                    }
                }
            }

            if (inflight.fetch_sub(1, std::memory_order_relaxed) == 1) {
                // Last task finished; wake any waiters
                cv.notify_all();
            }
        }
    }

public:
    explicit ParallelTreeSearch(size_t numThreads)
        : threadCount(numThreads ? numThreads : 1) {}

    std::shared_ptr<TreeNode<T>> search(const std::shared_ptr<TreeNode<T>>& root, const T& target) {
        // Reset state
        found.store(false, std::memory_order_relaxed);
        stop.store(false, std::memory_order_relaxed);
        nodesVisited.store(0, std::memory_order_relaxed);
        resultNode.reset();
        inflight.store(0, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lk(mx);
            tasks.clear();
        }

        if (!root) return nullptr;

        // Seed the first task
        inflight.fetch_add(1, std::memory_order_relaxed);
        push_task(Task{root, 0});

        // Start workers
        workers.clear();
        workers.reserve(threadCount);
        for (size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back([this, &target]{
                worker_loop(target);
            });
        }

        // Wait until either found or inflight drains
        {
            std::unique_lock<std::mutex> lk(mx);
            cv.wait(lk, [&]{
                return found.load() || inflight.load() == 0;
            });
        }

        // Signal stop and join threads
        stop.store(true);
        cv.notify_all();
        for (auto& th : workers) if (th.joinable()) th.join();
        workers.clear();

        return resultNode;
    }

    bool isFound() const { return found.load(std::memory_order_relaxed); }
    size_t getNodesVisited() const { return nodesVisited.load(std::memory_order_relaxed); }
};

