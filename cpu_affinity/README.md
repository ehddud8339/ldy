## 실험 설정
---
**P-core vs E-core IO 성능 비교**를 위해 환경을 설정해야 한다.

### 1. 변동성 최소화
- 장치/큐 설정
```
DEV=/dev/nvme1n1
echo 2 | sudo tee /sys/block/$(basename $DEV)/queue/rq_affinity
sudo sh -c 'for f in /sys/block/nvme*/queue/io_poll*; do echo 0 > $f; done'
```
- CPU 상태 고정
``` bash
sudo cpupower frequency-set -g performance
```
- P/E 코어 지정
``` bash
PSET="0,2,4,6,8,10,12,14"
ESET="16-23"
```
### 2. IRQ 경로 고정
NVMe 큐 완료가 다른 코어로 튀면 비교가 흐려질 수 있음.
``` bash
grep -iE 'nvme|pcie' /proc/interrupts
# P-core 실험 전
IRQS="$(grep -nE 'nvme|pcie' /proc/interrupts | awk -F: '{print $1}')"
for i in $IRQS; do echo $PSET | sudo tee /proc/irq/$i/smp_affinity_list; done
# E-core 실험 전
for i in $IRQS; do echo $ESET | sudo tee /proc/irq/$i/smp_affinity_list; done
```