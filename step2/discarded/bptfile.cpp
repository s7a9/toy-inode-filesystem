#include "bptfile.h"

#include <vector>
#include <cstdlib>
#include <cstring>
#include <iostream>

// ---- BPTFile class implementation ---- //

BPTFile::BPTFile(BlockManager* block_mgr):
    block_mgr_(block_mgr), meta_(nullptr), simple_(false), meta_block_(0),
    root_(nullptr), leaf_head_(nullptr) {}

BPTFile::BPTFile(BlockManager* block_mgr, blockid_t meta_block):
    block_mgr_(block_mgr), meta_(nullptr), simple_(false), meta_block_(0),
    root_(nullptr), leaf_head_(nullptr) {
    open(meta_block);
}

BPTFile::~BPTFile() {
    close();
}

bool BPTFile::open(blockid_t meta_block) {
    if (meta_ != nullptr) return false;
    meta_block_ = meta_block;
    meta_ = block_mgr_->load<BPTFileMeta>(meta_block);
    if (meta_->magic != BPTFileMeta::MAGIC) {
        std::cerr << "BPTFile: open: Invalid meta block" << std::endl;
        return open_failed_();
    }
    if ((meta_->mode & FILE_SIMPLE) != 0) {
        simple_ = true;
        return true; // simple file does not have root and leaf_head
    }
    simple_ = false;
    root_ = block_mgr_->load<BPTNodeBlock>(meta_->root);
    if (root_->magic != BPTNodeBlock::MAGIC) {
        std::cerr << "BPTFile: open: Invalid root block" << std::endl;
        return open_failed_();
    }
    if (meta_->size != root_->total_size) {
        std::cerr << "BPTFile: open: Invalid size in meta block" << std::endl;
        return open_failed_();
    }
    leaf_head_ = block_mgr_->load<BPTLeafBlock>(meta_->leaf_head);
    if (leaf_head_->magic != BPTLeafBlock::MAGIC) {
        std::cerr << "BPTFile: open: Invalid leaf head block" << std::endl;
        return open_failed_();
    }
    cached_nodes_[meta_->root] = root_;
    cached_leaves_[meta_->leaf_head] = leaf_head_;
    return true;
}

blockid_t BPTFile::create(uint32_t owner, BPTFileMode mode, BPTFileType type) {
    if (meta_ != nullptr) {
        std::cerr << "BPTFile: File already opened" << std::endl;
        return 0;
    }
    meta_ = block_mgr_->allocate<BPTFileMeta>(meta_block_);
    if (meta_ == nullptr) {
        std::cerr << "BPTFile: Failed to allocate meta block" << std::endl;
        return 0;
    }
    meta_->magic = BPTFileMeta::MAGIC;
    meta_->owner = owner;
    meta_->mode = mode;
    meta_->type = type;
    meta_->size = 0;
    if (mode & FILE_SIMPLE) {
        simple_ = true;
        return meta_block_;
    }
    simple_ = false;
    root_ = block_mgr_->allocate<BPTNodeBlock>(meta_->root);
    if (root_ == nullptr) {
        std::cerr << "BPTFile: Failed to allocate root block" << std::endl;
        return create_failed_();
    }
    leaf_head_ = block_mgr_->allocate<BPTLeafBlock>(meta_->leaf_head);
    if (leaf_head_ == nullptr) {
        std::cerr << "BPTFile: Failed to allocate leaf head block" << std::endl;
        return create_failed_();
    }
    init_node_block(root_, 0, 0, 0);
    root_->children[0] = meta_->leaf_head;
    init_leaf_block(leaf_head_, meta_->root, 0, 0);
    cached_nodes_[meta_->root] = root_;
    cached_leaves_[meta_->leaf_head] = leaf_head_;
    return meta_block_;
}

void BPTFile::close() {
    if (meta_ == nullptr) return;
    if (!simple_) {
        for (auto& iter : cached_nodes_) {
            block_mgr_->unref_block(iter.first);
        }
        for (auto& iter : cached_leaves_) {
            block_mgr_->unref_block(iter.first);
        }
    }
    block_mgr_->unref_block(meta_block_);
    meta_ = nullptr;
    root_ = nullptr;
    leaf_head_ = nullptr;
    cached_nodes_.clear();
    cached_leaves_.clear();
}

size_t BPTFile::size() const {
    if (meta_ == nullptr) return 0;
    return meta_->size;
}

bool BPTFile::open_failed_() {
    if (leaf_head_) block_mgr_->unref_block(meta_->leaf_head);
    if (root_) block_mgr_->unref_block(meta_->root);
    if (meta_) block_mgr_->unref_block(meta_block_);
    leaf_head_ = nullptr;
    root_ = nullptr;
    meta_ = nullptr;
    meta_block_ = 0;
    return false;
}

blockid_t BPTFile::create_failed_() {
    if (leaf_head_) block_mgr_->free_block(meta_->leaf_head);
    if (root_) block_mgr_->free_block(meta_->root);
    if (meta_) block_mgr_->free_block(meta_block_);
    leaf_head_ = nullptr;
    root_ = nullptr;
    meta_ = nullptr;
    meta_block_ = 0;
    return 0;
}

BPTNodeBlock* BPTFile::load_node_(blockid_t id) {
    auto iter = cached_nodes_.find(id);
    if (iter != cached_nodes_.end()) {
        return iter->second;
    }
    BPTNodeBlock* node = block_mgr_->load<BPTNodeBlock>(id);
    if (node->magic != BPTNodeBlock::MAGIC) {
        std::cerr << "BPTFile: load_node_: Invalid node block" << std::endl;
        block_mgr_->unref_block(id);
        return nullptr;
    }
    return cached_nodes_[id] = node;
}

BPTLeafBlock* BPTFile::load_leaf_(blockid_t id) {
    auto iter = cached_leaves_.find(id);
    if (iter != cached_leaves_.end()) {
        return iter->second;
    }
    BPTLeafBlock* leaf = block_mgr_->load<BPTLeafBlock>(id);
    if (leaf->magic != BPTLeafBlock::MAGIC) {
        std::cerr << "BPTFile: load_leaf_: Invalid leaf block" << std::endl;
        block_mgr_->unref_block(id);
        return nullptr;
    }
    return cached_leaves_[id] = leaf;
}