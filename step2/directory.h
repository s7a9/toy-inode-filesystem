#pragma once
#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <vector>
#include <string>

#include "inodefile.h"
#include "userfile.h"

constexpr size_t MAX_FILENAME_LEN = 32;

struct DirectoryEntry {
    size_t len; // 0 for deleted
    char filename[MAX_FILENAME_LEN];
    blockid_t inode;
};

class Directory {
public:
    Directory(InodeFile* file, blockid_t parent = 0);
    ~Directory();

    blockid_t lookup(const char* filename) const;
    const char* lookup(blockid_t inode) const;
    int add_entry(const char* filename, blockid_t inode);
    int remove_entry(const char* filename);
    int list(std::vector<std::string>& list) const;

    InodeFile* file() const { return file_; }

private:
    std::vector<DirectoryEntry> entries_;
    InodeFile* file_;
};

#endif // !DIRECTORY_H