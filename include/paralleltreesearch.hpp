#pragma once

#include <thread>
#include <chrono>
#include <mutex>
#include <memory>
#include <atomic>
#include <iostream>

#include "threadpool.hpp"
#include "treenode.hpp"

template <typename T>
class ParallelTreeSearch
{
private:
    std::shared_ptr<ThreadPool<T>> threadPool;
    std::atomic<bool> found;
    std::shared_ptr<TreeNode<T>> resultNode;
    std::mutex resultMutex;
    T target;
    const size_t numThreads;
    std::atomic<size_t> nodesVisited; // for benchmarking purpose

    void searchSubTree(std::shared_ptr<TreeNode<T>> node)
    {
        if (found.load())
        {
            return;
        }

        if (!node->markedVisited())
        {
            return;
        }

        nodesVisited++;

        if (node->data == target)
        {
            found = true;
            std::lock_guard<std::mutex> lock(resultMutex);
            resultNode = node;
            return;
        }

        if (node->isLeaf())
        {
            return;
        }

        for (auto &child : node->children)
        {
            if (found.load())
            {
                return;
            }

            if (threadPool->hasAvailableThread())
            {
                threadPool->enqueue([this, child]()
                                    { this->searchSubTree(child); });
            }
            else
            {
                searchSubTree(child);
            }
        }
    }

public:
    ParallelTreeSearch(size_t num_threads) : threadPool(std::make_shared<ThreadPool<T>>(num_threads)),
                                             found(false), resultNode(nullptr), numThreads(num_threads), nodesVisited(0) {}

    std::shared_ptr<TreeNode<T>> search(std::shared_ptr<TreeNode<T>> root, const T &targetValue)
    {
        found = false;
        resultNode = nullptr;
        target = targetValue;
        nodesVisited = 0;

        threadPool->enqueue([this, root]()
                            { this->searchSubTree(root); });

        while (threadPool->getActiveTaskCount() > 0 || threadPool->getAvailableThreadCount() < numThreads)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return resultNode;
    }

    bool isFound() const
    {
        return found.load();
    }

    size_t getNodesVisited() const
    {
        return nodesVisited.load();
    }
};