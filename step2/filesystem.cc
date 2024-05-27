#include "filesystem.h"

#include <cstring>
#include <iostream>

bool test_permission(InodeBlock* inode, uint32_t user, bool write) {
    if (user == 0) {
        return true;
    }
    uint16_t mode;
    if (inode->owner != user) {
        mode = write ? FILE_OTHER_WRITE : FILE_OTHER_READ;
    } else {
        mode = write ? FILE_WRITE : FILE_READ;
    }
    return (inode->mode & mode) != 0;
}

FileSystem::node_t::node_t(InodeFile* file, blockid_t parent): 
    rwcnt(0), refcnt(0), file(file) {
    if (file->inode()->type == TYPE_DIR) {
        dir = new Directory(file, parent);
    } else {
        dir = nullptr;
    }
    // std::cerr << "init node " << file->inode_id() << std::endl;
    sem_init(&sem, 0, 1);
}

FileSystem::node_t::~node_t() {
    // std::cerr << "delete node " << file->inode_id() << std::endl;
    if (dir) { delete dir; }
    if (file) { delete file; }
    sem_destroy(&sem);
}

bool FileSystem::node_t::try_lock(bool write) {
    sem_wait(&sem);
    bool locked = false;
    if (write) {
        if (rwcnt == 0) {
            rwcnt = -1;
            locked = true;
        }
    } else {
        if (rwcnt >= 0) {
            rwcnt++;
            locked = true;
        }
    }
    sem_post(&sem);
    return locked;
}

void FileSystem::node_t::unlock() {
    sem_wait(&sem);
    if (rwcnt > 0) {
        rwcnt--;
    } else {
        rwcnt = 0;
    }
    sem_post(&sem);
}

#define TRYLOCK(ACCESS) if (!node_->try_lock(ACCESS)) { std::cerr << "lock failed\n"; return ERROR_BUSY; }
#define CHKRET(cond, ERR) if (!(cond)) { node_->unlock(); std::cerr << "ecode: " << ERR << std::endl; return ERR; }

ecode_t WorkingDir::create_file(const char* filename) {
    TRYLOCK(true);
    CHKRET(node_->dir->lookup(filename) == 0, ERROR_EXIST);
    blockid_t file_inode = active_file_.create(user_, 033, TYPE_FILE);
    std::cout << "create file: " << filename << " inode: " << file_inode << std::endl;
    CHKRET(file_inode != 0, ERROR_INVALID);
    active_file_.close();
    ecode_t ret = node_->dir->add_entry(filename, file_inode);
    node_->unlock();
    return ret;
}

ecode_t WorkingDir::create_dir(const char* dirname) {
    ecode_t ret;
    TRYLOCK(true);
    CHKRET(node_->dir->lookup(dirname) == 0, ERROR_EXIST);
    blockid_t dir_inode = active_file_.create(user_, 033, TYPE_DIR);
    CHKRET(dir_inode != 0, ERROR_INVALID);
    { // flush directoy info into file
        Directory dir(&active_file_, node_->file->inode_id());
    }
    active_file_.close();
    std::cout << "create dir: " << dirname << " inode: " << dir_inode << std::endl;
    ret = node_->dir->add_entry(dirname, dir_inode);
    node_->unlock();
    return ret;
}

ecode_t WorkingDir::remove(const char* name) {
    TRYLOCK(true);
    blockid_t inode = node_->dir->lookup(name);
    CHKRET(inode != 0, ERROR_NOT_FOUND);
    active_file_.open(inode);
    if (active_file_.inode()->type == TYPE_DIR) {
        active_file_.close();
        node_->unlock();
        return ERROR_NOT_FILE;
    }
    if (!test_permission(active_file_.inode(), user_, true)) {
        active_file_.close();
        node_->unlock();
        return ERROR_PERMISSION;
    }
    node_->dir->remove_entry(name);
    active_file_.removeall();
    active_file_.close();
    fs_->block_mgr()->free_block(inode);
    node_->unlock();
    return 0;
}

ecode_t WorkingDir::remove_dir(const char* dirname) {
    ecode_t ret;
    TRYLOCK(true);
    blockid_t inode = node_->dir->lookup(dirname);
    CHKRET(inode != 0, ERROR_NOT_FOUND);
    ret = fs_->remove_(inode, user_);
    if (ret == 0) {
        node_->dir->remove_entry(dirname);
    }
    node_->unlock();
    return ret;
}

ecode_t WorkingDir::change_dir(const char* path) {
    ecode_t ret;
    TRYLOCK(false);
    blockid_t inode = node_->dir->lookup(path);
    CHKRET(inode != 0, ERROR_NOT_FOUND);
    ret = fs_->change_working_dir_(inode, this);
    node_->unlock();
    return ret;
}

ecode_t WorkingDir::list_dir(std::vector<std::string>& list) {
    TRYLOCK(false);
    node_->dir->list(list);
    node_->unlock();
    return 0;
}

ecode_t WorkingDir::chmod(const char* filename, uint16_t mode) {
    TRYLOCK(false);
    blockid_t inode = node_->dir->lookup(filename);
    CHKRET(inode != 0, ERROR_NOT_FOUND);
    CHKRET(active_file_.open(inode), ERROR_INVALID);
    CHKRET(user_ == 0 || active_file_.inode()->owner == user_, ERROR_PERMISSION);
    active_file_.set_mode(mode);
    active_file_.close();
    node_->unlock();
    return 0;
}

ecode_t WorkingDir::chown(const char* filename, uint32_t owner) {
    TRYLOCK(false);
    blockid_t inode = node_->dir->lookup(filename);
    CHKRET(inode != 0, ERROR_NOT_FOUND);
    CHKRET(active_file_.open(inode), ERROR_INVALID);
    CHKRET(user_ == 0 || active_file_.inode()->owner == user_, ERROR_PERMISSION);
    active_file_.set_owner(owner);
    active_file_.close();
    node_->unlock();
    return 0;
}

ecode_t WorkingDir::acquire_file(const char* filename, bool write) {
    TRYLOCK(write);
    ecode_t ret = open_file_(filename, write);
    if (ret != 0) {
        node_->unlock();
    }
    return ret;
}

ecode_t WorkingDir::rename(const char* oldname, const char* newname) {
    TRYLOCK(true);
    blockid_t inode = node_->dir->lookup(oldname);
    CHKRET(inode != 0, ERROR_NOT_FOUND);
    CHKRET(node_->dir->lookup(newname) == 0, ERROR_EXIST);
    CHKRET(active_file_.open(inode), ERROR_INVALID);
    bool permision = test_permission(active_file_.inode(), user_, true);
    active_file_.close();
    CHKRET(permision, ERROR_PERMISSION);
    node_->dir->remove_entry(oldname);
    node_->dir->add_entry(newname, inode);
    node_->unlock();
    return 0;
}

void WorkingDir::release_file() {
    active_file_.close();
    node_->unlock();
}

ecode_t WorkingDir::open_file_(const char* filename, bool write) {
    blockid_t inode = node_->dir->lookup(filename);
    CHKRET(inode != 0, ERROR_NOT_FOUND);
    if (!active_file_.open(inode)) {
        return ERROR_INVALID;
    }
    if (!test_permission(active_file_.inode(), user_, write)) {
        active_file_.close();
        return ERROR_PERMISSION;
    }
    // std::cout << "open file: " << filename << " inode: " << inode << std::endl;
    return 0;
}

FileSystem::FileSystem(RemoteDisk* disk, bool create): 
    disk_(disk), block_mgr_(nullptr), userfile_(nullptr) {
    sem_init(&lock_, 0, 1);
    if (create) {
        format_();
    } else {
        load_();
    }
}

FileSystem::~FileSystem() {
    close_();
    sem_destroy(&lock_);
}

WorkingDir* FileSystem::open_working_dir(const char* username) {
    uint32_t uid;
    if (strcmp(username, "root") == 0) {
        uid = 0;
    } else {
        uid = userfile_->lookup(username);
        if (uid == 0) {
            return nullptr;
        }
    }
    auto node = load_node_(ROOT_INODE);
    ++node->refcnt;
    return new WorkingDir(this, node, uid);
}

ecode_t FileSystem::add_user(const char* username, uint32_t& uid) {
    uid = userfile_->add_user(username);
    if (uid == 0) {
        return ERROR_EXIST;
    }
    return 0;
}

ecode_t FileSystem::remove_user(uint32_t uid) {
    return userfile_->remove_user(uid);
}

ecode_t FileSystem::list_users(std::vector<std::string>& list) {
    userfile_->list_users(list);
    return 0;
}

void FileSystem::flush() {
    if (block_mgr_) {
        block_mgr_->flush();
    }
}

ecode_t FileSystem::format() {
    auto root_node = load_node_(ROOT_INODE);
    if (root_node->refcnt > 1) {
        std::cerr << "format: Root node is busy" << std::endl;
        return ERROR_BUSY;
    }
    root_node->refcnt = 0;
    std::vector<node_t*> nodes;
    ecode_t ret = walk_and_acquire_(root_node, nodes);
    if (ret != 0) {
        std::cerr << "format: Failed to acquire nodes" << std::endl;
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            (*it)->unlock();
            release_node_(*it);
        }
        root_node->refcnt = 1;
        return ret;
    }
    format_();
    return 0;
}

void FileSystem::format_() {
    if (block_mgr_) close_();
    block_mgr_ = new BlockManager(disk_, true);
    // Create root inode
    InodeFile *root = new InodeFile(block_mgr_);
    blockid_t root_inode = root->create(0, 033, TYPE_DIR);
    if (root_inode != ROOT_INODE) {
        std::cerr << "format_: Root inode mismatch: " << root_inode << std::endl;
        exit(1);
    }
    node_t* root_node = new node_t(root, ROOT_INODE);
    root_node->refcnt = 1;
    nodes_[ROOT_INODE] = root_node;
    // Create user file
    InodeFile *uf_base = new InodeFile(block_mgr_);
    blockid_t uf_inode = uf_base->create(0, 000, TYPE_FILE);
    userfile_ = new UserFile(uf_base);
    root_node->dir->add_entry("userfile", uf_inode);
    // Create home directory
    InodeFile *home = new InodeFile(block_mgr_);
    blockid_t home_inode = home->create(0, 013, TYPE_DIR);
    node_t* home_node = new node_t(home, ROOT_INODE);
    nodes_[home_inode] = home_node;
    root_node->dir->add_entry("home", home_inode);
}

void FileSystem::load_() {
    block_mgr_ = new BlockManager(disk_);
    // Load root inode
    auto root_node = load_node_(ROOT_INODE);
    // std::cerr << "Root inode: " << root_node->file->inode_id() << std::endl;
    // Load user file
    blockid_t uf_inode = root_node->dir->lookup("userfile");
    // std::cerr << "User file inode: " << uf_inode << std::endl;
    if (uf_inode == 0) {
        std::cerr << "User file not found" << std::endl;
        exit(1);
    }
    InodeFile *uf_base = new InodeFile(block_mgr_);
    if (!uf_base->open(uf_inode)) {
        std::cerr << "Failed to open user file" << std::endl;
        exit(1);
    }
    userfile_ = new UserFile(uf_base);
}

void FileSystem::close_() {
    for (auto iter = nodes_.begin(); iter != nodes_.end(); ++iter) {
        delete iter->second;
    }
    nodes_.clear();
    if (userfile_) {
        delete userfile_;
        userfile_ = nullptr;
    }
    if (block_mgr_) {
        delete block_mgr_;
        block_mgr_ = nullptr;
    }
}

FileSystem::node_t* FileSystem::load_node_(blockid_t inode) {
    auto iter = nodes_.find(inode);
    if (iter != nodes_.end()) {
        return iter->second;
    }
    InodeFile* file = new InodeFile(block_mgr_, inode);
    if (!file->is_open()) {
        delete file;
        return nullptr;
    }
    node_t* node = new node_t(file, 0);
    sem_wait(&lock_);
    nodes_[inode] = node;
    sem_post(&lock_);
    return node;
}

void FileSystem::release_node_(node_t* node) {
    if (node->refcnt == 0) {
        sem_wait(&lock_);
        nodes_.erase(node->file->inode_id());
        sem_post(&lock_);
        delete node;
    }
}

ecode_t FileSystem::change_working_dir_(blockid_t block, WorkingDir* wd) {
    if (block == 0) {
        return ERROR_NOT_FOUND;
    }
    auto node = wd->node_;
    auto new_node = load_node_(block);
    if (node == new_node) {
        return 0;
    }
    if (new_node == nullptr) {
        std::cerr << "change_working_dir_: Failed to load node: " << block << std::endl;
        return ERROR_INVALID;
    }
    if (!test_permission(new_node->file->inode(), wd->user_, false)) {
        std::cerr << "change_working_dir_: Permission denied: " << block << std::endl;
        release_node_(new_node);
        return ERROR_PERMISSION;
    }
    if (new_node->dir == nullptr) {
        std::cerr << "change_working_dir_: Not a directory: " << block << std::endl;
        release_node_(new_node);
        return ERROR_NOT_DIR;
    }
    // std::cout << "refcnt: " << node->refcnt << std::endl;
    --node->refcnt;
    node->unlock(); // acquired in WorkingDir::change_dir
    ++new_node->refcnt;
    release_node_(node);
    wd->node_ = new_node;
    return 0;
}

ecode_t FileSystem::walk_and_acquire_(node_t *node, std::vector<node_t*> &nodes) {
    nodes.push_back(node);
    if (!node->try_lock(true) || node->refcnt > 0) {
        std::cerr << "Failed to lock node: " << node->file->inode_id() << ", refcnt: " << node->refcnt 
            << " rwcnt: " << node->rwcnt << std::endl;
        return ERROR_BUSY;
    }
    if (node->dir != nullptr) {
        auto dir = node->dir;
        std::vector<std::string> list;
        dir->list(list);
        for (auto it = list.begin(); it != list.end(); ++it) {
            if (it->compare(".") == 0 || it->compare("..") == 0) {
                continue;
            }
            blockid_t inode = dir->lookup(it->c_str());
            auto child = load_node_(inode);
            if (child == nullptr) {
                std::cerr << "Failed to load node: " << inode << std::endl;
                return ERROR_INVALID;
            }
            ecode_t ret = walk_and_acquire_(child, nodes);
            if (ret != 0) {
                return ret;
            }
        }
    }
    return 0;
}

void FileSystem::close_working_dir(WorkingDir*& wd) {
    if (wd == nullptr) {
        return;
    }
    auto node = wd->node_;
    --node->refcnt;
    release_node_(node);
    delete wd;
    wd = nullptr;
}

ecode_t FileSystem::remove_(blockid_t inode, uint32_t user) {
    std::cerr << "remove: " << inode << std::endl;
    auto node = load_node_(inode);
    if (node == nullptr) {
        std::cerr << "Failed to load node: " << inode << std::endl;
        return ERROR_INVALID;
    }
    if (!test_permission(node->file->inode(), user, true)) {
        std::cerr << "Permission denied: " << inode << std::endl;
        release_node_(node);
        return ERROR_PERMISSION;
    }
    std::vector<node_t*> nodes;
    ecode_t ret = walk_and_acquire_(node, nodes);
    if (ret != 0) { // release all nodes
        std::cerr << "Failed to acquire nodes" << std::endl;
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            (*it)->unlock();
            release_node_(*it);
        }
        return ret;
    }
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        std::cout << "remove node: " << (*it)->file->inode_id() << std::endl;
        auto node = *it;
        auto inode = node->file->inode_id();
        node->file->removeall();
        node->file->close();
        nodes_.erase(inode);
        delete node;
        block_mgr_->free_block(inode);
    }
    return 0;
}