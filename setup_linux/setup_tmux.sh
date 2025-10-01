#!/usr/bin/env bash
# setup_oh_my_tmux.sh
# Ubuntu 20.04.6 LTS 기준 oh-my-tmux 설치/설정 자동화

set -euo pipefail

need_cmd() { command -v "$1" >/dev/null 2>&1; }

echo "[1/6] 패키지 설치 (tmux, xclip, wl-clipboard, git, curl, ncurses-term)"
sudo apt update
sudo apt install -y tmux git curl xclip wl-clipboard ncurses-term

echo "[2/6] 기존 tmux 설정 백업"
TSUFFIX="$(date +%Y%m%d-%H%M%S)"
[ -f "$HOME/.tmux.conf" ] && mv "$HOME/.tmux.conf" "$HOME/.tmux.conf.backup.$TSUFFIX"
[ -f "$HOME/.tmux.conf.local" ] && mv "$HOME/.tmux.conf.local" "$HOME/.tmux.conf.local.backup.$TSUFFIX"

echo "[3/6] oh-my-tmux 클론 및 심볼릭 링크"
if [ ! -d "$HOME/.tmux" ]; then
  git clone --depth=1 https://github.com/gpakosz/.tmux.git "$HOME/.tmux"
else
  (cd "$HOME/.tmux" && git pull --ff-only)
fi
ln -sf "$HOME/.tmux/.tmux.conf" "$HOME/.tmux.conf"

echo "[4/6] ~/.tmux.conf.local 생성(커스텀 설정)"
cat >"$HOME/.tmux.conf.local" <<'EOF'
# ----- oh-my-tmux local overrides -----
# Truecolor & 색상
set -g default-terminal "tmux-256color"
set -as terminal-overrides ",xterm-256color:Tc"

# 마우스/스크롤
set -g mouse on

# vi키 바인딩
setw -g mode-keys vi
set -g status-keys vi

# 창/패널 인덱스 1부터
set -g base-index 1
setw -g pane-base-index 1

# 프리픽스 표시 강화
set -g @plugin 'tmux-plugins/tmux-prefix-highlight'
set -g status-right '#{prefix_highlight} %Y-%m-%d %H:%M '

# 복사 모드에서 시스템 클립보드와 연동 (X11/Wayland 환경)
# 필요에 따라 pbcopy/clip.exe 등으로 바꾸세요
bind -T copy-mode-vi y send -X copy-pipe-and-cancel "tmux save-buffer - | (wl-copy || xclip -i -selection clipboard)"

# 창/패널 이동 단축키(선호에 맞게 조정)
bind -n M-Left  select-pane -L
bind -n M-Right select-pane -R
bind -n M-Up    select-pane -U
bind -n M-Down  select-pane -D

# 상태줄 약간 굵게
set -g status-style bg=default,fg=colour245
EOF

echo "[5/6] terminfo 점검: tmux-256color 없으면 생성"
if ! infocmp tmux-256color >/dev/null 2>&1; then
  echo " - tmux-256color terminfo 생성"
  infocmp xterm-256color | sed \
    -e 's/xterm-256color/tmux-256color/' \
    -e 's/kf1=[^,]*/kf1=\EOP/' |
    tic -x - >/dev/null || true
fi

echo "[6/6] 마무리 안내"
echo "✅ oh-my-tmux 설치/설정 완료!"
echo " - 기본 설정 파일:  ~/.tmux.conf -> ~/.tmux/.tmux.conf (symlink)"
echo " - 로컬 오버라이드: ~/.tmux.conf.local"
echo
echo "적용 방법:"
echo " 1) 기존 tmux 세션이 있다면 모두 종료"
echo " 2) 새로운 터미널에서 'tmux' 실행"
echo " 3) 색상이 흐리면 터미널의 $TERM 이 xterm-256color 인지 확인"
echo
echo "팁:"
echo " - LazyVim/아이콘 사용 중이면 터미널 폰트를 Nerd Font로 설정"
echo " - 클립보드가 안 되면 X11(w/ xclip) 또는 Wayland(wl-clipboard) 여부를 확인"
