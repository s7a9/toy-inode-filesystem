#pragma once
#ifndef BPTFILE_H
#define BPTFILE_H

#include <unordered_map>
#include <stack>

#include "blockmgr.h"

enum BPTFileMode: uint16_t {
    FILE_READ = 0x01,
    FILE_WRITE = 0x02,
    FILE_EXEC = 0x04,
    FILE_OTHER_READ = 0x08,
    FILE_OTHER_WRITE = 0x10,
    FILE_OTHER_EXEC = 0x20,
    FILE_SIMPLE = 0x40, // Data is stored in meta block
    FILE_BAD = 0x80, // File is bad
};

enum BPTFileType: uint16_t {
    FILE = 0,
    DIR = 1,
    SYMLINK = 2,
};

struct BPTFileMeta {
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
    blockid_t root;
    blockid_t leaf_head;
    char data[0];
};
constexpr size_t BPTFileMetaSize = BLOCK_SIZE - sizeof(BPTFileMeta);

constexpr size_t BPTNodeM = 17;
struct BPTNodeBlock {
    static constexpr uint32_t MAGIC = 0x2C1D7C10;
    uint32_t magic;
    uint32_t count;
    uint32_t level; // 0 for node right above leaf
    uint32_t keys[BPTNodeM]; // keys are the size of contents in subtree
    uint64_t total_size;
    blockid_t parent;
    blockid_t next;
    blockid_t prev;
    blockid_t children[BPTNodeM];
};

struct BPTLeafBlock {
    static constexpr uint32_t MAGIC = 0x2C1D7C11;
    uint32_t magic;
    uint32_t count;
    blockid_t parent;
    blockid_t next;
    blockid_t prev;
    char data[0];
};
constexpr size_t BPTLeafN = BLOCK_SIZE - sizeof(BPTLeafBlock);

class BPTFile {
public:
    BPTFile(BlockManager* block_mgr);
    BPTFile(BlockManager* block_mgr, blockid_t meta_block);
    ~BPTFile();

    bool open(blockid_t meta_block);
    blockid_t create(uint32_t owner, BPTFileMode mode, BPTFileType type);
    void close();

    inline bool is_open() const { return meta_ != nullptr; }
    size_t size() const;

    size_t read(char* buf, size_t size, size_t offset);
    size_t write(const char* buf, size_t size, size_t offset);
    size_t insert(const char* buf, size_t size, size_t offset);
    size_t remove(size_t size, size_t offset);
    size_t readall(char* buf);
    size_t writeall(const char* buf, size_t size);
    bool removeall();

    bool truncate(size_t size);

    bool set_mode(uint16_t mode);
    bool set_owner(uint32_t owner);

    BPTFileMeta* meta() const { return meta_; }

    void dump() const; // For debugging

private:
    bool simple_;
    BlockManager* block_mgr_;

    blockid_t meta_block_;
    BPTFileMeta* meta_;
    BPTNodeBlock* root_;
    BPTLeafBlock* leaf_head_;

    std::unordered_map<blockid_t, BPTNodeBlock*> cached_nodes_;
    std::unordered_map<blockid_t, BPTLeafBlock*> cached_leaves_;

    inline bool open_failed_();
    inline blockid_t create_failed_();

    BPTNodeBlock* load_node_(blockid_t id);
    BPTLeafBlock* load_leaf_(blockid_t id);

    using bpt_trace_t = std::stack<std::pair<blockid_t,uint32_t>>;
    blockid_t bpt_find_(bpt_trace_t* parents, size_t& offset);

    size_t bpt_read_(char* buf, size_t size, size_t offset);
    size_t bpt_readall_(char* buf);
    size_t bpt_insert_(const char* buf, size_t size, size_t offset);
    size_t bpt_delete_recursive_(blockid_t node_id);
};

#endif // !BPTFILE_H