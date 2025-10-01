#!/usr/bin/env bash
set -euo pipefail

LOGDIR="fio_logs"
mkdir -p ${LOGDIR}

echo "==== Start stress_rw_1.fio ===="
sudo fio stress_rw_1.fio --output="${LOGDIR}/stress_rw_1.log" &

echo "==== Start stress_rw_2.fio ===="
sudo fio stress_rw_2.fio --output="${LOGDIR}/stress_rw_2.log" &

wait

echo "==== All jobs finished. Logs are in ${LOGDIR}/ ===="
