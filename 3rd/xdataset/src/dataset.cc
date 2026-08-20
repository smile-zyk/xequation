#include "dataset.h"

#include <sstream>
#include <stdexcept>

namespace xdataset
{
    Dataset::Dataset(std::string name)
        : name_(std::move(name))
    {}

    std::vector<std::string> Dataset::SplitPath(const std::string& path)
    {
        std::vector<std::string> parts;
        if (path.empty()) return parts;
        std::istringstream stream(path);
        std::string segment;
        while (std::getline(stream, segment, '/'))
            if (!segment.empty()) parts.push_back(segment);
        return parts;
    }

    // =========================================================================
    //  navigate -- walk the tree to a node at `path`
    // =========================================================================

    const TreeNode* Dataset::navigate(const std::string& path) const
    {
        auto parts = SplitPath(path);
        const TreeNode* node = &root_;
        for (const auto& seg : parts)
        {
            const InternalNode* internal = node->internal();
            if (!internal) return nullptr;
            auto it = internal->children.find(seg);
            if (it == internal->children.end()) return nullptr;
            node = it->second.get();
        }
        return node;
    }

    TreeNode* Dataset::navigate(const std::string& path)
    {
        auto parts = SplitPath(path);
        TreeNode* node = &root_;
        for (const auto& seg : parts)
        {
            InternalNode* internal = node->internal();
            if (!internal) return nullptr;
            auto it = internal->children.find(seg);
            if (it == internal->children.end()) return nullptr;
            node = it->second.get();
        }
        return node;
    }

    // =========================================================================
    //  collect_block_paths -- recursive traversal
    // =========================================================================

    void Dataset::collect_block_paths(const TreeNode& node,
                                      const std::string& prefix,
                                      std::vector<std::string>& paths) const
    {
        const InternalNode* internal = node.internal();
        if (!internal) return;  // LeafNode -- not recursed by callers, but safe

        for (const auto& kv : internal->children)
        {
            const std::string full = prefix.empty() ? kv.first : prefix + "/" + kv.first;
            if (kv.second->leaf())
                paths.push_back(full);
            else
                collect_block_paths(*kv.second, full, paths);
        }
    }

    // =========================================================================
    //  collect_block_count -- recursive count
    // =========================================================================

    std::size_t Dataset::collect_block_count(const TreeNode& node) const
    {
        const InternalNode* internal = node.internal();
        if (!internal) return 0;

        std::size_t count = 0;
        for (const auto& kv : internal->children)
        {
            if (kv.second->leaf())
                ++count;
            else
                count += collect_block_count(*kv.second);
        }
        return count;
    }

    // =========================================================================
    //  AddBlock -- three overloads, shared implementation
    // =========================================================================

    Block& Dataset::AddBlock(const std::string& path, const BlockCreateInfo& block_info)
    {
        return AddBlock(path, Block(block_info));
    }

    Block& Dataset::AddBlock(const std::string& path, BlockCreateInfo&& block_info)
    {
        return AddBlock(path, static_cast<const BlockCreateInfo&>(block_info));
    }

    Block& Dataset::AddBlock(const std::string& path, Block block)
    {
        auto parts = SplitPath(path);
        if (parts.empty())
            throw std::invalid_argument("block path must not be empty");

        // Navigate to the parent InternalNode, creating intermediate
        // nodes as needed.
        InternalNode* node = root_.internal();
        for (std::size_t i = 0; i + 1 < parts.size(); ++i)
        {
            auto it = node->children.find(parts[i]);
            if (it == node->children.end())
            {
                it = node->children.emplace(
                    parts[i],
                    std::unique_ptr<TreeNode>(new TreeNode(InternalNode{})))
                    .first;
            }
            else if (it->second->leaf())
            {
                throw std::invalid_argument(
                    "path segment '" + parts[i] + "' is already a leaf Block");
            }
            node = it->second->internal();
        }

        // Insert the Block as a LeafNode.
        auto& slot = node->children[parts.back()];
        if (slot && slot->leaf())
            throw std::invalid_argument("duplicate Block at path: " + path);

        std::string dotted = path;
        for (auto& ch : dotted) if (ch == '/') ch = '.';
        block.set_name(dotted);

        auto owned = std::unique_ptr<Block>(new Block(std::move(block)));
        Block& ref = *owned;
        slot = std::unique_ptr<TreeNode>(new TreeNode(LeafNode{std::move(owned)}));
        return ref;
    }

    // =========================================================================
    //  RemoveBlock
    // =========================================================================

    std::size_t Dataset::RemoveBlock(const std::string& path)
    {
        auto parts = SplitPath(path);
        if (parts.empty()) return 0;

        // Navigate to the parent.
        InternalNode* parent = root_.internal();
        for (std::size_t i = 0; i + 1 < parts.size(); ++i)
        {
            auto it = parent->children.find(parts[i]);
            if (it == parent->children.end()) return 0;
            parent = it->second->internal();
            if (!parent) return 0;
        }

        auto it = parent->children.find(parts.back());
        if (it == parent->children.end()) return 0;
        if (!it->second->leaf()) return 0;
        parent->children.erase(it);
        return 1;
    }

    // =========================================================================
    //  RemoveGroup
    // =========================================================================

    std::size_t Dataset::RemoveGroup(const std::string& path)
    {
        auto parts = SplitPath(path);
        if (parts.empty())
        {
            std::size_t n = block_count();
            root_.internal()->children.clear();
            return n;
        }

        const TreeNode* target = navigate(path);
        if (!target) return 0;
        std::size_t count = collect_block_count(*target);

        InternalNode* parent = root_.internal();
        for (std::size_t i = 0; i + 1 < parts.size(); ++i)
        {
            auto it = parent->children.find(parts[i]);
            if (it == parent->children.end()) return count;
            parent = it->second->internal();
            if (!parent) return count;
        }
        parent->children.erase(parts.back());
        return count;
    }

    // =========================================================================
    //  Query
    // =========================================================================

    bool Dataset::IsLeaf(const std::string& path) const
    {
        const TreeNode* node = navigate(path);
        return node != nullptr && node->leaf() != nullptr;
    }

    bool Dataset::Exists(const std::string& path) const
    {
        return navigate(path) != nullptr;
    }

    bool Dataset::HasUniqueDataArray(const std::string& data_array_name) const
    {
        std::size_t count = 0;
        std::vector<std::string> paths = GetAllBlockPaths();
        for (const auto& p : paths)
        {
            const TreeNode* node = navigate(p);
            const LeafNode* leaf = node ? node->leaf() : nullptr;
            if (!leaf) continue;
            const Block& block = *leaf->block;
            for (const auto& n : block.independents())
                if (n == data_array_name && ++count > 1) return false;
            for (const auto& n : block.dependents())
                if (n == data_array_name && ++count > 1) return false;
        }
        return count == 1;
    }

    // =========================================================================
    //  Access
    // =========================================================================

    const DataArray& Dataset::GetDataArray(const std::string& block_path, const std::string& data_array_name)
    {
        Block& block = GetBlock(block_path);
        return block.GetOrCreateDataArray(data_array_name);
    }

    const DataArray& Dataset::GetDataArray(const std::string& data_array_name)
    {
        std::string path = find_unique_data_array(data_array_name);
        return GetDataArray(path, data_array_name);
    }

    const Block& Dataset::GetBlock(const std::string& path) const
    {
        const TreeNode* node = navigate(path);
        const LeafNode* leaf = node ? node->leaf() : nullptr;
        if (!leaf) throw std::out_of_range("block not found: " + path);
        return *leaf->block;
    }

    Block& Dataset::GetBlock(const std::string& path)
    {
        TreeNode* node = navigate(path);
        LeafNode* leaf = node ? node->leaf() : nullptr;
        if (!leaf) throw std::out_of_range("block not found: " + path);
        return *leaf->block;
    }

    std::vector<std::string> Dataset::GetDataArrayNames(const std::string& block_path) const
    {
        const Block& block = GetBlock(block_path);
        std::vector<std::string> names;
        for (const auto& n : block.independents()) names.push_back(n);
        for (const auto& n : block.dependents()) names.push_back(n);
        return names;
    }

    // =========================================================================
    //  Enumeration
    // =========================================================================

    std::vector<std::string> Dataset::GetBlockNames(const std::string& group_path) const
    {
        const TreeNode* node = navigate(group_path);
        const InternalNode* internal = node ? node->internal() : nullptr;
        if (!internal) throw std::out_of_range("group not found: " + group_path);

        std::vector<std::string> names;
        for (const auto& kv : internal->children)
            if (kv.second->leaf())
                names.push_back(kv.first);
        return names;
    }

    std::vector<std::string> Dataset::GetGroupNames(const std::string& group_path) const
    {
        const TreeNode* node = navigate(group_path);
        if (!node) throw std::out_of_range("group not found: " + group_path);

        const InternalNode* internal = node->internal();
        if (!internal) return {};  // leaf (block), not a group

        std::vector<std::string> names;
        for (const auto& kv : internal->children)
            if (kv.second->internal())
                names.push_back(kv.first);
        return names;
    }

    std::vector<std::string> Dataset::GetAllBlockPaths() const
    {
        std::vector<std::string> paths;
        collect_block_paths(root_, "", paths);
        return paths;
    }

    std::size_t Dataset::block_count() const { return collect_block_count(root_); }

    // =========================================================================
    //  find_unique_data_array
    // =========================================================================

    std::string Dataset::find_unique_data_array(const std::string& data_array_name) const
    {
        std::string found_path;
        std::size_t count = 0;
        std::vector<std::string> paths = GetAllBlockPaths();
        for (const auto& p : paths)
        {
            const TreeNode* node = navigate(p);
            const LeafNode* leaf = node ? node->leaf() : nullptr;
            if (!leaf) continue;
            const Block& block = *leaf->block;
            for (const auto& n : block.independents())
                if (n == data_array_name) { found_path = p; ++count; }
            for (const auto& n : block.dependents())
                if (n == data_array_name) { found_path = p; ++count; }
        }
        if (count == 0)
            throw std::invalid_argument("DataArray '" + data_array_name + "' not found in any block");
        if (count > 1)
            throw std::invalid_argument("DataArray '" + data_array_name + "' is not unique (found in " + std::to_string(count) + " places); use the full block_path/data_array_name syntax");
        return found_path;
    }
}
