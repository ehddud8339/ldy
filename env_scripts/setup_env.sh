#!/usr/bin/env bash
set -euo pipefail

# =====================================================================
# Ubuntu 20.04.6 LTS 개발자 기본 세팅
#  - 공통 필수 + 커널/eBPF 권장 묶음 설치
#  - DEBIAN_FRONTEND 비대화식 처리
#  - fd-find -> fd 심볼릭 링크
#  - zsh/tmux 등 기본 도구 포함 (설정은 별도 스크립트에서 진행)
# =====================================================================

msg() { printf "\n\033[1;36m[INFO]\033[0m %s\n" "$*"; }
ok() { printf "\033[1;32m[OK]\033[0m %s\n" "$*"; }
warn() { printf "\033[1;33m[WARN]\033[0m %s\n" "$*"; }
need_sudo() { if [ "$(id -u)" -ne 0 ]; then echo "sudo"; fi; }

SUDO=$(need_sudo)

# 0) apt 인덱스/기본 툴
msg "Apt 인덱스 갱신 및 필수 패키지 설치"
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get update -y

COMMON_PKGS=(
  apt-transport-https ca-certificates gnupg2 software-properties-common
  locales tzdata
  curl wget git tmux zsh htop lsof strace tree
  unzip zip xz-utils p7zip-full
  net-tools iproute2 dnsutils traceroute openssh-client
  build-essential cmake ninja-build pkg-config autoconf automake libtool libtool-bin gettext
  ripgrep fd-find fzf
  python3 python3-pip python3-venv
  fontconfig
)

KERNEL_SYS_PKGS=(
  "linux-headers-$(uname -r)"
  linux-tools-common linux-tools-generic "linux-tools-$(uname -r)"
  clang llvm libelf-dev zlib1g-dev libcap-dev dwarves libbpf-dev
  nvme-cli sysstat iotop
)

EXTRA_PKGS=(
  git-lfs # 필요없으면 주석
  # openssh-server  # SSH 서버가 필요하면 주석 해제
  # rar unrar       # RAR 필요 시
)

$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y \
  "${COMMON_PKGS[@]}" \
  "${KERNEL_SYS_PKGS[@]}" \
  "${EXTRA_PKGS[@]}"

# 1) 로케일(UTF-8) 확보
if ! locale -a | grep -qi 'en_US\.utf8'; then
  msg "en_US.UTF-8 로케일 생성"
  $SUDO locale-gen en_US.UTF-8 || true
fi

# 2) fd-find -> fd 링크 (Ubuntu는 fdfind로 설치됨)
if ! command -v fd >/dev/null 2>&1; then
  if command -v fdfind >/dev/null 2>&1; then
    $SUDO ln -sf "$(command -v fdfind)" /usr/local/bin/fd
    ok "fd 심볼릭 링크 생성: /usr/local/bin/fd -> $(command -v fdfind)"
  else
    warn "fdfind가 설치되지 않았습니다(비정상)."
  fi
fi

# 3) 성능 툴 sanity
msg "성능/커널 툴 버전 확인"
for c in gcc g++ clang llc bpftool pahole perf nvme; do
  command -v "$c" >/dev/null 2>&1 && printf "  - %-8s %s\n" "$c" "$("$c" --version 2>/dev/null | head -n1)" || true
done

# 4) 마무리 안내
ok "기본 개발 패키지 설치 완료"
echo "다음 단계:"
echo "  • zsh/oh-my-zsh/powerlevel10k 설정 스크립트 적용"
echo "  • tmux(oh-my-tmux) 설정 스크립트 적용"
echo "  • Neovim/LazyVim 설치 스크립트 적용 (GLIBC 이슈 시 소스 빌드 권장)"
