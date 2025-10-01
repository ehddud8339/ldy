#!/usr/bin/env bash
# zsh + oh-my-zsh + powerlevel10k 자동 설치/설정 스크립트
# - Ubuntu 20.04.6 LTS 가정
# - Nerd Font(예: JetBrainsMono Nerd Font)가 이미 설치되어 있으면 바로 아이콘/기호가 표시됩니다.
# - oh-my-zsh 설치는 비대화(RUNZSH/CHSH 제어)로 진행한 뒤, 마지막에 기본 셸을 zsh로 변경합니다.

set -euo pipefail

need_cmd() { command -v "$1" >/dev/null 2>&1; }

echo "[1/6] 패키지 설치: zsh, git, curl"
sudo apt update
sudo apt install -y zsh git curl

echo "[2/6] oh-my-zsh 비대화 설치 (기존 .zshrc 백업 유지)"
# 기존 .zshrc 안전 백업
if [ -f "$HOME/.zshrc" ]; then
  cp "$HOME/.zshrc" "$HOME/.zshrc.backup.$(date +%Y%m%d-%H%M%S)"
fi

export RUNZSH=no
export CHSH=no
export KEEP_ZSHRC=yes
# 설치 스크립트는 ~/.oh-my-zsh 생성 및 기본 템플릿 배치
sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)"

echo "[3/6] powerlevel10k 및 플러그인 설치"
ZSH_CUSTOM="${ZSH_CUSTOM:-$HOME/.oh-my-zsh/custom}"

# powerlevel10k
if [ ! -d "$ZSH_CUSTOM/themes/powerlevel10k" ]; then
  git clone --depth=1 https://github.com/romkatv/powerlevel10k.git \
    "$ZSH_CUSTOM/themes/powerlevel10k"
fi

# zsh-autosuggestions
if [ ! -d "$ZSH_CUSTOM/plugins/zsh-autosuggestions" ]; then
  git clone --depth=1 https://github.com/zsh-users/zsh-autosuggestions \
    "$ZSH_CUSTOM/plugins/zsh-autosuggestions"
fi

# zsh-syntax-highlighting
if [ ! -d "$ZSH_CUSTOM/plugins/zsh-syntax-highlighting" ]; then
  git clone --depth=1 https://github.com/zsh-users/zsh-syntax-highlighting \
    "$ZSH_CUSTOM/plugins/zsh-syntax-highlighting"
fi

echo "[4/6] ~/.zshrc 자동 구성"
# 기본 ~/.zshrc 생성되어 있다고 가정(oh-my-zsh 설치 시)
ZSHRC="$HOME/.zshrc"

# ZSH_THEME 설정을 powerlevel10k로 치환
if grep -q '^ZSH_THEME=' "$ZSHRC"; then
  sed -i 's/^ZSH_THEME=.*/ZSH_THEME="powerlevel10k\/powerlevel10k"/' "$ZSHRC"
else
  echo 'ZSH_THEME="powerlevel10k/powerlevel10k"' >>"$ZSHRC"
fi

# plugins 라인 교체(존재 시) 또는 추가
if grep -q '^plugins=' "$ZSHRC"; then
  sed -i 's/^plugins=.*/plugins=(git zsh-autosuggestions zsh-syntax-highlighting)/' "$ZSHRC"
else
  echo 'plugins=(git zsh-autosuggestions zsh-syntax-highlighting)' >>"$ZSHRC"
fi

# 플러그인 수동 source가 필요한 경우(oh-my-zsh 로딩 뒤에 오는 것이 안전)
# - 많은 테마/플러그인 환경에서 아래 두 줄이 없어도 동작하지만, 확실히 해 둠
if ! grep -q 'zsh-autosuggestions' "$ZSHRC"; then
  cat >>"$ZSHRC" <<'EOF'

# --- added by setup script ---
# ensure plugins are sourced (oh-my-zsh loads from $ZSH_CUSTOM/plugins)
# autosuggestions & syntax highlighting (order matters: autosuggestions after completion, highlighting at the end)
# (oh-my-zsh plugin loader covers these, but keeping explicit source is harmless if needed)
# source $ZSH/custom/plugins/zsh-autosuggestions/zsh-autosuggestions.zsh
# source $ZSH/custom/plugins/zsh-syntax-highlighting/zsh-syntax-highlighting.zsh
EOF
fi

# powerlevel10k 권장: non-interactive 시에도 깨지지 않도록 옵션 몇 가지(선택)
if ! grep -q 'POWERLEVEL9K_DISABLE_CONFIGURATION_WIZARD' "$ZSHRC"; then
  cat >>"$ZSHRC" <<'EOF'
# disable p10k wizard on first run (you can run `p10k configure` later)
typeset -g POWERLEVEL9K_DISABLE_CONFIGURATION_WIZARD=true
EOF
fi

echo "[5/6] 기본 로그인 셸을 zsh로 변경"
ZSH_PATH="$(command -v zsh)"
if [ -z "${ZSH_PATH}" ]; then
  echo "[ERR] zsh 경로를 찾을 수 없습니다." >&2
  exit 1
fi

# /etc/shells에 zsh 경로가 없으면 추가(일부 환경 대비)
if ! grep -q "$ZSH_PATH" /etc/shells; then
  echo "[INFO] /etc/shells에 $ZSH_PATH 추가 (sudo 필요)"
  echo "$ZSH_PATH" | sudo tee -a /etc/shells >/dev/null
fi

# 사용자 기본 셸 변경(비대화). 암호 입력을 요구할 수 있음.
if [ "$SHELL" != "$ZSH_PATH" ]; then
  chsh -s "$ZSH_PATH"
fi

echo "[6/6] 완료 안내"
echo "✅ 설치/설정 완료!"
echo " - oh-my-zsh: $HOME/.oh-my-zsh"
echo " - theme     : powerlevel10k"
echo " - plugins   : git, zsh-autosuggestions, zsh-syntax-highlighting"
echo " - default shell → zsh (새 로그인/새 터미널에서 적용)"
echo
echo "권장:"
echo " 1) 터미널 폰트를 Nerd Font(예: JetBrainsMono Nerd Font)로 설정"
echo " 2) 새 터미널을 열어 zsh이 기본으로 뜨는지 확인"
echo " 3) 필요하면 'p10k configure'로 프롬프트 상세 설정"
