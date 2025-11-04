#include "include/treenode.hpp"
#include "include/threadpool.hpp"
#include "include/paralleltreesearch.hpp"

#include <iostream>
#include <memory>

template <typename T>
void resetTree(std::shared_ptr<TreeNode<T>> node)
{
    if (!node)
        return;
    node->visited.store(false);
    for (auto &child : node->children)
    {
        resetTree(child);
    }
}

template <typename T>
std::shared_ptr<TreeNode<T>> createSampleTree()
{
    auto root = std::make_shared<TreeNode<T>>(1);

    auto child1 = std::make_shared<TreeNode<T>>(2);
    auto child2 = std::make_shared<TreeNode<T>>(3);
    auto child3 = std::make_shared<TreeNode<T>>(4);

    auto child1_1 = std::make_shared<TreeNode<T>>(5);
    auto child1_2 = std::make_shared<TreeNode<T>>(6);
    auto child2_1 = std::make_shared<TreeNode<T>>(7);
    auto child2_2 = std::make_shared<TreeNode<T>>(8);
    auto child3_1 = std::make_shared<TreeNode<T>>(9);

    auto child1_1_1 = std::make_shared<TreeNode<T>>(10);
    auto child2_1_1 = std::make_shared<TreeNode<T>>(11);

    root->addChild(child1);
    root->addChild(child2);
    root->addChild(child3);

    child1->addChild(child1_1);
    child1->addChild(child1_2);
    child2->addChild(child2_1);
    child2->addChild(child2_2);
    child3->addChild(child3_1);

    child1_1->addChild(child1_1_1);
    child2_1->addChild(child2_1_1);

    return root;
}

// Main function demonstrating usage
int main()
{
    std::cout << "=== Parallel Tree Search Demo ===" << std::endl;

    // Create a sample tree
    auto tree = createSampleTree<int>();

    // Create parallel search with 4 threads
    ParallelTreeSearch<int> searcher(4);

    // Search for different values
    std::vector<int> searchTargets = {7, 11, 15, 1};

    for (int target : searchTargets)
    {
        std::cout << "\nSearching for: " << target << std::endl;

        // Reset tree before each search
        resetTree(tree);

        auto result = searcher.search(tree, target);

        if (result != nullptr)
        {
            std::cout << "Found: " << result->data << std::endl;
        }
        else
        {
            std::cout << "Not found" << std::endl;
        }
    }

    std::cout << "\n=== Search Complete ===" << std::endl;

    return 0;
}