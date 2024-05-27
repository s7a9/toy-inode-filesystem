#include "directory.h"

#include <cstring>

Directory::Directory(InodeFile* file, blockid_t parent): file_(file) {
    // Load directory entries
    if (parent) {
        entries_.resize(2);
        entries_[0].len = 1;
        strcpy(entries_[0].filename, ".");
        entries_[0].inode = file->inode_id();
        entries_[1].len = 2;
        strcpy(entries_[1].filename, "..");
        entries_[1].inode = parent;
    } else {
        entries_.resize(file_->size() / sizeof(DirectoryEntry));
        for (size_t i = 0; i < entries_.size(); ++i) {
            file_->read((char*)&entries_[i], sizeof(DirectoryEntry), i * sizeof(DirectoryEntry));
        }
    }
}

Directory::~Directory() {
    // Save directory entries
    for (size_t i = 0; i < entries_.size(); ++i) {
        file_->write((char*)&entries_[i], sizeof(DirectoryEntry), i * sizeof(DirectoryEntry));
    }
}

blockid_t Directory::lookup(const char* filename) const {
    for (const DirectoryEntry& entry : entries_) {
        if (entry.len != 0 && strcmp(entry.filename, filename) == 0) {
            return entry.inode;
        }
    }
    return 0;
}

int Directory::add_entry(const char* filename, blockid_t inode) {
    size_t len = strlen(filename);
    if (len >= MAX_FILENAME_LEN) {
        return -1;
    }
    for (DirectoryEntry& entry : entries_) {
        if (entry.len == 0) {
            entry.len = len;
            strcpy(entry.filename, filename);
            entry.inode = inode;
            return 0;
        }
    }
    entries_.resize(entries_.size() + 1);
    DirectoryEntry& entry = entries_.back();
    entry.len = len;
    strcpy(entry.filename, filename);
    entry.inode = inode;
    return 0;
}

int Directory::remove_entry(const char* filename) {
    for (DirectoryEntry& entry : entries_) {
        if (entry.len != 0 && strcmp(entry.filename, filename) == 0) {
            entry.len = 0;
            return 0;
        }
    }
    return -1;
}

int Directory::list(std::vector<std::string>& list) const {
    for (const DirectoryEntry& entry : entries_) {
        if (entry.len != 0) {
            list.push_back(std::string(entry.filename, entry.len));
        }
    }
    return 0;
}