#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

enum evt_id {
  EVT_VFS_READ_ENTER  = 0,
  EVT_VFS_READ_EXIT   = 1,

  EVT_FS_READ_ENTER   = 10,
  EVT_FS_READ_EXIT    = 11,

  EVT_BLK_SUBMIT      = 20,
  EVT_BLK_COMPLETE    = 21,

  EVT_DRIVER_ENTER    = 30, // Driver 계층 진입 시점
  EVT_DRIVER_EXIT     = 31, // Device cmd queue에 요청 삽입 후 Driver 계층 반환 시점

  EVT_IRQ_START       = 40,
  EVT_IRQ_DONE        = 41,
};

struct evt_hdr {
  uint64_t ts_ns;

  uint32_t pid;
  uint32_t tgid;

  uint32_t evt_id;
  uint32_t reserved;

  uint64_t req_id;
};

/* ==== VFS ==== */

struct vfs_payload {
  uint64_t inode;
  uint32_t dev_major;
  uint32_t dev_minor;

  char filename[64];

  uint64_t file_offset;
  uint32_t len_req;
  uint32_t len_ret;
};

/* ==== FS ==== */

struct fs_payload {
  uint64_t inode;
  uint64_t file_offset;
  uint32_t bytes_issued_to_cache; // page cahce에서 준비(또는 readahead) 시도한 바이트
  uint32_t flags;                 // FS-specific flags, e.g. readahead? direct IO?
};

/* ==== BLK ==== */

struct blk_payload {
  uint64_t sector;    // starting sector
  uint32_t bytes;     // bio size in bytes
  uint32_t rwbs;      // read/write flags summary;
  uint32_t queue_id;  // blk/driver queue index
  uint32_t reserved;

  uint64_t io_inner_id;
};

/* ==== Driver ==== */

struct driver_payload {
  uint16_t hwq;
  uint16_t cq;
  uint16_t cmd_id;
  uint16_t status;
  uint32_t reserved;

  uint64_t io_inner_id;
};

/* ==== IRQ ==== */

struct irq_payload {
  uint32_t irq_vec;
  uint32_t reserved;

  uint64_t io_inner_id;
};

/* ==== EVENT ==== */

struct event {
  struct evt_hdr hdr;

  union {
    struct vfs_payload    vfs;
    struct fs_payload     fs;
    struct blk_payload    blk;
    struct driver_payload driver;
    struct irq_payload    irq;
  } payload;
};
