#include "userfile.h"

#include <cstring>
#include <iostream>

UserFile::UserFile(InodeFile* file): file_(file) {
    if (file_->size() % sizeof(UserData) != 0) {
        std::cerr << "UserFile: Invalid file size" << std::endl;
        return;
    }
    users_.resize(file_->size() / sizeof(UserData));
    for (size_t i = 0; i < users_.size(); ++i) {
        file_->read((char*)&users_[i], sizeof(UserData), i * sizeof(UserData));
    }
    if (users_.empty()) { // add root user
        // std::cout << "UserFile: Adding root user" << std::endl;
        UserData root = {0, "root"};
        users_.push_back(root);
    }
}

UserFile::~UserFile() {
    for (size_t i = 0; i < users_.size(); ++i) {
        file_->write((char*)&users_[i], sizeof(UserData), i * sizeof(UserData));
    }
    delete file_;
}

uint32_t UserFile::add_user(const char* username) {
    size_t len = strlen(username);
    if (len >= MAX_USERNAME_LEN) {
        std::cerr << "UserFile: Username too long" << std::endl;
        return 0;
    }
    UserData user = {len, ""};
    strcpy(user.username, username);
    users_.push_back(user);
    return users_.size() - 1;
}

int UserFile::remove_user(uint32_t uid) {
    if (uid == 0) {
        std::cerr << "UserFile: Cannot remove root user" << std::endl;
        return -1;
    }
    if (uid >= users_.size()) {
        std::cerr << "UserFile: Invalid user id" << std::endl;
        return -1;
    }
    users_[uid].len = 0;
    return 0;
}

uint32_t UserFile::lookup(const char* username) const {
    for (size_t i = 0; i < users_.size(); ++i) {
        if (users_[i].len > 0 && strcmp(users_[i].username, username) == 0) {
            return i;
        }
    }
    return 0;
}

const char* UserFile::get_username(uint32_t uid) const {
    if (uid >= users_.size()) {
        std::cerr << "UserFile: Invalid user id" << std::endl;
        return nullptr;
    }
    if (users_[uid].len == 0) {
        return nullptr;
    }
    return users_[uid].username;
}

int UserFile::set_username(uint32_t uid, const char* username) {
    if (uid == 0) {
        std::cerr << "UserFile: Cannot change root username" << std::endl;
        return -1;
    }
    if (uid >= users_.size()) {
        std::cerr << "UserFile: Invalid user id" << std::endl;
        return -1;
    }
    size_t len = strlen(username);
    if (len >= MAX_USERNAME_LEN) {
        std::cerr << "UserFile: Username too long" << std::endl;
        return -1;
    }
    users_[uid].len = len;
    strcpy(users_[uid].username, username);
    return 0;
}

void UserFile::list_users(std::vector<std::string>& list) const {
    for (size_t i = 0; i < users_.size(); ++i) {
        if (users_[i].len > 0) {
            list.push_back(std::to_string(i) + ':' + users_[i].username);
        }
    }
}