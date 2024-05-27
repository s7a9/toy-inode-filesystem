#pragma once
#ifndef BLOCKMGR_H
#define BLOCKMGR_H

#include <queue>
#include <semaphore.h>
#include <unordered_map>
#include <iostream>

#include "idisk.h"

using blockid_t = uint64_t;

constexpr uint32_t BLOCK_SIZE = SECTION_SIZE;
constexpr size_t MAX_DATA_POOL_SIZE = 1024;
constexpr size_t MAX_ROUTINE_FLUSH_SIZE = 32;

struct SuperBlock {
    static constexpr uint32_t MAGIC = 0x2C1D7C0D;
    uint32_t magic;
    uint32_t block_size;
    blockid_t free_list_head;
    blockid_t root_inode;
    blockid_t block_end;
    uint64_t version;
};

struct FreeBlock {
    static constexpr uint32_t MAGIC = 0x2C1D7C0E;
    uint32_t magic;
    blockid_t next;
    blockid_t id;
    uint64_t version;
};

class BlockManager {
    struct LockGuard {
        LockGuard(sem_t* lock) : lock_(lock) { sem_wait(lock_); }
        ~LockGuard() { sem_post(lock_); }
        sem_t* lock_;
    };

    struct Data {
        bool dirty;
        uint32_t refcnt;
        char data[BLOCK_SIZE];
    };

    using block_map_t = std::unordered_map<blockid_t, Data*>;
    using map_iter_t = block_map_t::iterator;

public:
    BlockManager(RemoteDisk* disk, bool create = false);
    ~BlockManager();

    template <class block_t>
    inline block_t* load(blockid_t block) {
        if (block == 0) return nullptr;
        if (check_block_range_(block) < 0) return nullptr;
        LockGuard lock_guard(&lock_);
        auto iter = load_block_(block);
        iter->second->refcnt++;
        return reinterpret_cast<block_t*>(iter->second->data);
    }

    template <class block_t>
    inline block_t* allocate(blockid_t& block) {
        LockGuard lock_guard(&lock_);
        auto iter = allocate_();
        if (iter == blocks_.end()) {
            block = 0;
            return nullptr;
        }
        block = iter->first;
        return reinterpret_cast<block_t*>(iter->second->data);
    }

    void dirtify(blockid_t block);

    void unref_block(blockid_t block);
    
    void free_block(blockid_t block);

    void flush();

private:
    map_iter_t allocate_();

    blockid_t& next_block_() { return superblock_->block_end; }
    blockid_t& free_list_head_() { return superblock_->free_list_head; }
    blockid_t& root_inode() { return superblock_->root_inode; }
    uint64_t version() { return superblock_->version; }

    bool incr_next_block();

    map_iter_t load_block_(blockid_t block, bool read = true);
    void release_block_(map_iter_t block);
    void flush_block_(map_iter_t block);
    Data* get_free_data_();
    int check_block_range_(blockid_t block);

    RemoteDisk* disk_;

    block_map_t blocks_;
    std::queue<Data*> free_data_;

    SuperBlock* superblock_;
    sem_t lock_;
};

#endif // !BLOCKMGR_H