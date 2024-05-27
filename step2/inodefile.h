#pragma once
#ifndef INODEFILE_H
#define INODEFILE_H

#include <vector>
#include <string>
#include <unordered_map>

#include "blockmgr.h"

enum InodeFileMode: uint16_t {
    FILE_READ = 0x01,
    FILE_WRITE = 0x02,
    FILE_EXEC = 0x04,
    FILE_OTHER_READ = 0x08,
    FILE_OTHER_WRITE = 0x10,
    FILE_OTHER_EXEC = 0x20,
};

enum InodeFileType: uint16_t {
    TYPE_FILE = 0,
    TYPE_DIR = 1,
    TYPE_SYMLINK = 2,
};

constexpr size_t INODE_DIRECT_BLOCK = 23;
struct InodeBlock {
    static constexpr uint32_t MAGIC = 0x2C1D7C0F;
    uint32_t magic;
    uint32_t owner;
    uint16_t mode;
    uint16_t type;
    uint32_t nlink;
    uint64_t size;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    blockid_t direct[INODE_DIRECT_BLOCK];
    blockid_t indirect;
    blockid_t double_indirect;
    blockid_t triple_indirect;
};

constexpr size_t InodeEntryNum = 30;
struct InodeEntryBlock {
    static constexpr uint32_t MAGIC = 0x2C1D7C10;
    uint32_t magic;
    uint32_t count;
    blockid_t parent;
    blockid_t children[InodeEntryNum];
};
constexpr size_t InodeEntryBlockSize = sizeof(InodeEntryBlock);

struct InodeDataBlock {
    static constexpr uint32_t MAGIC = 0x2C1D7C11;
    uint32_t magic;
    char data[0];
};
constexpr size_t InodeDataSize = BLOCK_SIZE - sizeof(InodeDataBlock);

class InodeFile {
public:
    InodeFile(BlockManager* block_mgr);
    InodeFile(BlockManager* block_mgr, blockid_t inode_block);
    ~InodeFile();

    bool open(blockid_t inode_id);
    blockid_t create(uint32_t owner, uint16_t mode, uint16_t type);
    void close();

    inline bool is_open() const { return inode_ != nullptr; }
    size_t size() const;

    size_t read(char* buf, size_t size, size_t offset);
    size_t write(const char* buf, size_t size, size_t offset);
    size_t insert(const char* buf, size_t size, size_t offset);
    size_t remove(size_t size, size_t offset);

    size_t readall(char* buf);
    bool removeall();

    bool truncate(size_t size);

    bool set_mode(uint16_t mode);
    bool set_owner(uint32_t owner);

    InodeBlock* inode() const { return inode_; }
    blockid_t inode_id() const { return inode_block_; }

    std::string dump() const;
    
private:
    BlockManager* block_mgr_;

    InodeBlock* inode_;
    blockid_t inode_block_;

    std::unordered_map<blockid_t, InodeDataBlock*> cached_data_;
    std::vector<blockid_t> data_ids_;
    std::vector<blockid_t> entry_ids_;

    blockid_t create_failed_();
    InodeDataBlock* load_data_(size_t index, bool create);

    inline bool load_entries_();
    inline bool load_entries_(int level, blockid_t entry_id, size_t& data_num);
    inline bool save_entries_();
    inline blockid_t save_entries_(int level, size_t& i);
};

#endif // !INODEFILE_H