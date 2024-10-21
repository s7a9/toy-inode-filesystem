#include "inodefile.h"

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <ctime>
#include <iomanip>

class TempData {
public:
    TempData(BlockManager* block_mgr): block_mgr(block_mgr), cur_offset(0), 
        last_block(nullptr), last_block_id(0) {}

    ~TempData() {
        if (last_block != nullptr) {
            block_mgr->unref_block(last_block_id);
        }
        for (auto& iter : cached_data) {
            block_mgr->unref_block(iter.first);
        }
    }

    bool write(const char* buf, size_t size) {
        size_t write_size = 0;
        while (write_size < size) {
            if (last_block == nullptr) {
                last_block = block_mgr->allocate<InodeDataBlock>(last_block_id);
                if (last_block == nullptr) return false;
                last_block->magic = InodeDataBlock::MAGIC;
                cached_data[last_block_id] = last_block;
                data_ids.push_back(last_block_id);
            }
            size_t write = std::min(size - write_size, InodeDataSize - cur_offset);
            memcpy(last_block->data + cur_offset, buf + write_size, write);
            block_mgr->dirtify(last_block_id);
            write_size += write;
            cur_offset += write;
            if (cur_offset == InodeDataSize) {
                cur_offset = 0;
                last_block = nullptr;
            }
        }
        return true;
    }

    void move_to(std::unordered_map<blockid_t, InodeDataBlock*>& data,
        std::vector<blockid_t>& ids, size_t start) {
        while (ids.size() > start) {
            auto id = ids.back();
            // std::cout << "free block: " << id << std::endl;
            block_mgr->free_block(id);
            data.erase(id);
            ids.pop_back();
        }
        for (auto id : data_ids) {
            // std::cout << "move block: " << id << std::endl;
            data[id] = cached_data[id];
            ids.push_back(id);
        }
        data_ids.clear();
        cached_data.clear();
        last_block = nullptr;
        last_block_id = 0;
    }

    BlockManager* block_mgr;
    size_t cur_offset;
    InodeDataBlock* last_block;
    blockid_t last_block_id;
    std::unordered_map<blockid_t, InodeDataBlock*> cached_data;
    std::vector<blockid_t> data_ids;
};

InodeFile::InodeFile(BlockManager* block_mgr):
    block_mgr_(block_mgr), inode_(nullptr), inode_block_(0) {}

InodeFile::InodeFile(BlockManager* block_mgr, blockid_t inode_block):
    block_mgr_(block_mgr), inode_(nullptr), inode_block_(0) {
    open(inode_block);
}

InodeFile::~InodeFile() {
    close();
}

bool InodeFile::open(blockid_t inode_block) {
    if (is_open()) close();
    inode_ = block_mgr_->load<InodeBlock>(inode_block);
    if (inode_->magic != InodeBlock::MAGIC) {
        std::cerr << "InodeFile::open: Bad magic number\n";
        block_mgr_->unref_block(inode_block);
        return false;
    }
    inode_block_ = inode_block;
    if (!load_entries_()) {
        std::cerr << "InodeFile::open: Failed to load entries\n";
        close();
        return false;
    }
    inode_->atime = time(nullptr);
    return true;
}

blockid_t InodeFile::create(uint32_t owner, uint16_t mode, uint16_t type) {
    if (is_open()) close();
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
    save_entries_();
    for (auto& iter : cached_data_) {
        block_mgr_->unref_block(iter.first);
    }
    block_mgr_->dirtify(inode_block_);
    block_mgr_->unref_block(inode_block_);
    cached_data_.clear();
    data_ids_.clear();
    entry_ids_.clear();
    inode_ = nullptr;
    inode_block_ = 0;
}

size_t InodeFile::size() const {
    if (inode_block_ == 0) return 0;
    return inode_->size;
}

size_t InodeFile::read(char* buf, size_t size, size_t offset) {
    if (inode_block_ == 0) return 0;
    if (offset + size > inode_->size) return 0;
    inode_->atime = time(nullptr);
    size = std::min(size, (size_t)inode_->size - offset);
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
    inode_->mtime = inode_->atime = time(nullptr);
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

size_t InodeFile::insert(const char* buf, size_t size, size_t offset) {
    if (inode_block_ == 0) return 0;
    if (offset > inode_->size) return 0;
    inode_->mtime = inode_->atime = time(nullptr);
    size_t index = offset / InodeDataSize;
    size_t offset_in_block = offset % InodeDataSize;
    size_t remaining_size = inode_->size - offset;
    auto data = load_data_(index, true);
    // Construct a temporary buffer to hold the data
    TempData temp_data(block_mgr_);
    if (!temp_data.write(data->data, offset_in_block)) return 0;
    if (!temp_data.write(buf, size)) return 0;
    size_t i = index;
    while (remaining_size > 0) {
        data = load_data_(i, true);
        size_t write_size = std::min(InodeDataSize - offset_in_block, remaining_size);
        if (!temp_data.write(data->data + offset_in_block, write_size)) return 0;
        offset_in_block = 0;
        remaining_size -= write_size;
    }
    // Move the temporary buffer to the actual data
    temp_data.move_to(cached_data_, data_ids_, index);
    inode_->size += size;
    return size;
}

size_t InodeFile::remove(size_t size, size_t offset) {
    if (inode_block_ == 0) return 0;
    if (offset >= inode_->size) return 0;
    inode_->mtime = inode_->atime = time(nullptr);
    size = std::min(size, (size_t)inode_->size - offset);
    size_t index = offset / InodeDataSize;
    size_t offset_in_block = offset % InodeDataSize;
    size_t remaining_size = inode_->size - offset - size;
    size_t delete_size = size;
    auto data = load_data_(index, false);
    // Construct a temporary buffer to hold the data
    TempData temp_data(block_mgr_);
    if (!temp_data.write(data->data, offset_in_block)) return 0;
    size_t i = index;
    do { // Skip the removed blocks
        if (offset_in_block + delete_size < InodeDataSize) {
            offset_in_block += delete_size;
            break;
        }
        delete_size -= InodeDataSize - offset_in_block;
        offset_in_block = 0;
    } while (++i < data_ids_.size());
    // Copy rest of the data
    while (remaining_size > 0) {
        data = load_data_(i, false);
        if (data == nullptr) return 0;
        size_t read_size = std::min(InodeDataSize - offset_in_block, remaining_size);
        if (!temp_data.write(data->data + offset_in_block, read_size)) return 0;
        remaining_size -= read_size;
        offset_in_block = 0;
        ++i;
    }
    // Move the temporary buffer to the actual data
    temp_data.move_to(cached_data_, data_ids_, index);
    inode_->size -= size;
    return size;
}

size_t InodeFile::readall(char* buf) {
    return read(buf, inode_->size, 0);
}

bool InodeFile::removeall() {
    if (inode_block_ == 0) return false;
    inode_->mtime = inode_->atime = time(nullptr);
    inode_->size = 0;
    for (auto id : data_ids_) {
        block_mgr_->free_block(id);
    }
    cached_data_.clear();
    data_ids_.clear();
    return true;
}

bool InodeFile::truncate(size_t size) {
    if (inode_block_ == 0) return false;
    size_t id_len = (size + InodeDataSize - 1) / InodeDataSize;
    inode_->mtime = inode_->atime = time(nullptr);
    if (size >= inode_->size) { // get more data blocks
        for (size_t i = data_ids_.size(); i < id_len; ++i) {
            auto data = load_data_(i, true);
            if (data == nullptr) return false;
        }
    } else { // free unused data blocks
        for (size_t i = id_len; i < data_ids_.size(); ++i) {
            block_mgr_->free_block(data_ids_[i]);
        }
        data_ids_.resize(id_len);
    }
    inode_->size = size;
    return true;
}

bool InodeFile::set_mode(uint16_t mode) {
    if (inode_block_ == 0) return false;
    inode_->mtime = inode_->atime = time(nullptr);
    inode_->mode = mode;
    return true;
}

bool InodeFile::set_owner(uint32_t owner) {
    if (inode_block_ == 0) return false;
    inode_->mtime = inode_->atime = time(nullptr);
    inode_->owner = owner;
    return true;
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
    size_t data_num = (inode_->size + InodeDataSize - 1) / InodeDataSize;
    cached_data_.clear();
    if (data_num == 0) return true;
    cached_data_.reserve(data_num);
    entry_ids_.clear();
    for (size_t i = 0; i < INODE_DIRECT_BLOCK; ++i) {
        data_ids_.push_back(inode_->direct[i]);
        if (--data_num == 0) return true;
    }
    if (!load_entries_(1, inode_->indirect, data_num)) return false;
    if (!load_entries_(2, inode_->double_indirect, data_num)) return false;
    if (!load_entries_(3, inode_->triple_indirect, data_num)) return false;
    return true;
}

bool InodeFile::load_entries_(int level, blockid_t entry_id, size_t& data_num) {
    if (data_num == 0) return true;
    if (entry_id == 0) return false;
    auto entry = block_mgr_->load<InodeEntryBlock>(entry_id);
    entry_ids_.push_back(entry_id);
    if (entry->magic != InodeEntryBlock::MAGIC) {
        std::cerr << "InodeFile::load_entries_: Bad magic number\n";
        block_mgr_->unref_block(entry_id);
        return false;
    }
    for (size_t i = 0; i < entry->count; ++i) {
        if (level == 1) {
            data_ids_.push_back(entry->children[i]);
            if (--data_num == 0) {
                block_mgr_->unref_block(entry_id);
                return true;
            }
        } else {
            if (!load_entries_(level - 1, entry->children[i], data_num)) {
                block_mgr_->unref_block(entry_id);
                return false;
            }
        }
    }
    block_mgr_->unref_block(entry_id);
    return true;
}

bool InodeFile::save_entries_() {
    if (inode_block_ == 0) return false;
    size_t i = 0;
    for (; i < INODE_DIRECT_BLOCK && i < data_ids_.size(); ++i) {
        inode_->direct[i] = data_ids_[i];
        // std::cout << "direct: " << inode_->direct[i] << std::endl;
    }
    inode_->indirect = save_entries_(1, i);
    inode_->double_indirect = save_entries_(2, i);
    inode_->triple_indirect = save_entries_(3, i);
    // free unused entry blocks
    for (size_t j = 0; j < entry_ids_.size(); ++j) {
        block_mgr_->free_block(entry_ids_[j]);
    }
    return i == data_ids_.size();
}

blockid_t InodeFile::save_entries_(int level, size_t& i) {
    if (i == data_ids_.size()) return 0;
    // std::cerr << "InodeFile::save_entries_: Saving entries: " << level << std::endl;
    InodeEntryBlock* entry;
    blockid_t entry_id;
    if (entry_ids_.empty()) {
        entry = block_mgr_->allocate<InodeEntryBlock>(entry_id);
    } else {
        entry_id = entry_ids_.back();
        entry = block_mgr_->load<InodeEntryBlock>(entry_id);
        block_mgr_->dirtify(entry_id);
        entry_ids_.pop_back();
    }
    // std::cerr << "InodeFile::save_entries_: Entry block: " << entry_id << std::endl;
    if (entry == nullptr) return 0;
    entry->magic = InodeEntryBlock::MAGIC;
    entry->count = 0;
    size_t count = 0;
    while (i < data_ids_.size() && count < InodeEntryNum) {
        // std::cerr << "InodeFile::save_entries_: Child: " << data_ids_[i] << std::endl;
        if (level == 1) {
            entry->children[count] = data_ids_[i];
            ++count;
            ++i;
        } else {
            entry->children[count] = save_entries_(level - 1, i);
            if (entry->children[count] == 0) {
                block_mgr_->free_block(entry_id);
                return 0;
            }
            ++count;
        }
    }
    entry->count = count;
    block_mgr_->unref_block(entry_id);
    return entry_id;
}

std::string InodeFile::dump() const {
    static const char* type_strs[] = {
        "Regular", "Directory", "Symlink",
    };
    static const char* mode_strs[] = {
        "---", "--r", "-w-", "-wr", "x--", "x-r", "xw-", "xwr",
    };
    if (inode_block_ == 0) return "File not open.\n";
    std::stringstream ss;
    ss << "InodeFile: inode=" << inode_block_ << ", size=" << inode_->size
        << ", owner=" << inode_->owner << ", mode=" << mode_strs[inode_->mode >> 3] 
        << mode_strs[inode_->mode & 0x7] << ", type=" << type_strs[inode_->type] 
        << ", nlink=" << inode_->nlink << std::endl;
    ss << "A " << std::put_time(std::localtime((time_t*)&inode_->atime), "%c %Z") << std::endl
        << "M " << std::put_time(std::localtime((time_t*)&inode_->mtime), "%c %Z") << std::endl
        << "C " << std::put_time(std::localtime((time_t*)&inode_->ctime), "%c %Z") << std::endl;
    if (data_ids_.empty()) {
        ss << "No data blocks\n";
    } else {
        ss << "Datablocks: id= " << std::hex;
        for (size_t i = 0; i < data_ids_.size(); ++i) {
            ss << data_ids_[i] << " ";
        }
        ss << std::endl;
    }
    for (size_t i = 0; i < entry_ids_.size(); ++i) {
        auto entry = block_mgr_->load<InodeEntryBlock>(entry_ids_[i]);
        ss << "EntryBlock: id=" << entry_ids_[i] << ", count=" << std::dec << entry->count
            << ", parent=" << std::hex << entry->parent << std::endl << "  Children id= ";
        for (size_t j = 0; j < entry->count; ++j) {
            ss << entry->children[j] << " ";
        }
        ss << std::endl;
    }
    return ss.str();
}