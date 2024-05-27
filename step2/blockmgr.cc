#include "blockmgr.h"

#include <iostream>
#include <cstring>

#define LOCK() LockGuard lock_guard(&lock_)

#define CYLINDER(x) (uint32_t)(x >> 32)
#define SECTION(x) (uint32_t)(x & 0xFFFFFFFF)

BlockManager::BlockManager(RemoteDisk* disk, bool create): disk_(disk) {
    // Load super block
    auto iter = load_block_(0);
    superblock_ = reinterpret_cast<SuperBlock*>(iter->second->data);
    iter->second->refcnt = 1;
    iter->second->dirty = true;
    if (superblock_->magic != SuperBlock::MAGIC || create) {
        std::cout << "BlockManager: Creating file system on remote disk..." << std::endl;
        superblock_->magic = SuperBlock::MAGIC;
        superblock_->block_size = BLOCK_SIZE;
        superblock_->free_list_head = 0;
        superblock_->root_inode = 0;
        superblock_->block_end = 0;
        superblock_->version = time(nullptr);
    }
    // print super block info
    std::cout << "BlockManager: Block size: " << superblock_->block_size
        << ", Free list head: " << superblock_->free_list_head
        << ", Root inode: " << superblock_->root_inode
        << ", Block end: " << superblock_->block_end
        << ", Version: " << superblock_->version << std::endl;
    sem_init(&lock_, 0, 1);
}

BlockManager::~BlockManager() {
    LOCK();
    size_t block_num = blocks_.size(), cur_block = 0;
    for (auto it = blocks_.begin(); it != blocks_.end(); ++it) {
        std::cout << "BlockManager: Flushing blocks (" << ++cur_block 
            << "/" << block_num << ")\r" << std::flush;
        if (it->second->dirty) {
            disk_->write_disk_section(
                CYLINDER(it->first), SECTION(it->first), 
                BLOCK_SIZE, it->second->data
            );
        }
        delete it->second;
    }
    std::cout << std::endl;
    while (!free_data_.empty()) {
        delete free_data_.front();
        free_data_.pop();
    }
    sem_destroy(&lock_);
}

void BlockManager::dirtify(blockid_t block) {
    if (block == 0) return;
    if (check_block_range_(block) < 0) return;
    LOCK();
    auto it = blocks_.find(block);
    if (it != blocks_.end()) {
        it->second->dirty = true;
    }
}

BlockManager::map_iter_t BlockManager::allocate_() {
    map_iter_t iter;
    if (free_list_head_() == 0) { // Allocate new block
        if (!incr_next_block()) {
            std::cerr << "BlockManager: Disk is full" << std::endl;
            return blocks_.end();
        }
        iter = load_block_(next_block_(), false);
    } else { // Reuse free block
        iter = load_block_(free_list_head_(), true);
        auto free_block = reinterpret_cast<FreeBlock*>(iter->second->data);
        if (free_block->magic == FreeBlock::MAGIC && free_block->version == version()) {
            free_list_head_() = free_block->next;
        }
        else free_list_head_() = 0;
    }
    memset(iter->second->data, 0, BLOCK_SIZE);
    iter->second->dirty = true;
    iter->second->refcnt = 1;
    return iter;
}

void BlockManager::unref_block(blockid_t block) {
    if (block == 0) return;
    if (check_block_range_(block) < 0) return;
    LOCK();
    auto it = blocks_.find(block);
    if (it != blocks_.end()) {
        --it->second->refcnt;
        if (it->second->refcnt <= 0 && blocks_.size() > MAX_DATA_POOL_SIZE) {
            flush_block_(it);
            release_block_(it);
        }
    }
}

void BlockManager::free_block(blockid_t block) {
    if (block == 0) return;
    if (check_block_range_(block) < 0) return;
    LOCK();
    auto iter = load_block_(block);
    auto data = iter->second;
    data->dirty = true;
    data->refcnt = 0;
    auto free_block = reinterpret_cast<FreeBlock*>(data->data);
    if (free_block->magic == FreeBlock::MAGIC && free_block->version == version()) {
        std::cerr << "BlockManager: Block " << block << " is already free" << std::endl;
        return;
    }
    free_block->magic = FreeBlock::MAGIC;
    // std::cout << "current free list head: " << free_list_head_() << std::endl;
    free_block->next = free_list_head_();
    // std::cout << "current free block next: " << free_block->next << std::endl;
    free_block->version = version();
    free_block->id = block;
    free_list_head_() = block;
}

void BlockManager::flush() {
    LOCK();
    size_t block_num = blocks_.size(), cur_block = 0;
    for (auto it = blocks_.begin(); it != blocks_.end(); ++it) {
        std::cout << "BlockManager: Flushing blocks (" << ++cur_block 
            << "/" << block_num << ")\r" << std::flush;
        flush_block_(it);
    }
    std::cout << std::endl;
}

bool BlockManager::incr_next_block() {
    blockid_t block = next_block_();
    int32_t cylinder = CYLINDER(block);
    int32_t section = SECTION(block);
    if (cylinder == disk_->cylinder_num()) {
        return false;
    }
    ++section;
    if (section == disk_->section_num()) {
        section = 0;
        ++cylinder;
    }
    next_block_() = (uint64_t(cylinder) << 32) | section;
    return cylinder != disk_->cylinder_num();
}

BlockManager::map_iter_t BlockManager::load_block_(blockid_t block, bool read) {
    auto it = blocks_.find(block);
    if (it == blocks_.end()) {
        Data* data = get_free_data_();
        if (read) {
            disk_->read_disk_section(CYLINDER(block), SECTION(block), data->data);
        } else {
            memset(data->data, 0, BLOCK_SIZE);
        }
        data->dirty = false;
        data->refcnt = 0;
        it = blocks_.insert({block, data}).first;
    } else {
        if (it->second->refcnt < 0) it->second->refcnt = 0;
    }
    return it;
}

void BlockManager::release_block_(map_iter_t block) {
    if (free_data_.size() < MAX_DATA_POOL_SIZE) {
        free_data_.push(block->second);
    } else {
        delete block->second;
    }
    blocks_.erase(block);
}

void BlockManager::flush_block_(map_iter_t block) {
    if (block->second->dirty && block->second->refcnt == 0) {
        disk_->write_disk_section(
            CYLINDER(block->first), SECTION(block->first), 
            BLOCK_SIZE, block->second->data
        );
        block->second->dirty = false;
    }
}

BlockManager::Data* BlockManager::get_free_data_() {
    Data* data;
    if (!free_data_.empty()) {
        data = free_data_.front();
        free_data_.pop();
        return data;
    }
    if (blocks_.size() < MAX_DATA_POOL_SIZE) {
        return new Data;
    }
    map_iter_t it;
    for (it = blocks_.begin(); it != blocks_.end(); ++it) {
        if (it->second->refcnt == 0) {
            flush_block_(it);
            data = it->second;
            blocks_.erase(it);
            return data;
        }
    }
    return new Data;
}

int BlockManager::check_block_range_(blockid_t block) {
    if (block <= superblock_->block_end) {
        return 0;
    }
    std::cerr << "BlockManager: Invalid block " << block << std::endl;
    return -1;
}