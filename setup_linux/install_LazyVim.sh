#!/usr/bin/env bash
# LazyVim on Ubuntu 20.04.6 LTS (reproducible pinned install)
# - Neovim: v0.11.2 (built from source with LuaJIT)   [LazyVim requires >= 0.11.2]
# - tree-sitter-cli: 0.22.6 (global via npm)
# - LazyVim Starter: cloned into ~/.config/nvim
# References: see "참고문헌" in the answer.

set -euo pipefail

# -----------------------------
# 0) 버전 변수 (필요 시 여기만 수정)
# -----------------------------
NEOVIM_TAG="v0.11.2"
TS_CLI_VERSION="0.22.6"

# -----------------------------
# 1) 기본 패키지 & 빌드 의존성
# -----------------------------
sudo apt update
sudo apt install -y \
  build-essential git curl ca-certificates \
  ninja-build gettext libtool libtool-bin autoconf automake cmake g++ pkg-config unzip \
  python3 python3-pip python3-venv \
  ripgrep fd-find fzf

# fd-find 바이너리 심볼릭 링크 (Ubuntu는 fd가 fdfind로 설치됨)
if ! command -v fd >/dev/null 2>&1 && command -v fdfind >/dev/null 2>&1; then
  sudo ln -sf "$(command -v fdfind)" /usr/local/bin/fd
fi

# -----------------------------
# 2) Neovim v0.11.2 빌드/설치
#    (LuaJIT 포함 빌드 · /usr/local)
# -----------------------------
TMPDIR="$(mktemp -d)"
pushd "$TMPDIR"

git clone --depth=1 --branch "${NEOVIM_TAG}" https://github.com/neovim/neovim.git
cd neovim
make CMAKE_BUILD_TYPE=Release
sudo make install
popd
rm -rf "$TMPDIR"

# sanity check
nvim --version | head -n 3

# --- [ADD] Node 18 LTS 설치 (NodeSource) -----------------
# (apt의 구버전 nodejs가 깔려 있다면 정리)
if command -v node >/dev/null 2>&1; then
  echo "[info] existing node: $(node -v)"
fi

# NodeSource 레포 추가 및 Node 18 설치
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt-get install -y nodejs

# sanity check
node -v
npm -v
# ---------------------------------------------------------

# --- [REPLACE] tree-sitter-cli 설치 블록 ------------------
# 구버전 환경에서 실패했던 tree-sitter-cli는 Node 18 설치 후 재시도
sudo npm install -g "tree-sitter-cli@${TS_CLI_VERSION}"
tree-sitter --version
# ---------------------------------------------------------

# -----------------------------
# 4) LazyVim Starter 배치
#    (기존 설정 백업 후 배치)
# -----------------------------
NVIM_CFG="$HOME/.config/nvim"
NVIM_BAK="$HOME/.config/nvim.backup.$(date +%Y%m%d-%H%M%S)"

if [ -d "$NVIM_CFG" ]; then
  echo "기존 Neovim 설정을 백업합니다 → $NVIM_BAK"
  mv "$NVIM_CFG" "$NVIM_BAK"
fi

git clone https://github.com/LazyVim/starter "$NVIM_CFG"
# Starter는 개인 설정용이므로 Git 이력 제거 권장
rm -rf "$NVIM_CFG/.git"

# -----------------------------
# 5) 최초 기동 전 권장 설정
# -----------------------------
# Nerd Font는 수동 설치 권장 (옵션). 여기서는 생략.
# 플러그인/파서 설치는 최초 nvim 실행 후 자동 진행되며,
# 필요시 :Lazy, :Mason, :TSInstall 명령 사용.

echo
echo "✅ 설치 완료!"
echo "- Neovim: $(nvim --version | head -n 1)"
echo "- tree-sitter-cli: $(tree-sitter --version)"
echo
echo "이제 'nvim'을 실행하면 LazyVim이 부트스트랩됩니다."
echo "플러그인 상태는 :Lazy, LSP는 :Mason, Treesitter는 :TSInstall 로 관리하세요."
