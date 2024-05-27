#pragma once
#ifndef ERRORCODE_H
#define ERRORCODE_H

using ecode_t = int;

constexpr ecode_t ERROR_SUCCESS = 0;
constexpr ecode_t ERROR_NOT_FOUND = -1;
constexpr ecode_t ERROR_INVALID = -2;
constexpr ecode_t ERROR_NO_SPACE = -3;
constexpr ecode_t ERROR_BAD_SIZE = -8;
constexpr ecode_t ERROR_PERMISSION = -9;
constexpr ecode_t ERROR_NOT_DIR = -11;
constexpr ecode_t ERROR_NOT_FILE = -12;
constexpr ecode_t ERROR_NOT_SYMLINK = -13;
constexpr ecode_t ERROR_BUSY = -14;
constexpr ecode_t ERROR_EXIST = -15;
constexpr ecode_t ERROR_USER_NOT_FOUND = -16;
constexpr ecode_t ERROR_INVALID_OP = -17;

enum Operation : int {
    OP_NOPE = 0,
    OP_FORMAT = 1,
    OP_CREATE = 2,
    OP_MKDIR = 3,
    OP_RMFILE = 4,
    OP_CD = 5,
    OP_RMDIR = 6,
    OP_LS = 7,
    OP_CAT = 8,
    OP_WRITE = 9,
    OP_INSERT = 10,
    OP_DELETE = 11,
    OP_SIZE = 12,
    OP_TRUNCATE = 13,
    OP_STAT = 14,
    OP_CHMOD = 15,
    OP_CHOWN = 16,
    OP_ADDUSER = 17,
    OP_DELUSER = 18,
    OP_LSUSER = 19,
    OP_READ = 20,
    OP_DELALL = 21,
    OP_EXIT = 22,
    OP_FLUSH = 23,
    OP_RENAME = 24,
};

#endif // !ERRORCODE_H