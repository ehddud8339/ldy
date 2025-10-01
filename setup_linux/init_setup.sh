#!/usr/bin/env bash
# Ubuntu 20.04.6 LTS 초기 세팅 스크립트
# 범위: 1~5, 8, 9 (기본 관리, 버전관리, 개발, 네트워킹, 압축, 보안, 유틸리티)

set -e

# 패키지 업데이트
sudo apt update && sudo apt upgrade -y

# 1. 시스템 기본 관리
sudo apt install -y \
  build-essential \
  linux-headers-$(uname -r) \
  manpages-dev \
  net-tools \
  curl wget \
  vim nano

# 2. 버전 관리 및 협업
sudo apt install -y \
  git \
  openssh-client openssh-server \
  gnupg ca-certificates

# 3. 개발용 패키지
sudo apt install -y \
  gdb valgrind cmake pkg-config \
  python3 python3-pip python3-venv \
  default-jdk maven

# 4. 네트워킹/분석
sudo apt install -y \
  tcpdump iftop iproute2 \
  htop iotop dstat sysstat

# 5. 압축/파일 관리
sudo apt install -y \
  unzip zip tar xz-utils p7zip-full

# 8. 품질 및 보안
sudo apt install -y \
  ufw fail2ban

# 9. 기타 유틸리티
sudo apt install -y \
  tree jq ripgrep silversearcher-ag tmux

echo "✅ Ubuntu 20.04.6 LTS 초기 패키지 설치 완료!"


