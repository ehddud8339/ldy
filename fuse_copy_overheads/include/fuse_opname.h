/* fuse_opname.h */

#ifndef _FUSE_OPNAME_H
#define _FUSE_OPNAME_H

#pragma once

enum fuse_opcode_user {
    FUSE_LOOKUP  = 1,
    FUSE_FORGET  = 2,
    FUSE_GETATTR = 3,
    FUSE_SETATTR  = 4,
    FUSE_OPEN    = 14,
    FUSE_READ    = 15,
    FUSE_WRITE   = 16,
    FUSE_STATFS  = 17,
    FUSE_RELEASE = 18,
    FUSE_FSYNC   = 20,
    FUSE_FLUSH   = 25,
    FUSE_OPENDIR = 27,
    FUSE_READDIR = 28,
    FUSE_RELEASEDIR = 29,
};

static inline const char *fuse_opcode_name(uint32_t opcode)
{
    switch (opcode) {
    case FUSE_LOOKUP:     return "LOOKUP";
    case FUSE_GETATTR:    return "GETATTR";
    case FUSE_SETATTR:    return "SETATTR";
    case FUSE_OPEN:       return "OPEN";
    case FUSE_READ:       return "READ";
    case FUSE_WRITE:      return "WRITE";
    case FUSE_STATFS:     return "STATFS";
    case FUSE_RELEASE:    return "RELEASE";
    case FUSE_FSYNC:      return "FSYNC";
    case FUSE_FLUSH:      return "FLUSH";
    case FUSE_OPENDIR:    return "OPENDIR";
    case FUSE_READDIR:    return "READDIR";
    case FUSE_RELEASEDIR: return "RELEASEDIR";
    default:              return "UNKNOWN";
    }
}

#endif

