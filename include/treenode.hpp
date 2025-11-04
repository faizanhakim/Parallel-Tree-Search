#pragma once

#include <vector>
#include <memory>
#include <atomic>

template <typename T>
class TreeNode
{
public:
    T data;
    std::vector<std::shared_ptr<TreeNode<T>>> children;
    mutable std::atomic<bool> visited;

    explicit TreeNode(const T &value) : data(value), visited(false) {}

    void addChild(std::shared_ptr<TreeNode<T>> child)
    {
        children.push_back(child);
    }

    bool isLeaf() const
    {
        return children.empty();
    }

    bool markedVisited()
    {
        bool expected = false;
        return visited.compare_exchange_strong(expected, true);
    }

    bool isVisited() const
    {
        return visited.load();
    }
};