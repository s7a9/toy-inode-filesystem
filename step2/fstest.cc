#include <iostream>
#include <string>
#include <vector>

#include "blockmgr.h"
#include "inodefile.h"
#include "filesystem.h"

struct HexCharStruct {
  unsigned char c;
  HexCharStruct(unsigned char _c) : c(_c) { }
};

inline std::ostream& operator<<(std::ostream& o, const HexCharStruct& hs) {
  return (o << std::hex << (int)hs.c);
}

inline HexCharStruct hex(unsigned char _c) {
  return HexCharStruct(_c);
}

class TestBase {
public:
    virtual int run() = 0;
    virtual const char* name() const = 0;
    virtual ~TestBase() {}
    std::ostream& out() {
        return std::cout << name() << ": ";
    }
};

class TestBlockManager : public TestBase {
public:
    struct TestBlock {
        uint32_t magic;
        uint32_t data;
    };

    int run() override {
        out() << "start testing..." << std::endl;
        disk = new RemoteDisk("127.0.0.1", 9348);
        if (!disk->open()) {
            out() << "Failed to open disk" << std::endl;
            return 1;
        }
        blockmgr = new BlockManager(disk, true);
        std::string line;
        do {
            std::cout << "BM>>> ";
            std::cin >> line;
            blockid_t block;
            if (line == "exit") break;
            else if (line == "alloc") {
                auto test_block = blockmgr->allocate<TestBlock>(block);
                test_block->magic = 0x2C1D7C0F;
                test_block->data = block * 2;
                blockmgr->dirtify(block);
                std::cout << "Allocated block " << block << std::endl;
            } else if (line == "load") {
                std::cin >> block;
                auto test_block = blockmgr->load<TestBlock>(block);
                if (test_block) {
                    std::cout << "Loaded block " << block << ": magic=" << test_block->magic
                        << ", data=" << test_block->data << std::endl;
                } else {
                    std::cout << "Failed to load block " << block << std::endl;
                }
            } else if (line == "dirtify") {
                std::cin >> block;
                blockmgr->dirtify(block);
                std::cout << "Dirtified block " << block << std::endl;
            } else if (line == "unref") {
                std::cin >> block;
                blockmgr->unref_block(block);
                std::cout << "Unreferenced block " << block << std::endl;
            } else if (line == "free") {
                std::cin >> block;
                blockmgr->free_block(block);
                std::cout << "Freed block " << block << std::endl;
            } else if (line == "read") {
                std::cin >> block;
                auto test_block = blockmgr->load<TestBlock>(block);
                if (test_block) {
                    std::cout << "Read block " << block << ": magic=" << test_block->magic
                        << ", data=" << test_block->data << std::endl;
                } else {
                    std::cout << "Failed to read block " << block << std::endl;
                }
            } else if (line == "flush") {
                blockmgr->flush();
                std::cout << "Flushed block manager" << std::endl;
            } else {
                std::cout << "Unknown command" << std::endl;
            }
        } while (true);
        delete blockmgr;
        delete disk;
        return 0;
    }

    const char* name() const override {
        static const char* name = "TestBlockManager";
        return name;
    }

private:
    RemoteDisk* disk = nullptr;
    BlockManager* blockmgr = nullptr;
};

class InodeFileTest : public TestBase {
public:
    int run() override {
        out() << "start testing..." << std::endl;
        disk = new RemoteDisk("127.0.0.1", 9348);
        if (!disk->open()) {
            out() << "Failed to open disk" << std::endl;
            return 1;
        }
        blockmgr = new BlockManager(disk, false);
        std::string line;
        InodeFile inodefile(blockmgr);
        blockid_t inode_block;
        do {
            std::cout << "INODE>>> ";
            std::cin >> line;
            if (line == "exit") break;
            else if (line == "create") {
                inode_block = inodefile.create(0, 0, 0);
                std::cout << "Created inode block " << inode_block << std::endl;
            } else if (line == "write") {
                std::string data;
                size_t pos;
                std::cin >> pos >> data;
                inodefile.write(data.c_str(), data.size(), pos);
            } else if (line == "read") {
                size_t pos, size;
                std::cin >> pos >> size;
                char buf[size];
                if (inodefile.read(buf, size, pos) == size)
                    std::cout << "Read: " << std::string(buf, size) << std::endl;
                else std::cerr << "Failed to read" << std::endl;
            } else if (line == "truncate") {
                size_t size;
                std::cin >> size;
                inodefile.truncate(size);
            } else if (line == "insert") {
                std::string data;
                size_t pos;
                std::cin >> pos >> data;
                inodefile.insert(data.c_str(), data.size(), pos);
            } else if (line == "remove") {
                size_t pos, size;
                std::cin >> pos >> size;
                inodefile.remove(size, pos);
            } else if (line == "removeall") {
                inodefile.removeall();
            } else if (line == "readall") {
                char buf[inodefile.size()+1];
                inodefile.readall(buf);
                buf[inodefile.size()] = '\0';
                std::cout << "Read all: " << std::string(buf, inodefile.size()) << std::endl;
            } else if (line == "close") {
                inodefile.close();
            } else if (line == "open") {
                std::cin >> inode_block;
                inodefile.open(inode_block);
            } else if (line == "dump") {
                std::cout << inodefile.dump();
            } else if (line == "is_open") {
                std::cout << "Is open: " << inodefile.is_open() << std::endl;
            } else {
                std::cout << "Unknown command" << std::endl;
            }
        } while (true);
        inodefile.close();
        delete blockmgr;
        delete disk;
        return 0;
    }

    const char* name() const override {
        static const char* name = "InodeFileTest";
        return name;
    }

private:
    RemoteDisk* disk = nullptr;
    BlockManager* blockmgr = nullptr;
};

class FileSystemTest : public TestBase {
public:
    int run() override {
        out() << "start testing..." << std::endl;
        disk = new RemoteDisk("127.0.0.1", 9348);
        if (!disk->open()) {
            out() << "Failed to open disk" << std::endl;
            return 1;
        }
        std::string line;
        out() << "load last fs? [yes] ";
        std::cin >> line;
        fs = new FileSystem(disk, line != "yes");
        out() << "FS loaded, login as: ";
        std::cin >> line;
        auto wd = fs->open_working_dir(line.c_str());
        if (!wd) {
            out() << "Failed to open working dir" << std::endl;
            delete fs;
            delete disk;
            return 1;
        }
        do {
            std::cout << "FS>>> ";
            std::cin >> line;
            if (line == "exit") break;
            else if (line == "create") {
                std::string filename;
                std::cin >> filename;
                wd->create_file(filename.c_str());
            } else if (line == "write") {
                std::string filename, data;
                size_t offset;
                std::cin >> filename >> offset >> data;
                wd->acquire_file(filename.c_str(), true);
                wd->active_file().write(data.c_str(), data.size(), offset);
                wd->release_file();
            } else if (line == "insert") {
                std::string filename, data;
                size_t offset;
                std::cin >> filename >> offset >> data;
                wd->acquire_file(filename.c_str(), true);
                wd->active_file().insert(data.c_str(), data.size(), offset);
                wd->release_file();
            } else if (line == "delete") {
                std::string filename;
                size_t offset, size;
                std::cin >> filename >> offset >> size;
                wd->acquire_file(filename.c_str(), true);
                wd->active_file().remove(size, offset);
                wd->release_file();
            } else if (line == "truncate") {
                std::string filename;
                size_t size;
                std::cin >> filename >> size;
                wd->acquire_file(filename.c_str(), true);
                wd->active_file().truncate(size);
                wd->release_file();
            } else if (line == "catch") {
                std::string filename;
                std::cin >> filename;
                wd->acquire_file(filename.c_str(), false);
                std::vector<char> data(wd->active_file().size());
                wd->active_file().readall(data.data());
                wd->release_file();
                std::cout << "Catch: " << std::string(data.begin(), data.end()) << std::endl;
            } else if (line == "mkdir") {
                std::string dirname;
                std::cin >> dirname;
                wd->create_dir(dirname.c_str());
            } else if (line == "list") {
                std::vector<std::string> list;
                wd->list_dir(list);
                for (auto& name : list) {
                    std::cout << name << std::endl;
                }
            } else if (line == "stat") {
                std::string name;
                std::cin >> name;
                wd->acquire_file(name.c_str(), false);
                std::cout << wd->active_file().dump();
                wd->release_file();
            } else if (line == "chmod") {
                std::string name;
                uint16_t mode;
                std::cin >> name >> mode;
                wd->chmod(name.c_str(), mode);
            } else if (line == "chown") {
                std::string name;
                uint32_t owner;
                std::cin >> name >> owner;
                wd->chown(name.c_str(), owner);
            } else if (line == "rm") {
                std::string name;
                std::cin >> name;
                wd->remove(name.c_str());
            } else if (line == "cd") {
                std::string path;
                std::cin >> path;
                wd->change_dir(path.c_str());
            } else if (line == "rmdir") {
                std::string name;
                std::cin >> name;
                wd->remove_dir(name.c_str());
            } else if (line == "adduser") {
                std::string name;
                std::cin >> name;
                uint32_t uid;
                fs->add_user(name.c_str(), uid);
                std::cout << "Added user " << name << " with uid " << uid << std::endl;
            } else if (line == "rmuser") {
                uint32_t uid;
                std::cin >> uid;
                fs->remove_user(uid);
                std::cout << "Removed user " << uid << std::endl;
            } else if (line == "listusers") {
                std::vector<std::string> list;
                fs->list_users(list);
                for (auto& name : list) {
                    std::cout << name << std::endl;
                }
            } else if (line == "flush") {
                fs->flush();
                std::cout << "Flushed filesystem" << std::endl;
            } else if (line == "format") {
                fs->close_working_dir(wd);
                fs->format();
                std::cout << "Formatted filesystem, login as root" << std::endl;
                wd = fs->open_working_dir("root");
            } else if (line == "rename") {
                std::string oldname, newname;
                std::cin >> oldname >> newname;
                wd->rename(oldname.c_str(), newname.c_str());
            } else {
                std::cout << "Unknown command" << std::endl;
            }
        } while (true);
        fs->close_working_dir(wd);
        delete fs;
        delete disk;
        return 0;
    }

    const char* name() const override {
        static const char* name = "FileSystemTest";
        return name;
    }

private:
    RemoteDisk* disk = nullptr;
    FileSystem* fs = nullptr;
};

int main() {
    std::vector<TestBase*> tests = {
        // new TestBlockManager(),
        new FileSystemTest(),
        // new InodeFileTest(),
    };
    for (auto test : tests) {
        if (test->run() == 0) test->out() << " passed" << std::endl;
        else test->out() << " failed" << std::endl;
        delete test;
    }
    return 0;
}