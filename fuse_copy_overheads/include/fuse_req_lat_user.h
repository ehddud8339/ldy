/* fuse_req_lat_user.h: user-space에서만 쓰는 헤더 예시 */

#ifndef _FUSE_REQ_LAT_USER_H
#define _FUSE_REQ_LAT_USER_H

#include <stdint.h>

struct fuse_req_event {
    uint64_t unique;          // fuse_in_header.unique
    uint32_t opcode;          // fuse_in_header.opcode
    uint32_t len;             // fuse_in_header.len
    int32_t  err;             // fuse_out_header.error
    uint32_t _pad;

    uint64_t enqueue_ts_ns;   // pending queue에 들어간 시각
    uint64_t dequeue_ts_ns;   // daemon이 가져간 시각
    uint64_t done_ts_ns;      // daemon이 완료해서 커널에 완료 알린 시각

    uint64_t queue_wait_ns;   // dequeue - enqueue
    uint64_t daemon_ns;       // done - dequeue

    uint64_t seq;             // 몇 번째 요청인지 (BPF에서 증가시키는 카운터)
};

#endif

