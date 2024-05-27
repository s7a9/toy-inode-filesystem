#include "inodefile.h"

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cmath>

InodeFile::InodeFile(BlockManager* block_mgr):
    block_mgr_(block_mgr), inode_(nullptr), inode_block_(0) {}

InodeFile::InodeFile(BlockManager* block_mgr, blockid_t inode_block):
    block_mgr_(block_mgr) {
    open(inode_block);
}

InodeFile::~InodeFile() {
    close();
}

bool InodeFile::open(blockid_t inode_block) {
    if (inode_block == 0) return false;
    inode_ = block_mgr_->load<InodeBlock>(inode_block);
    if (inode_->magic != InodeBlock::MAGIC) {
        std::cerr << "InodeFile::open: Bad magic number\n";
        block_mgr_->unref_block(inode_block);
        return false;
    }
    inode_block_ = inode_block;
    if (!load_entries_()) {
        close();
        return false;
    }
    return true;
}

blockid_t InodeFile::create(uint32_t owner, InodeFileMode mode, InodeFileType type) {
    if (inode_block_ != 0) return create_failed_();
    blockid_t inode_block;
    inode_ = block_mgr_->allocate<InodeBlock>(inode_block);
    if (inode_block == 0) return create_failed_();
    // memset(inode_, 0, sizeof(InodeBlock));
    inode_->magic = InodeBlock::MAGIC;
    inode_->owner = owner;
    inode_->mode = mode;
    inode_->type = type;
    inode_->nlink = 1;
    inode_->atime = inode_->mtime = inode_->ctime = time(nullptr);
    return inode_block_ = inode_block;
}

void InodeFile::close() {
    if (inode_block_ == 0) return;
    block_mgr_->dirtify(inode_block_);
    block_mgr_->unref_block(inode_block_);
    inode_ = nullptr;
    inode_block_ = 0;
    for (auto& iter : cached_data_) {
        block_mgr_->unref_block(iter.first);
    }
    save_entries_();
    cached_data_.clear();
    data_ids_.clear();
}

size_t InodeFile::size() const {
    if (inode_block_ == 0) return 0;
    return inode_->size;
}

size_t InodeFile::read(char* buf, size_t size, size_t offset) {
    if (inode_block_ == 0) return 0;
    if (offset >= inode_->size) return 0;
    inode_->atime = time(nullptr);
    size = std::min(size, inode_->size - offset);
    size_t read_size = 0;
    size_t index = offset / InodeDataSize;
    size_t offset_in_block = offset % InodeDataSize;
    while (read_size < size) {
        auto data = load_data_(index, false);
        if (data == nullptr) return read_size;
        size_t read = std::min(size - read_size, InodeDataSize - offset_in_block);
        memcpy(buf + read_size, data->data + offset_in_block, read);
        read_size += read;
        offset_in_block = 0;
        ++index;
    }
    return read_size;
}

size_t InodeFile::write(const char* buf, size_t size, size_t offset) {
    if (inode_block_ == 0) return 0;
    if (offset > inode_->size) return 0;
    inode_->mtime = inode_->ctime = inode_->atime = time(nullptr);
    size_t write_size = 0;
    size_t index = offset / InodeDataSize;
    size_t offset_in_block = offset % InodeDataSize;
    while (write_size < size) {
        auto data = load_data_(index, true);
        if (data == nullptr) return write_size;
        size_t write = std::min(size - write_size, InodeDataSize - offset_in_block);
        memcpy(data->data + offset_in_block, buf + write_size, write);
        block_mgr_->dirtify(data_ids_[index]);
        write_size += write;
        offset_in_block = 0;
        ++index;
    }
    if (offset + size > inode_->size) {
        inode_->size = offset + size;
    }
    return write_size;
}

#define INSERT_END return insert_size

size_t InodeFile::insert(const char* buf, size_t size, size_t offset) {
    if (inode_block_ == 0) return 0;
    if (offset > inode_->size) return 0;
    inode_->mtime = inode_->ctime = inode_->atime = time(nullptr);
    size_t insert_size = 0;
    size_t index = offset / InodeDataSize;
    size_t offset_in_block = offset % InodeDataSize;
    char buffer[InodeDataSize];
    std::vector<blockid_t> new_data_ids;
    // Insert and swap out data in the first block
    auto data = load_data_(index, true);
    if (data == nullptr) return insert_size;
    size_t buffer_size = std::min(size, InodeDataSize - offset_in_block);
    memcpy(buffer, data->data + offset_in_block, buffer_size);
    memcpy(data->data + offset_in_block, buf, buffer_size);
    block_mgr_->dirtify(data_ids_[index]);
    insert_size += buffer_size;
    index++; // Move to the next block
    // Create new data blocks if size exceeds the block size
    if (insert_size + InodeDataSize <= size) {
        do {
            blockid_t new_data_id;
            auto new_data = block_mgr_->allocate<InodeDataBlock>(new_data_id);
            if (new_data_id == 0) {
                std::cerr << "InodeFile::insert: Failed to allocate new data block\n";
                INSERT_END;
            }
            new_data->magic = InodeDataBlock::MAGIC;
            new_data_ids.push_back(new_data_id);
            memcpy(new_data->data, buf + insert_size, InodeDataSize);
            block_mgr_->dirtify(new_data_id);
            insert_size += InodeDataSize;
        } while (insert_size + InodeDataSize <= size);
    }
    // Merge buffer and remaining data
    if (insert_size < size) {
        size_t remaining_size = size - insert_size;
        if (remaining_size + buffer_size >= InodeDataSize) { // Need a new data block
            size_t buffer_moved_size = InodeDataSize - remaining_size;
            blockid_t new_data_id;
            auto new_data = block_mgr_->allocate<InodeDataBlock>(new_data_id);
            if (new_data_id == 0) {
                std::cerr << "InodeFile::insert: Failed to allocate new data block\n";
                INSERT_END;
            }
            new_data->magic = InodeDataBlock::MAGIC;
            new_data_ids.push_back(new_data_id);
            memcpy(new_data->data, buf + insert_size, remaining_size);
            memcpy(new_data->data + remaining_size, buffer, buffer_moved_size);
            block_mgr_->dirtify(new_data_id);
            insert_size += InodeDataSize;
            memmove(buffer, buffer + buffer_moved_size, buffer_size - buffer_moved_size);
            buffer_size -= buffer_moved_size;
        } else {
            memmove(buffer + remaining_size, buffer, buffer_size);
            memcpy(buffer, buf + insert_size, remaining_size);
            buffer_size += remaining_size;
        }
    }
    // Insert buffer into rest of the data blocks
    if (buffer_size > 0) {
        size_t last_block_size = inode_->size % InodeDataSize;
        size_t move_size = InodeDataSize - buffer_size;
        size_t i = data_ids_.size() - 1;
        InodeDataBlock* next_data = nullptr;
        auto data = load_data_(i, false);
        if (last_block_size >= move_size) {
            next_data = load_data_(i + 1, true);
        }
        while (i-- >= index) {
            if (next_data) {
                memcpy(next_data->data, data->data + move_size, buffer_size);
            }
            memmove(data->data + buffer_size, data->data, move_size);
            next_data = data;
            data = load_data_(i, false);
        }
        memcpy(data->data, buffer, buffer_size);
    }
    data_ids_.insert(data_ids_.begin() + index, new_data_ids.begin(), new_data_ids.end());
    inode_->size += insert_size;
    return insert_size;
}

size_t InodeFile::remove(size_t size, size_t offset) {
    if (inode_block_ == 0) return 0;
    if (offset >= inode_->size) return 0;
    inode_->mtime = inode_->ctime = inode_->atime = time(nullptr);
    size = std::min(size, inode_->size - offset);
    size_t remove_size = 0;
    size_t index = offset / InodeDataSize;
    size_t offset_in_block = offset % InodeDataSize;
    remove_size = std::min(size, InodeDataSize - offset_in_block);
    // Remove middle data blocks
    while (remove_size + InodeDataSize <= size) {
        block_mgr_->free_block(data_ids_[index + 1]);
        data_ids_.erase(data_ids_.begin() + index + 1);
        remove_size += InodeDataSize;
    }
    // Move data from the next block to current block
    size_t i = index;
    size_t remaining_size = size - remove_size;
    auto data = load_data_(i, false);
    auto next_data = load_data_(i + 1, false);
    block_mgr_->dirtify(data_ids_[i]);
    if (offset_in_block + remaining_size < InodeDataSize) {
        size_t back_offset = offset_in_block + remaining_size;
        memmove(data->data + offset_in_block, data->data + back_offset,
            InodeDataSize - back_offset);
        offset_in_block = back_offset;
    } else if (offset_in_block < remaining_size) {
        size_t move_size = InodeDataSize - remaining_size;
        memcpy(data->data + offset_in_block, next_data->data + remaining_size, move_size);
        offset_in_block += move_size;
        remove_size += move_size;
        remaining_size -= move_size;
        block_mgr_->free_block(data_ids_[i + 1]);
        data_ids_.erase(data_ids_.begin() + i + 1);
    }
    // Move remaining data
    while (i + 1 < data_ids_.size()) {
        data = load_data_(i, false);
        next_data = load_data_(i + 1, false);
        block_mgr_->dirtify(data_ids_[i]);
        memcpy(data->data + offset_in_block, next_data->data, remaining_size);
        memmove(next_data->data, next_data->data + remaining_size, InodeDataSize - remaining_size);
        remove_size += remaining_size;
    }
    inode_->size -= remove_size;
    return remove_size;
}

blockid_t InodeFile::create_failed_() {
    std::cerr << "InodeFile::create_failed_: Cleaning up\n";
    close();
    return 0;
}

InodeDataBlock* InodeFile::load_data_(size_t index, bool create) {
    if (index >= data_ids_.size()) { // Need to create new data block
        if (!create || index > data_ids_.size()) return nullptr;
        blockid_t data_id;
        auto data = block_mgr_->allocate<InodeDataBlock>(data_id);
        if (data_id == 0) return nullptr;
        // memset(data, 0, sizeof(InodeDataBlock)); // block_mgr_->allocate already does this
        data->magic = InodeDataBlock::MAGIC;
        data_ids_.push_back(data_id);
        cached_data_[data_id] = data;
        return data;
    }
    auto datablock_id = data_ids_[index];
    auto it = cached_data_.find(datablock_id);
    if (it != cached_data_.end()) return it->second;
    auto data = block_mgr_->load<InodeDataBlock>(datablock_id);
    if (data->magic != InodeDataBlock::MAGIC) {
        std::cerr << "InodeFile::load_data_: Bad magic number\n";
        block_mgr_->unref_block(datablock_id);
        return nullptr;
    }
    cached_data_[datablock_id] = data;
    return data;
}

bool InodeFile::load_entries_() {
    if (inode_block_ == 0) return false;
    auto entry_num = (inode_->size + InodeEntryNum - 1) / InodeEntryNum;
    cached_data_.clear();
    cached_data_.reserve(entry_num);
    entry_blocks_.clear();
    for (size_t i = 0; i < INODE_DIRECT_BLOCK; ++i) {
        data_ids_.push_back(inode_->direct[i]);
        if (--entry_num == 0) return true;
    }
    if (!load_entries_(1, inode_->indirect, entry_num)) return false;
    if (!load_entries_(2, inode_->double_indirect, entry_num)) return false;
    if (!load_entries_(3, inode_->triple_indirect, entry_num)) return false;
    return true;
}

bool InodeFile::load_entries_(int level, blockid_t entry_id, size_t& entry_num) {
    if (entry_num == 0) return true;
    if (entry_id == 0) return false;
    auto entry = block_mgr_->load<InodeEntryBlock>(entry_id);
    entry_blocks_.push_back(entry);
    if (entry->magic != InodeEntryBlock::MAGIC) {
        std::cerr << "InodeFile::load_entries_: Bad magic number\n";
        block_mgr_->unref_block(entry_id);
        return false;
    }
    for (size_t i = 0; i < entry->count; ++i) {
        if (level == 1) {
            data_ids_.push_back(entry->children[i]);
            if (--entry_num == 0) return true;
        } else {
            if (!load_entries_(level - 1, entry->children[i], entry_num)) {
                return false;
            }
        }
    }
    return true;
}