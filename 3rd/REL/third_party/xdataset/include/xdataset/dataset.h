#ifndef DATASET_H
#define DATASET_H

#include <boost/variant.hpp>
#include <boost/variant/recursive_wrapper.hpp>

#include <memory>
#include <string>
#include <vector>

#include "block.h"
#include "data_array.h"

namespace xdataset
{

// =========================================================================
// InternalNode -- Group-level node in the Dataset tree
// =========================================================================
//
// An InternalNode holds only child nodes (no Block).  A node is guaranteed
// by the type system to be EITHER an InternalNode OR a LeafNode — it can
// never be both.  Intermediate nodes are created implicitly by
// Dataset::AddBlock().
// =========================================================================

struct InternalNode;

// =========================================================================
// LeafNode -- Block-bearing leaf in the Dataset tree
// =========================================================================

struct LeafNode
{
    std::unique_ptr<Block> block;
};

// =========================================================================
// TreeNode -- a node in the Dataset tree (InternalNode | LeafNode)
// =========================================================================

using TreeNode = boost::variant<
    boost::recursive_wrapper<InternalNode>,
    LeafNode
>;

struct InternalNode
{
    ordered_map<std::string, std::unique_ptr<TreeNode>> children;
};

// ========================================================================
    // ========================================================================
    // Dataset
    // ========================================================================
    //
    // A Dataset is a tree-structured container.  Internal nodes hold
    // children (name -> child mappings).  Leaf nodes are Blocks which
    // hold the actual simulation data (independents + dependents).
    //
    // C++ API uses '/' as path separator; REL uses '.' for the tree and
    // a final '.' to separate block name from data_array name:
    //
    //    C++:  GetDataArray("simulation/SP1/SP", "Vout")
    //    REL:  noise.simulation.SP1.SP.Vout
    //
    //    noise            -- Dataset name
    //    simulation/SP1   -- nested InternalNodes (created implicitly by AddBlock)
    //    SP               -- Block (leaf node, holds independents + dependents)
    //    Vout             -- DataArray within the Block
    //
    // Intermediate InternalNodes are created on demand when AddBlock is called.
    //
    // Shortcut: if `Vout` is a unique DataArray name across the entire
    // Dataset, you can omit the block path:
    //
    //    C++:  GetDataArray("Vout")
    //    REL:  noise..Vout   -- matches *.*. ... .Vout (any block)
    //
    // ========================================================================

    class XDATASET_API Dataset
    {
    public:
        // --------------------------------------------------------------------
        // Construction
        // --------------------------------------------------------------------

        Dataset() = default;
        explicit Dataset(std::string name);

        /// Human-readable name, e.g. "noise".
        const std::string& name() const { return name_; }
        void               set_name(std::string name) { name_ = std::move(name); }

        // --------------------------------------------------------------------
        // Mutation
        // --------------------------------------------------------------------

        /// Add a Block at `path`.  Intermediate nodes are created implicitly.
        /// Block::name() is the path with `/` replaced by `.`.
        ///
        /// Example:  AddBlock("simulation/SP1/SP", info) -> Block "simulation.SP1.SP"
        Block& AddBlock(const std::string& path,
                        const BlockCreateInfo& block_info);

        Block& AddBlock(const std::string& path,
                        BlockCreateInfo&& block_info);

        /// Add a pre-built Block.  Used by deserialization (HDF5 reader etc.).
        Block& AddBlock(const std::string& path, Block block);

        /// Remove a Block and return 1, or 0 if not found.  Empty parent
        /// nodes are NOT automatically cleaned up.
        std::size_t RemoveBlock(const std::string& path);

        /// Remove a node and all its descendants.  Returns the number
        /// of Blocks removed.
        std::size_t RemoveGroup(const std::string& path);

        // --------------------------------------------------------------------
        // Query
        // --------------------------------------------------------------------

        /// True if a Block exists at `path` (node is a LeafNode).
        bool IsLeaf(const std::string& path) const;

        /// True if any node (InternalNode or LeafNode) exists at `path`.
        bool Exists(const std::string& path) const;

        /// True when `data_array_name` appears exactly once across all
        /// Blocks in the entire Dataset.
        bool HasUniqueDataArray(const std::string& data_array_name) const;

        // --------------------------------------------------------------------
        // Access
        // --------------------------------------------------------------------

        /// Full hierarchical access.
        ///
        /// Example:  GetDataArray("simulation/SP1/SP", "freq")
        const DataArray& GetDataArray(const std::string& block_path,
                                      const std::string& data_array_name);

        /// Global unique-name shortcut.
        /// Equivalent to `//data_array_name`.
        const DataArray& GetDataArray(const std::string& data_array_name);

        /// Const / mutable Block access by path.
        const Block& GetBlock(const std::string& path) const;
        Block&       GetBlock(const std::string& path);

        /// Ordered DataArray names within a specific Block
        /// (independents first, then dependents, insertion order).
        std::vector<std::string> GetDataArrayNames(
            const std::string& block_path) const;

        // --------------------------------------------------------------------
        // Enumeration
        // --------------------------------------------------------------------

        /// Direct child Block names under `group_path` (root = "").
        std::vector<std::string> GetBlockNames(
            const std::string& group_path = "") const;

        /// Direct child InternalNode names under `group_path` (root = "").
        std::vector<std::string> GetGroupNames(
            const std::string& group_path = "") const;

        /// All Block paths (recursive), insertion order.
        std::vector<std::string> GetAllBlockPaths() const;

        // --------------------------------------------------------------------
        // Capacity
        // --------------------------------------------------------------------

        /// Total number of Blocks in the Dataset.
        std::size_t block_count() const;

        // --------------------------------------------------------------------
        // Utilities
        // --------------------------------------------------------------------

        /// Split a '/' path into segments.  "a/b/c" -> ["a", "b", "c"].
        static std::vector<std::string> SplitPath(const std::string& path);

    private:
        /// Navigate to the node at `path` (const).  Returns nullptr if not found.
        const TreeNode* navigate(const std::string& path) const;

        /// Navigate to the node at `path` (mutable).  Returns nullptr if not found.
        TreeNode* navigate(const std::string& path);

        /// Recursively collect Block paths.
        void collect_block_paths(const TreeNode& node,
                                 const std::string& prefix,
                                 std::vector<std::string>& paths) const;

        /// Recursively count Blocks.
        std::size_t collect_block_count(const TreeNode& node) const;

        /// Walk the tree looking for `data_array_name`.
        /// Returns the block path of the unique match.
        std::string find_unique_data_array(
            const std::string& data_array_name) const;

        std::string name_;
        TreeNode    root_;
    };
}

#endif  // DATASET_H