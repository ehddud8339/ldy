#!/usr/bin/env bash
export DEBIAN_FRONTEND=noninteractive
set -euo pipefail

# ==============================================================================
# oh-my-tmux setup for Ubuntu 20.04.6 LTS (Updated: no heredoc in tmux conf)
# - Installs tmux + deps
# - Clones/updates gpakosz/.tmux
# - Symlinks ~/.tmux.conf and writes sane ~/.tmux.conf.local
# - Ensures tmux-256color terminfo exists (fallback builds it if missing)
# - Installs a clipboard helper script OUTSIDE tmux.conf (no heredoc)
# - Prefers zsh as default-shell when available
# ==============================================================================

msg()  { printf "\n\033[1;36m[INFO]\033[0m %s\n" "$*"; }
ok()   { printf "\033[1;32m[OK]\033[0m %s\n" "$*"; }
warn() { printf "\n\033[1;33m[WARN]\033[0m %s\n" "$*"; }
need_sudo() { if [ "$(id -u)" -ne 0 ]; then echo "sudo"; fi; }

SUDO=$(need_sudo)
HOME_BIN="${HOME}/.local/bin"
mkdir -p "${HOME_BIN}"

# ----- 0) deps ----------------------------------------------------------------
msg "Installing dependencies (tmux, git, clipboard & terminfo tools)"
$SUDO apt-get update -y
$SUDO apt-get install -y \
  tmux git curl ca-certificates xclip wl-clipboard \
  ncurses-bin ncurses-term

# locale (optional; helps UTF-8 glyphs)
if ! locale -a | grep -qi 'en_US\.utf8'; then
  $SUDO locale-gen en_US.UTF-8 || true
fi

# ----- 1) ensure terminfo: tmux-256color -------------------------------------
msg "Checking terminfo entry: tmux-256color"
if ! infocmp tmux-256color >/dev/null 2>&1; then
  warn "tmux-256color not found. Installing a minimal entry locally (~/.terminfo)"
  TI_DIR="${HOME}/.terminfo"
  mkdir -p "${TI_DIR}"
  cat > /tmp/tmux-256color.src <<'EOF'
tmux-256color|tmux with 256 colors,
  use=xterm-256color,
  Tc,
EOF
  tic -x -o "${TI_DIR}" /tmp/tmux-256color.src || warn "tic failed; will fall back to screen-256color"
  rm -f /tmp/tmux-256color.src
else
  ok "terminfo tmux-256color available"
fi

# ----- 2) clone/update oh-my-tmux --------------------------------------------
OMT_DIR="${HOME}/.tmux"
if [ -d "${OMT_DIR}/.git" ]; then
  msg "Updating existing gpakosz/.tmux"
  git -C "${OMT_DIR}" pull --ff-only
else
  msg "Cloning gpakosz/.tmux -> ${OMT_DIR}"
  git clone --depth 1 https://github.com/gpakosz/.tmux.git "${OMT_DIR}"
fi

# Symlink ~/.tmux.conf
if [ -L "${HOME}/.tmux.conf" ] || [ -f "${HOME}/.tmux.conf" ]; then
  if [ "$(readlink -f "${HOME}/.tmux.conf" 2>/dev/null || true)" != "${OMT_DIR}/.tmux.conf" ]; then
    msg "Updating ~/.tmux.conf symlink"
    rm -f "${HOME}/.tmux.conf"
    ln -s "${OMT_DIR}/.tmux.conf" "${HOME}/.tmux.conf"
  fi
else
  ln -s "${OMT_DIR}/.tmux.conf" "${HOME}/.tmux.conf"
fi
ok "~/.tmux.conf -> ${OMT_DIR}/.tmux.conf"

# ----- 3) install clipboard helper (outside tmux.conf) ------------------------
msg "Installing clipboard helper: ${HOME_BIN}/tmux-copy"
cat > "${HOME_BIN}/tmux-copy" <<'EOS'
#!/usr/bin/env bash
set -euo pipefail
if command -v wl-copy >/dev/null 2>&1; then
  wl-copy
elif command -v xclip >/dev/null 2>&1; then
  xclip -selection clipboard -in
else
  # No system clipboard tool; consume stdin to keep tmux buffer only
  cat >/dev/null
fi
EOS
chmod +x "${HOME_BIN}/tmux-copy"
ok "Helper ready"

# ----- 4) write ~/.tmux.conf.local (no heredoc; idempotent) -------------------
LOCAL="${HOME}/.tmux.conf.local"
msg "Writing ~/.tmux.conf.local"

# Remove any previous problematic heredoc block (if present)
if [ -f "${LOCAL}" ]; then
  sed -i '/^run-shell .*tmux-copy .*<<.*/,/^EOS'\''$/d' "${LOCAL}" || true
fi

# Build fresh local config
{
  echo "# =========================================================================="
  echo "# oh-my-tmux local overrides (auto-generated, no heredoc) "
  echo "# =========================================================================="
  echo
  echo "# --- basics ---"
  echo "set -g mouse on"
  echo "set -g history-limit 200000"
  echo "set -g escape-time 10"
  echo "setw -g mode-keys vi"
  echo
  echo "# --- default shell (prefer zsh) ---"
  echo "set-option -g default-shell /usr/bin/zsh"
  echo 'set-option -g default-command "exec /usr/bin/zsh -l"'
  echo
  echo "# --- terminal & colors ---"
  if infocmp tmux-256color >/dev/null 2>&1; then
    echo 'set -g default-terminal "tmux-256color"'
  else
    echo 'set -g default-terminal "screen-256color"'
  fi
  echo "set -ga terminal-overrides ',xterm-256color:RGB,*:Tc'"
  echo
  echo "# --- status & misc ---"
  echo "set -g status-interval 5"
  echo "set -g clock-mode-style 24"
  echo "set -g allow-rename off"
  echo "set -g renumber-windows on"
  echo "set -g base-index 1"
  echo "setw -g pane-base-index 1"
  echo
  echo "# --- clipboard integration (vi-copy: y, C-c) ---"
  echo "bind -T copy-mode-vi y   send-keys -X copy-pipe-and-cancel '${HOME_BIN}/tmux-copy'"
  echo "bind -T copy-mode-vi C-c send-keys -X copy-pipe-and-cancel '${HOME_BIN}/tmux-copy'"
  echo
} > "${LOCAL}.tmp"

# Backup if existed, then replace
if [ -f "${LOCAL}" ]; then
  cp -f "${LOCAL}" "${LOCAL}.bak.$(date +%Y%m%d%H%M%S)"
fi
mv -f "${LOCAL}.tmp" "${LOCAL}"
ok "Wrote ${LOCAL}"

# ----- 5) reload if inside tmux ----------------------------------------------
if [ -n "${TMUX-}" ]; then
  msg "Reloading tmux config in current session"
  tmux source-file "${HOME}/.tmux.conf" || warn "tmux reload failed (will apply on next tmux)"
fi

# ----- 6) summary --------------------------------------------------------------
msg "Done."
tmux -V || true
echo "Config:"
echo "  • oh-my-tmux dir : ${OMT_DIR}"
echo "  • main conf      : ${HOME}/.tmux.conf (symlink)"
echo "  • local overrides: ${LOCAL}"
echo "  • clipboard wrap : ${HOME_BIN}/tmux-copy"
echo
echo "Tips:"
echo "  1) 새 터미널에서 'tmux' 실행"
echo "  2) Nerd Font(예: MesloLGS NF)로 터미널 글꼴을 지정해야 powerline 심볼이 정상 표시됩니다."
echo "  3) TrueColor 확인: tmux 안에서 'echo \$COLORTERM' 가 'truecolor'면 OK"

