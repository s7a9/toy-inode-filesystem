# Test run

## Build

Simply use `make` under project root directory.

> make

## Set up the system

First, run the basic disk server.

> bin/BDS data/diskfile 128 128 8472

Then, run the file system.

> bin/FS 127.0.0.1 8472 9887

The file system would show disk information and ask the user if he/she wants to format the disk.

```bash
Connected to disk on 127.0.0.1:10383 with 128 cylinders and 128 sectors
Would you like to format the disk? (y/n): 
```

Choose no.

```bash
BlockManager: Block size: 256, Free list head: 89, Root inode: 0, Block end: 12884901958, Version: 1716916532
Server started on port 9887
```

## Login the file system

> bin/FC 127.0.0.1 9887

Use a user name to login the system, then we can do the operations.

```bash
Enter username: root
Login successful
FS >> cd home
success
FS >> ls
total: 3
.
..
f
FS >>
```

For other run tests, you can refer to the Experiment section in my report `Project3report.pdf`.