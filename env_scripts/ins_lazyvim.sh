#!/usr/bin/env bash
# lazyvim_full_setup.sh (merged)
# Ubuntu 20.04.6 LTS + LazyVim reproducible setup
# - Neovim v0.11.2 (source build, LuaJIT)
# - Node 20 LTS + tree-sitter-cli@0.22.6
# - LazyVim Starter + quality-of-life configs
# - JetBrainsMono Nerd Font install + GNOME Terminal font apply (if available)
# - tmux truecolor, devicons priority, suda.vim, clipboard sharing

set -euo pipefail

# -----------------------------
# Version pins (edit here)
# -----------------------------
NEOVIM_TAG="v0.11.2"
TS_CLI_VERSION="0.22.6"

# -----------------------------
# Paths and constants
# -----------------------------
NVIM_DIR="${HOME}/.config/nvim"
PLUG_DIR="${NVIM_DIR}/lua/plugins"
OPT_FILE="${NVIM_DIR}/lua/config/options.lua"

FONT_DIR="${HOME}/.local/share/fonts/NerdFonts/JetBrainsMono"
FONT_FAMILY="JetBrainsMono Nerd Font Mono"
FONT_SIZE="12"
FONT_NAME="${FONT_FAMILY} ${FONT_SIZE}"

need_cmd() { command -v "$1" >/dev/null 2>&1; }

msg() { printf "\n\033[1;36m[INFO]\033[0m %s\n" "$*"; }
ok() { printf "\033[1;32m[OK]\033[0m %s\n" "$*"; }
warn() { printf "\033[1;33m[WARN]\033[0m %s\n" "$*"; }

# -----------------------------
# 1) Base & build dependencies
# -----------------------------
msg "Install base build deps and tools"
sudo env DEBIAN_FRONTEND=noninteractive apt-get update -y
sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y \
  build-essential git curl ca-certificates \
  ninja-build gettext libtool libtool-bin autoconf automake cmake g++ pkg-config unzip \
  python3 python3-pip python3-venv \
  ripgrep fd-find fzf \
  xclip wl-clipboard fontconfig unzip uuid-runtime libglib2.0-bin

# fd-find -> fd link (Ubuntu names binary 'fdfind')
if ! command -v fd >/dev/null 2>&1 && command -v fdfind >/dev/null 2>&1; then
  sudo ln -sf "$(command -v fdfind)" /usr/local/bin/fd
fi

# -----------------------------
# 2) Build & install Neovim
# -----------------------------
msg "Build and install Neovim ${NEOVIM_TAG} (LuaJIT) to /usr/local"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
pushd "$TMPDIR" >/dev/null

git clone --depth=1 --branch "${NEOVIM_TAG}" https://github.com/neovim/neovim.git
cd neovim
make CMAKE_BUILD_TYPE=Release
sudo make install

popd >/dev/null
nvim --version | head -n3

# -----------------------------
# 3) Node 20 LTS + tree-sitter-cli
# -----------------------------
msg "Install Node 20 LTS (NodeSource) and tree-sitter-cli@${TS_CLI_VERSION}"
if command -v node >/dev/null 2>&1; then
  echo " - existing node: $(node -v)"
fi
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y nodejs
node -v
npm -v

# global install tree-sitter-cli (system-wide)
sudo npm install -g "tree-sitter-cli@${TS_CLI_VERSION}"
tree-sitter --version

# -----------------------------
# 4) LazyVim Starter
# -----------------------------
msg "Clone LazyVim Starter (backup old config if exists)"
NVIM_BAK="${NVIM_DIR}.backup.$(date +%Y%m%d-%H%M%S)"
if [ -d "$NVIM_DIR" ]; then
  mv "$NVIM_DIR" "$NVIM_BAK"
  echo " - previous config backed up to ${NVIM_BAK}"
fi
git clone https://github.com/LazyVim/starter "$NVIM_DIR"
rm -rf "$NVIM_DIR/.git"

# -----------------------------
# 5) QoL: suda.vim, clipboard, devicons priority
# -----------------------------
msg "Add suda.vim plugin and clipboard/devicons settings"
mkdir -p "$PLUG_DIR"

# suda.vim spec
cat >"${PLUG_DIR}/suda.lua" <<'EOF'
return {
  {
    "lambdalisue/suda.vim",
    init = function()
      vim.g.suda_smart_edit = 1
      vim.api.nvim_create_user_command("W", "SudaWrite", {})
      vim.cmd([[
        cnoreabbrev <expr> w!! (getcmdtype() == ':' and getcmdline() == 'w!!') ? 'SudaWrite' : 'w!!'
      ]])
    end,
  },
}
EOF

# options.lua: clipboard & guifont (append once)
mkdir -p "$(dirname "$OPT_FILE")"
if [ -f "$OPT_FILE" ]; then
  cp "$OPT_FILE" "${OPT_FILE}.bak.$(date +%Y%m%d-%H%M%S)"
fi
if ! [ -f "$OPT_FILE" ] || ! grep -q "System clipboard + GUI font (added by lazyvim_full_setup.sh)" "$OPT_FILE"; then
  cat >>"$OPT_FILE" <<EOF

-- System clipboard + GUI font (added by lazyvim_full_setup.sh)
vim.opt.clipboard = "unnamedplus"
pcall(function()
  vim.opt.guifont = "${FONT_FAMILY}:h${FONT_SIZE}"
end)
EOF
fi

# devicons load priority (only if not present)
DEVICONS_SPEC="${PLUG_DIR}/00-devicons.lua"
if [ ! -f "$DEVICONS_SPEC" ]; then
  cat >"$DEVICONS_SPEC" <<'EOF'
return {
  {
    "nvim-tree/nvim-web-devicons",
    lazy = false,
    priority = 1000,
    config = function()
      require("nvim-web-devicons").setup({})
    end,
  },
}
EOF
fi

# -----------------------------
# 6) Nerd Font: JetBrainsMono + GNOME Terminal apply (if available)
# -----------------------------
msg "Install JetBrainsMono Nerd Font and try to apply to GNOME Terminal"
mkdir -p "$FONT_DIR"
NF_TMP="$(mktemp -d)"
trap 'rm -rf "$NF_TMP"' RETURN

NERD_TAG="$(curl -fsSL https://api.github.com/repos/ryanoasis/nerd-fonts/releases/latest |
  grep -oP '"tag_name":\s*"\K[^"]+' || true)"
[ -z "${NERD_TAG:-}" ] && NERD_TAG="v3.4.0"
FONT_URL="https://github.com/ryanoasis/nerd-fonts/releases/download/${NERD_TAG}/JetBrainsMono.zip"
echo " - download: ${FONT_URL}"
curl -fL "$FONT_URL" -o "$NF_TMP/JetBrainsMono.zip"
unzip -o "$NF_TMP/JetBrainsMono.zip" -d "$FONT_DIR" >/dev/null
fc-cache -fv >/dev/null || true

apply_gnome_terminal_font() {
  if ! need_cmd gsettings; then
    echo " - [INFO] gsettings not found; skip GNOME Terminal apply."
    return 0
  fi
  if ! gsettings list-schemas | grep -q 'org.gnome.Terminal.Legacy.Profile'; then
    echo " - [INFO] GNOME Terminal schema not found; skip apply."
    return 0
  fi
  local default_uuid
  default_uuid="$(gsettings get org.gnome.Terminal.ProfilesList default 2>/dev/null | tr -d "'")"
  if [ -z "${default_uuid}" ]; then
    local raw_list first_uuid
    raw_list="$(gsettings get org.gnome.Terminal.ProfilesList list 2>/dev/null || echo "[]")"
    first_uuid="$(printf "%s" "$raw_list" | tr -d "[]' " | cut -d, -f1)"
    default_uuid="$first_uuid"
  fi
  if [ -z "${default_uuid}" ]; then
    local new_uuid list_str
    new_uuid="$(uuidgen)"
    list_str="$(gsettings get org.gnome.Terminal.ProfilesList list 2>/dev/null || echo "[]")"
    if printf "%s" "$list_str" | grep -q "$new_uuid"; then :; elif [ "$list_str" = "[]" ]; then
      list_str="['$new_uuid']"
    else
      list_str="$(printf "%s" "$list_str" | sed "s/]$/, '$new_uuid']/")"
    fi
    gsettings set org.gnome.Terminal.ProfilesList list "$list_str"
    gsettings set org.gnome.Terminal.ProfilesList default "'$new_uuid'"
    default_uuid="$new_uuid"
    gsettings set "org.gnome.Terminal.Legacy.Profile:/org/gnome/terminal/legacy/profiles:/:$default_uuid/" visible-name "'Default (Auto)'"
  fi
  if [ -n "${default_uuid}" ]; then
    local base="/org/gnome/terminal/legacy/profiles:/:${default_uuid}/"
    echo " - profile: ${default_uuid}"
    gsettings set "org.gnome.Terminal.Legacy.Profile:${base}" use-system-font false
    gsettings set "org.gnome.Terminal.Legacy.Profile:${base}" font "${FONT_NAME}"
    echo " - applied: ${FONT_NAME}"
  else
    echo " - [WARN] Could not determine default profile. Open GNOME Terminal once and re-run."
  fi
}
apply_gnome_terminal_font

# -----------------------------
# 7) tmux truecolor
# -----------------------------
msg "Configure tmux for truecolor"
TMUXCONF="$HOME/.tmux.conf"
if [ ! -f "$TMUXCONF" ] || ! grep -q 'terminal-overrides' "$TMUXCONF"; then
  {
    echo ""
    echo "# ---- Added by lazyvim_full_setup.sh ----"
    echo 'set -g default-terminal "tmux-256color"'
    echo 'set -as terminal-overrides ",xterm-256color:Tc"'
  } >>"$TMUXCONF"
  echo " - ${TMUXCONF} updated (restart all tmux sessions)"
fi

hash -r || true

# -----------------------------
# 8) Final notes
# -----------------------------
ok "All done!"
echo " - Neovim: $(nvim --version | head -n1)"
echo " - Node: $(node -v), npm: $(npm -v)"
echo " - tree-sitter-cli: $(command -v tree-sitter || echo 'not found')"
echo " - Nerd Font installed: JetBrainsMono -> ${FONT_NAME}"
echo
echo "Next:"
echo "  1) Open a new terminal, run 'nvim' → LazyVim bootstrap"
echo "  2) Check ':Lazy', ':Mason', ':checkhealth nvim-treesitter'"
echo "  3) If icons look wrong, confirm terminal font is '${FONT_FAMILY}'"
