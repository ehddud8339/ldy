## 실험 설정
---
### cgroup을 통한 CPU list 제한
**cgroup v2**의 `cpuset`을 이용하여 특정 프로세스가 사용할 수 있는 CPU 노드를 지정
```
``` 
`mkdir -p /sys/fs/cgroup/<user_group>`
`echo 0-n > /sys/fs/cgroup/<user_group>/cpuset.cpus`
`echo <pid> > /sys/fs/cgroup/<user_group>/cgroup.procs`

### CPU 전원/주파수 고정 방법
**CPUFreq governor**를 이용하여CPU의 주파수를 고정

현재 governor 확인
`cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`

모든 코어를 performance 모드로 고정
`for c in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
  echo performance | sudo tee $c
done`
```
```
