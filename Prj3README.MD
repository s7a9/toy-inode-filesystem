# Project3: Inode File System

## bytepack

- `bytepack.h/.c` Provides a C style serialize and deserialize utils for network transmission.

## network

- `network.h/.c` Provides unified tcp/ip utilities for step1 and step2.

## step1

- `disksim.h/.c` Simulates the behavious of a physical disk.
- `server.c` Serve the disk operation requests.

## step2

- `blockmgr.h/.cc` Manages the allocation of blocks on disk.
- `inodefile.h/.cc` Manages a single inode file.
- `directory.h/.cc` Reads an inode file as directory and operates on it.
- `userfile.h/.cc` Provides an interface for a special file in file system to hold records for users.
- `idisk.h/.cc` is the network interface for remote disk.
- `fstest.cc` has the tests for step2.
- `filesystem.h/.cc` is the file system.
- `server.cc` receives requests and dispatches them to file system.
- `client.cc` parse commands and communicate with server.