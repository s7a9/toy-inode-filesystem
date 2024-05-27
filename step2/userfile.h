#pragma once
#ifndef USERFILE_H
#define USERFILE_H

#include <vector>
#include <cstdint>

#include "inodefile.h"

constexpr size_t MAX_USERNAME_LEN = 32;

struct UserData {
    size_t len; // 0 for deleted
    char username[MAX_USERNAME_LEN];
};

class UserFile {
public:
    UserFile(InodeFile* file);
    ~UserFile();

    uint32_t add_user(const char* username);
    int remove_user(uint32_t uid);
    uint32_t lookup(const char* username) const;
    const char* get_username(uint32_t uid) const;
    int set_username(uint32_t uid, const char* username);
    void list_users(std::vector<std::string>& list) const;

    InodeFile* file() const { return file_; }

private:
    std::vector<UserData> users_;
    InodeFile* file_;
};

#endif // !USERFILE_H