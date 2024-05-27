#pragma once
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <vector>
#include <string>
#include <unordered_map>
#include <semaphore.h>

#include "errorcode.h"
#include "inodefile.h"
#include "userfile.h"
#include "directory.h"

constexpr blockid_t ROOT_INODE = 1;

class FileSystem {
    struct node_t {
        int rwcnt; // pos for read, neg for write
        int refcnt;
        sem_t sem;
        InodeFile* file;
        Directory* dir;

        node_t(InodeFile* file, blockid_t parent); // parent != 0 for initializing directory
        ~node_t();

        bool try_lock(bool write);
        void unlock();
    };

public:
    class WorkingDir {
        friend FileSystem;
        WorkingDir(FileSystem* fs, node_t* node, uint32_t user): 
            user_(user), fs_(fs), node_(node), active_file_(fs->block_mgr()) {}

    public:
        ecode_t create_file(const char* filename);
        ecode_t create_dir(const char* dirname);
        ecode_t remove(const char* name);
        ecode_t remove_dir(const char* dirname);
        ecode_t change_dir(const char* path);
        ecode_t list_dir(std::vector<std::string>& list);
        ecode_t chmod(const char* name, uint16_t mode);
        ecode_t chown(const char* name, uint32_t owner);
        ecode_t rename(const char* oldname, const char* newname);

        ecode_t acquire_file(const char* filename, bool write);
        void release_file();
        
        InodeFile& active_file() { return active_file_; }
        FileSystem* fs() const { return fs_; }
        uint32_t user() const { return user_; }

    private:
        ecode_t open_file_(const char* filename, bool write);

        uint32_t user_;
        FileSystem* fs_;
        node_t* node_;
        InodeFile active_file_;
    };

    FileSystem(RemoteDisk* disk, bool create = false);
    ~FileSystem();

    WorkingDir* open_working_dir(const char* username);
    void close_working_dir(WorkingDir*& wd);

    ecode_t add_user(const char* username, uint32_t& uid);
    ecode_t remove_user(uint32_t uid);
    ecode_t list_users(std::vector<std::string>& list);

    void flush();
    ecode_t format();

    BlockManager* block_mgr() const { return block_mgr_; }

private:
    friend WorkingDir;

    void format_();
    void load_();
    void close_();

    node_t* load_node_(blockid_t inode);
    void release_node_(node_t* node);

    ecode_t change_working_dir_(blockid_t inode, WorkingDir* wd);
    ecode_t walk_and_acquire_(node_t *node, std::vector<node_t*> &nodes);

    ecode_t remove_(blockid_t inode, uint32_t user);

    std::unordered_map<blockid_t, node_t*> nodes_;

    RemoteDisk* disk_;
    BlockManager* block_mgr_;
    UserFile* userfile_;

    sem_t lock_;
};

using WorkingDir = FileSystem::WorkingDir;

#endif // !FILESYSTEM_H