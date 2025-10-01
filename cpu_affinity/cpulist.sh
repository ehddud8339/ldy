#!/usr/bin/env bash

DEV=nvme1n1

cat /sys/block/${DEV}/queue/scheduler
cat /sys/block/${DEV}/queue/rq_affinity

for d in /sys/block/${DEV}/mq/*; do
    hctx=$(basename ${d})
    echo -n "hctx ${hctx} -> CPUs: "
    cat "${d}/cpu_list"
done
