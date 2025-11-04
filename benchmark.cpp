// benchmark.cpp
#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <random>
#include <queue>
#include <iomanip>
#include <algorithm>
#include "include/treenode.hpp"
#include "include/threadpool.hpp"
#include "include/paralleltreesearch.hpp"

// Performance metrics structure
struct PerformanceMetrics
{
    std::string algorithmName;
    double executionTimeMs;
    size_t nodesVisited;
    bool found;
    int depth;
};

// Tree generator class
class TreeGenerator
{
private:
    std::mt19937 rng;

public:
    TreeGenerator(unsigned seed = 42) : rng(seed) {}

    // Generate a balanced tree
    std::shared_ptr<TreeNode<int>> generateBalancedTree(int depth, int branchingFactor, int &nodeCounter)
    {
        auto node = std::make_shared<TreeNode<int>>(nodeCounter++);

        if (depth > 0)
        {
            for (int i = 0; i < branchingFactor; ++i)
            {
                node->addChild(generateBalancedTree(depth - 1, branchingFactor, nodeCounter));
            }
        }

        return node;
    }

    // Generate a random tree
    std::shared_ptr<TreeNode<int>> generateRandomTree(int maxNodes, int minChildren, int maxChildren, int &nodeCounter)
    {
        if (nodeCounter >= maxNodes)
        {
            return nullptr;
        }

        auto node = std::make_shared<TreeNode<int>>(nodeCounter++);

        if (nodeCounter < maxNodes)
        {
            std::uniform_int_distribution<int> dist(minChildren, maxChildren);
            int numChildren = dist(rng);

            for (int i = 0; i < numChildren && nodeCounter < maxNodes; ++i)
            {
                auto child = generateRandomTree(maxNodes, minChildren, maxChildren, nodeCounter);
                if (child)
                {
                    node->addChild(child);
                }
            }
        }

        return node;
    }

    // Generate a skewed tree (worst case for some algorithms)
    std::shared_ptr<TreeNode<int>> generateSkewedTree(int depth, int &nodeCounter)
    {
        auto node = std::make_shared<TreeNode<int>>(nodeCounter++);

        if (depth > 0)
        {
            // Add one deep child and several shallow children
            node->addChild(generateSkewedTree(depth - 1, nodeCounter));

            // Add 2-3 leaf children
            std::uniform_int_distribution<int> dist(2, 3);
            int leafChildren = dist(rng);
            for (int i = 0; i < leafChildren; ++i)
            {
                node->addChild(std::make_shared<TreeNode<int>>(nodeCounter++));
            }
        }

        return node;
    }
};

// DFS Search
class DFSSearch
{
private:
    size_t nodesVisited;

    bool dfsHelper(std::shared_ptr<TreeNode<int>> node, int target)
    {
        if (!node)
            return false;

        nodesVisited++;

        if (node->data == target)
        {
            return true;
        }

        for (auto &child : node->children)
        {
            if (dfsHelper(child, target))
            {
                return true;
            }
        }

        return false;
    }

public:
    PerformanceMetrics search(std::shared_ptr<TreeNode<int>> root, int target)
    {
        nodesVisited = 0;

        auto start = std::chrono::high_resolution_clock::now();
        bool found = dfsHelper(root, target);
        auto end = std::chrono::high_resolution_clock::now();

        double duration = std::chrono::duration<double, std::milli>(end - start).count();

        return {"DFS", duration, nodesVisited, found, -1};
    }
};

// BFS Search
class BFSSearch
{
private:
    size_t nodesVisited;

public:
    PerformanceMetrics search(std::shared_ptr<TreeNode<int>> root, int target)
    {
        nodesVisited = 0;

        auto start = std::chrono::high_resolution_clock::now();

        std::queue<std::shared_ptr<TreeNode<int>>> q;
        q.push(root);
        bool found = false;

        while (!q.empty() && !found)
        {
            auto node = q.front();
            q.pop();

            nodesVisited++;

            if (node->data == target)
            {
                found = true;
                break;
            }

            for (auto &child : node->children)
            {
                q.push(child);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::milli>(end - start).count();

        return {"BFS", duration, nodesVisited, found, -1};
    }
};

// Helper to reset tree for parallel search
void resetTree(std::shared_ptr<TreeNode<int>> node)
{
    if (!node)
        return;
    node->visited.store(false);
    for (auto &child : node->children)
    {
        resetTree(child);
    }
}

// Helper to count total nodes
int countNodes(std::shared_ptr<TreeNode<int>> node)
{
    if (!node)
        return 0;
    int count = 1;
    for (auto &child : node->children)
    {
        count += countNodes(child);
    }
    return count;
}

// Helper to calculate tree depth
int calculateDepth(std::shared_ptr<TreeNode<int>> node)
{
    if (!node || node->isLeaf())
        return 0;
    int maxDepth = 0;
    for (auto &child : node->children)
    {
        maxDepth = std::max(maxDepth, calculateDepth(child));
    }
    return maxDepth + 1;
}

// Print results table
void printResults(const std::vector<PerformanceMetrics> &results, int totalNodes, int treeDepth)
{
    std::cout << "\n"
              << std::string(80, '=') << "\n";
    std::cout << "BENCHMARK RESULTS\n";
    std::cout << std::string(80, '=') << "\n";
    std::cout << "Tree Size: " << totalNodes << " nodes\n";
    std::cout << "Tree Depth: " << treeDepth << "\n";
    std::cout << std::string(80, '-') << "\n";

    std::cout << std::left << std::setw(25) << "Algorithm"
              << std::right << std::setw(15) << "Time (ms)"
              << std::setw(15) << "Nodes Visited"
              << std::setw(12) << "Found"
              << std::setw(13) << "Speedup" << "\n";
    std::cout << std::string(80, '-') << "\n";

    double baselineTime = results[0].executionTimeMs;

    for (const auto &result : results)
    {
        double speedup = baselineTime / result.executionTimeMs;

        std::cout << std::left << std::setw(25) << result.algorithmName
                  << std::right << std::setw(15) << std::fixed << std::setprecision(3) << result.executionTimeMs
                  << std::setw(15) << result.nodesVisited
                  << std::setw(12) << (result.found ? "Yes" : "No")
                  << std::setw(13) << std::fixed << std::setprecision(2) << speedup << "x\n";
    }

    std::cout << std::string(80, '=') << "\n\n";
}

// Run benchmark suite
void runBenchmark(const std::string &testName, std::shared_ptr<TreeNode<int>> tree, int target)
{
    std::cout << "\n"
              << std::string(80, '=') << "\n";
    std::cout << "RUNNING: " << testName << "\n";
    std::cout << "Target value: " << target << "\n";

    int totalNodes = countNodes(tree);
    int treeDepth = calculateDepth(tree);

    std::vector<PerformanceMetrics> results;

    // DFS Search
    DFSSearch dfs;
    results.push_back(dfs.search(tree, target));

    // BFS Search
    BFSSearch bfs;
    results.push_back(bfs.search(tree, target));

    // Parallel Search with different thread counts
    std::vector<int> threadCounts = {2, 4, 8, 16};

    for (int numThreads : threadCounts)
    {
        resetTree(tree);
        ParallelTreeSearch<int> parallelSearch(numThreads);

        auto start = std::chrono::high_resolution_clock::now();
        auto result = parallelSearch.search(tree, target);
        auto end = std::chrono::high_resolution_clock::now();

        double duration = std::chrono::duration<double, std::milli>(end - start).count();
        size_t nodesVisited = parallelSearch.getNodesVisited();

        std::string name = "Parallel (" + std::to_string(numThreads) + " threads)";
        results.push_back({name, duration, nodesVisited, result != nullptr, -1});
    }

    printResults(results, totalNodes, treeDepth);
}

int main()
{
    TreeGenerator generator;

    std::cout << "\n";
    std::cout << "==============================================================================\n";
    std::cout << "|          TREE SEARCH ALGORITHM PERFORMANCE BENCHMARK SUITE                 |\n";
    std::cout << "==============================================================================\n";

    std::cout << "\n>>> SECTION 1: Threading Overhead Analysis <<<\n";

    // Test 1: Small Tree - Threading overhead dominates
    {
        int nodeCounter = 0;
        auto tree = generator.generateBalancedTree(4, 3, nodeCounter);
        int target = nodeCounter / 2;
        runBenchmark("Test 1: Small Tree (depth=4, branching=3) - Threading Overhead", tree, target);
    }

    // Test 2: Medium Tree - Transition point
    {
        int nodeCounter = 0;
        auto tree = generator.generateBalancedTree(6, 4, nodeCounter);
        int target = nodeCounter - 100;
        runBenchmark("Test 2: Medium Tree (depth=6, branching=4) - Transition Point", tree, target);
    }

    std::cout << "\n>>> SECTION 2: Large Trees - Parallel Advantage <<<\n";

    // Test 3: Large Balanced Tree
    {
        int nodeCounter = 0;
        auto tree = generator.generateBalancedTree(9, 4, nodeCounter);
        int target = nodeCounter - 5000;
        runBenchmark("Test 3: Large Balanced Tree (depth=9, branching=4) ~262K nodes", tree, target);
    }

    // Test 4: Very Large Balanced Tree
    {
        int nodeCounter = 0;
        auto tree = generator.generateBalancedTree(10, 4, nodeCounter);
        int target = nodeCounter - 10000;
        runBenchmark("Test 4: Very Large Tree (depth=10, branching=4) ~1M nodes", tree, target);
    }

    // Test 5: Massive Wide Tree
    {
        int nodeCounter = 0;
        auto tree = generator.generateBalancedTree(8, 8, nodeCounter);
        int target = nodeCounter - 20000;
        runBenchmark("Test 5: Massive Wide Tree (depth=8, branching=8) ~16M nodes", tree, target);
    }

    std::cout << "\n>>> SECTION 3: Worst Case Scenarios - DFS Must Traverse Entire Tree <<<\n";

    // Test 6: DFS Nightmare - Target at Rightmost Leaf
    {
        int nodeCounter = 0;
        auto tree = generator.generateBalancedTree(10, 3, nodeCounter);
        int target = nodeCounter - 1; // Last node DFS reaches
        runBenchmark("Test 6: DFS WORST - Rightmost Leaf (depth=10, branching=3) ~88K nodes", tree, target);
    }

    // Test 7: DFS Nightmare - Large Tree, Rightmost Node
    {
        int nodeCounter = 0;
        auto tree = generator.generateBalancedTree(10, 4, nodeCounter);
        int target = nodeCounter - 1; // Last node in DFS order
        runBenchmark("Test 7: DFS WORST - Large Tree Rightmost (depth=10, branching=4) ~1M nodes", tree, target);
    }

    // Test 8: DFS Nightmare - Massive Tree, Target at End
    {
        int nodeCounter = 0;
        auto tree = generator.generateBalancedTree(9, 5, nodeCounter);
        int target = nodeCounter - 1; // Absolute last node
        runBenchmark("Test 8: DFS WORST - Massive Rightmost (depth=9, branching=5) ~1.9M nodes", tree, target);
    }

    // Test 9: Target Not Found - Must Search Every Node
    {
        int nodeCounter = 0;
        auto tree = generator.generateBalancedTree(9, 5, nodeCounter);
        int target = -1; // Doesn't exist
        runBenchmark("Test 9: WORST CASE - Target Not Found (depth=9, branching=5) ~1.9M nodes", tree, target);
    }

    // Test 10: Skewed Tree - Worst for Parallel
    {
        int nodeCounter = 0;
        auto tree = generator.generateSkewedTree(2000, nodeCounter);
        int target = nodeCounter - 1; // At the end of long chain
        runBenchmark("Test 10: WORST CASE - Skewed Tree (depth=2000, unbalanced)", tree, target);
    }

    // Test 11: Deep Tree - Target at Bottom Right
    {
        int nodeCounter = 0;
        auto tree = generator.generateBalancedTree(11, 3, nodeCounter);
        int target = nodeCounter - 1; // Deepest rightmost node
        runBenchmark("Test 11: DFS WORST - Deep Rightmost (depth=11, branching=3) ~177K nodes", tree, target);
    }

    // Test 12: Wide Tree - Target at Far Right
    {
        int nodeCounter = 0;
        auto tree = generator.generateBalancedTree(7, 8, nodeCounter);
        int target = nodeCounter - 1; // Far right in wide tree
        runBenchmark("Test 12: DFS WORST - Wide Rightmost (depth=7, branching=8) ~2.3M nodes", tree, target);
    }

    return 0;
}