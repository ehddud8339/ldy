#!/usr/bin/env bash
# lazyvim_plus.sh (fixed & enhanced)
# Ubuntu 20.04.6 LTS + LazyVim post-setup
# - sudo 저장: lambdalisue/suda.vim 설치 + :W / w!!
# - 시스템 클립보드 공유: clipboard=unnamedplus (+ xclip/wl-clipboard)
# - Nerd Font: JetBrainsMono 설치 + GNOME Terminal 기본 폰트 적용(없으면 프로파일 자동 생성)
# - tmux truecolor 설정
# - devicons 로드 우선순위 고정

set -euo pipefail

NVIM_DIR="${HOME}/.config/nvim"
PLUG_DIR="${NVIM_DIR}/lua/plugins"
OPT_FILE="${NVIM_DIR}/lua/config/options.lua"

FONT_DIR="${HOME}/.local/share/fonts/NerdFonts/JetBrainsMono"
FONT_FAMILY="JetBrainsMono Nerd Font Mono"
FONT_SIZE="12"
FONT_NAME="${FONT_FAMILY} ${FONT_SIZE}"

need_cmd() { command -v "$1" >/dev/null 2>&1; }

if ! need_cmd nvim; then
  echo "[ERR] nvim을 찾을 수 없습니다. 먼저 Neovim을 설치하세요."
  exit 1
fi

echo "[1/6] 의존성 설치 (xclip, wl-clipboard, fontconfig, unzip, curl, uuid-runtime, libglib2.0-bin)"
sudo apt update
sudo apt install -y xclip wl-clipboard fontconfig unzip curl uuid-runtime libglib2.0-bin

echo "[2/6] JetBrainsMono Nerd Font 설치"
mkdir -p "$FONT_DIR"
TMPD="$(mktemp -d)"
trap 'rm -rf "$TMPD"' EXIT

# 최신 태그 -> 실패 시 폴백
NERD_TAG="$(curl -fsSL https://api.github.com/repos/ryanoasis/nerd-fonts/releases/latest |
  grep -oP '"tag_name":\s*"\K[^"]+' || true)"
[ -z "${NERD_TAG:-}" ] && NERD_TAG="v3.4.0"

FONT_URL="https://github.com/ryanoasis/nerd-fonts/releases/download/${NERD_TAG}/JetBrainsMono.zip"
echo " - 다운로드: ${FONT_URL}"
curl -fL "$FONT_URL" -o "$TMPD/JetBrainsMono.zip"
unzip -o "$TMPD/JetBrainsMono.zip" -d "$FONT_DIR" >/dev/null
fc-cache -fv >/dev/null || true

echo "[3/6] GNOME Terminal 폰트 적용 시도"
apply_gnome_terminal_font() {
  # gsettings와 터미널 스키마가 있는지 확인
  if ! need_cmd gsettings; then
    echo " - [INFO] gsettings가 없어 GNOME Terminal 자동 적용을 건너뜁니다."
    return 0
  fi
  if ! gsettings list-schemas | grep -q 'org.gnome.Terminal.Legacy.Profile'; then
    echo " - [INFO] GNOME Terminal 스키마를 찾지 못했습니다. 수동으로 폰트를 선택하세요."
    return 0
  fi

  # 기본 프로파일 UUID 파싱
  local default_uuid
  default_uuid="$(gsettings get org.gnome.Terminal.ProfilesList default 2>/dev/null | tr -d "'")"
  if [ -z "${default_uuid}" ]; then
    # 리스트에서 첫 항목 시도
    local raw_list first_uuid
    raw_list="$(gsettings get org.gnome.Terminal.ProfilesList list 2>/dev/null)" # 예: "['uuid1','uuid2']"
    first_uuid="$(printf "%s" "$raw_list" | tr -d "[]' " | cut -d, -f1)"
    default_uuid="$first_uuid"
  fi

  # 여전히 없으면 새 프로파일 생성
  if [ -z "${default_uuid}" ]; then
    local new_uuid
    new_uuid="$(uuidgen)"
    local list_str
    list_str="$(gsettings get org.gnome.Terminal.ProfilesList list 2>/dev/null || echo "[]")"
    # 리스트에 새 UUID 추가 (간단 파서)
    if printf "%s" "$list_str" | grep -q "$new_uuid"; then
      : # 이미 포함
    elif [ "$list_str" = "[]" ]; then
      list_str="['$new_uuid']"
    else
      # 끝의 ']' 앞에 추가
      list_str="$(printf "%s" "$list_str" | sed "s/]$/, '$new_uuid']/")"
    fi
    gsettings set org.gnome.Terminal.ProfilesList list "$list_str"
    gsettings set org.gnome.Terminal.ProfilesList default "'$new_uuid'"
    default_uuid="$new_uuid"
    # 가독성용 이름 부여(선택)
    gsettings set "org.gnome.Terminal.Legacy.Profile:/org/gnome/terminal/legacy/profiles:/:$default_uuid/" visible-name "'Default (Auto)'"
  fi

  # 최종 적용
  if [ -n "${default_uuid}" ]; then
    local base="/org/gnome/terminal/legacy/profiles:/:${default_uuid}/"
    echo " - 프로파일: ${default_uuid}"
    gsettings set "org.gnome.Terminal.Legacy.Profile:${base}" use-system-font false
    gsettings set "org.gnome.Terminal.Legacy.Profile:${base}" font "${FONT_NAME}"
    echo " - 적용 완료: ${FONT_NAME}"
  else
    echo " - [WARN] 기본 프로파일 UUID를 찾거나 만들지 못했습니다. GNOME Terminal을 한 번 실행한 뒤 다시 시도하세요."
  fi
}

apply_gnome_terminal_font

echo "[4/6] suda.vim 플러그인 스펙 추가"
mkdir -p "$PLUG_DIR"
cat >"${PLUG_DIR}/suda.lua" <<'EOF'
return {
  {
    "lambdalisue/suda.vim",
    init = function()
      vim.g.suda_smart_edit = 1
      -- :W -> :SudaWrite
      vim.api.nvim_create_user_command("W", "SudaWrite", {})
      -- w!! -> :SudaWrite
      vim.cmd([[
        cnoreabbrev <expr> w!! (getcmdtype() == ':' and getcmdline() == 'w!!') ? 'SudaWrite' : 'w!!'
      ]])
    end,
  },
}
EOF

echo "[5/6] clipboard=unnamedplus / guifont / devicons 로드 우선순위 구성"
mkdir -p "$(dirname "$OPT_FILE")"
if [ -f "$OPT_FILE" ]; then
  cp "$OPT_FILE" "${OPT_FILE}.bak.$(date +%Y%m%d-%H%M%S)"
fi

# 옵션 블록 주입(중복 방지)
if ! [ -f "$OPT_FILE" ] || ! grep -q "System clipboard + GUI font (added by lazyvim_plus.sh)" "$OPT_FILE"; then
  cat >>"$OPT_FILE" <<EOF

-- [auto] System clipboard + GUI font (added by lazyvim_plus.sh)
-- 시스템 클립보드 공유 (X11: xclip, Wayland: wl-clipboard 필요)
vim.opt.clipboard = "unnamedplus"

-- GUI 클라이언트(Neovide/nvim-qt 등)에서 Nerd Font 사용
pcall(function()
  vim.opt.guifont = "${FONT_FAMILY}:h${FONT_SIZE}"
end)
EOF
fi

# devicons 선로딩 스펙(없는 경우만)
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

echo "[6/6] tmux truecolor 설정"
TMUXCONF="$HOME/.tmux.conf"
if [ ! -f "$TMUXCONF" ] || ! grep -q 'terminal-overrides' "$TMUXCONF"; then
  {
    echo ""
    echo "# ---- Added by lazyvim_plus.sh for Neovim truecolor & glyphs ----"
    echo 'set -g default-terminal "tmux-256color"'
    echo 'set -as terminal-overrides ",xterm-256color:Tc"'
  } >>"$TMUXCONF"
  echo " - ${TMUXCONF} 갱신 (모든 tmux 세션을 완전히 종료 후 재시작하세요)"
fi

hash -r || true
echo
echo "✅ 완료!"
echo " - Nerd Font 설치: JetBrainsMono"
echo " - GNOME Terminal 폰트 적용(가능 시): ${FONT_NAME}"
echo " - Neovim: suda.vim / clipboard / devicons 적용"
echo " - tmux: truecolor 설정 완료 (세션 재시작 필요)"
echo
echo "다음 단계:"
echo " 1) 새 터미널 창을 열고 'nvim' 실행 → :Lazy, :Mason, :checkhealth 확인"
echo " 2) 아이콘이 안 보이면, 터미널 프로파일 폰트가 '${FONT_FAMILY}'인지 재확인"
