#!/usr/bin/env bash
set -euo pipefail

BS=$1
SIZE=$2
#CPU=$3
#JOB=$4
LOGDIR="fio_logs"
FIOSCRIPT="sync.fio"

# ---- seqread ----
sudo fio --bs=${BS} --size=${SIZE} --section=seqread \
    --cpus_allowed=2 --cpus_allowed_policy=shared ${FIOSCRIPT} \
    --output="${LOGDIR}/seqread_${SIZE}_${BS}_2.log"
sudo fio --bs=${BS} --size=${SIZE} --section=seqread \
    --cpus_allowed=16 --cpus_allowed_policy=shared ${FIOSCRIPT} \
    --output="${LOGDIR}/seqread_${SIZE}_${BS}_16.log"
sleep 2
# ---- randread ----
sudo fio --bs=${BS} --size=${SIZE} --section=randread \
    --cpus_allowed=2 --cpus_allowed_policy=shared ${FIOSCRIPT} \
    --output="${LOGDIR}/randread_${SIZE}_${BS}_2.log"
sudo fio --bs=${BS} --size=${SIZE} --section=randread \
    --cpus_allowed=16 --cpus_allowed_policy=shared ${FIOSCRIPT} \
    --output="${LOGDIR}/randread_${SIZE}_${BS}_16.log"
sleep 2
# ---- seqwrite ----
sudo fio --bs=${BS} --size=${SIZE} --section=seqwrite \
    --cpus_allowed=2 --cpus_allowed_policy=shared ${FIOSCRIPT} \
    --output="${LOGDIR}/seqwrite_${SIZE}_${BS}_2.log"
sudo fio --bs=${BS} --size=${SIZE} --section=seqwrite \
    --cpus_allowed=16 --cpus_allowed_policy=shared ${FIOSCRIPT} \
    --output="${LOGDIR}/seqwrite_${SIZE}_${BS}_16.log"
sleep 2
# ---- randwrite ----
sudo fio --bs=${BS} --size=${SIZE} --section=randwrite \
    --cpus_allowed=2 --cpus_allowed_policy=shared ${FIOSCRIPT} \
    --output="${LOGDIR}/randwrite_${SIZE}_${BS}_2.log"
sudo fio --bs=${BS} --size=${SIZE} --section=randwrite \
    --cpus_allowed=16 --cpus_allowed_policy=shared ${FIOSCRIPT} \
    --output="${LOGDIR}/randwrite_${SIZE}_${BS}_16.log"
sleep 2
# ---- randrw ----
sudo fio --bs=${BS} --size=${SIZE} --section=randrw \
    --cpus_allowed=2 --cpus_allowed_policy=shared ${FIOSCRIPT} \
    --output="${LOGDIR}/randrw_${SIZE}_${BS}_2.log"
sudo fio --bs=${BS} --size=${SIZE} --section=randrw \
    --cpus_allowed=16 --cpus_allowed_policy=shared ${FIOSCRIPT} \
    --output="${LOGDIR}/randrw_${SIZE}_${BS}_16.log"
sleep 2

#for i in {1..23}; do
#    sudo fio --bs=${BS} --size=${SIZE} --section=${JOB} \
#        --cpus_allowed=${i} --cpus_allowed_policy=shared ${FIOSCRIPT} \
#        --output="${LOGDIR}/${JOB}_${SIZE}_${i}.log"
#done
