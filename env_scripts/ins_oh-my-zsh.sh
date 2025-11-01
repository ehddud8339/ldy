#!/usr/bin/env bash
set -euo pipefail

# === Config (변경 가능) ========================================================
ZSH_PKG="zsh"                         # Ubuntu 20.04 repo: zsh 5.8
ZSH_DEFAULT_SHELL="/usr/bin/zsh"
OMZ_DIR="${HOME}/.oh-my-zsh"
ZSHRC="${HOME}/.zshrc"
OMZ_CUSTOM="${OMZ_DIR}/custom"
THEMES_DIR="${OMZ_CUSTOM}/themes"
PLUGINS_DIR="${OMZ_CUSTOM}/plugins"
P10K_DIR="${THEMES_DIR}/powerlevel10k"
P10K_VERSION_TAG="v1.18.0"           # 안정 태그(필요시 업데이트 가능)
# 환경변수 OMZ_COMMIT 이 지정되면 해당 커밋으로 체크아웃
OMZ_REPO="https://github.com/ohmyzsh/ohmyzsh.git"
P10K_REPO="https://github.com/romkatv/powerlevel10k.git"
ZSH_AUTOSUGG_REPO="https://github.com/zsh-users/zsh-autosuggestions.git"
ZSH_SYNTAX_REPO="https://github.com/zsh-users/zsh-syntax-highlighting.git"

# MesloLGS NF font files from powerlevel10k media repo
FONTS_BASE_URL="https://raw.githubusercontent.com/romkatv/powerlevel10k-media/master"
FONT_FILES=(
  "MesloLGS NF Regular.ttf"
  "MesloLGS NF Bold.ttf"
  "MesloLGS NF Italic.ttf"
  "MesloLGS NF Bold Italic.ttf"
)
FONTS_DIR="${HOME}/.local/share/fonts/MesloLGS_NF"
GNOME_FONT_NAME="MesloLGS NF Regular 12"

# === Helper ===================================================================
need_sudo() {
  if [ "$(id -u)" -ne 0 ]; then echo "sudo"; else echo ""; fi
}

msg() { printf "\n\033[1;36m[INFO]\033[0m %s\n" "$*"; }
warn(){ printf "\n\033[1;33m[WARN]\033[0m %s\n" "$*"; }
ok()  { printf "\033[1;32m[OK]\033[0m %s\n" "$*"; }

# === 0. Apt 준비 및 필수 패키지 =================================================
msg "패키지 인덱스 업데이트 및 필수 의존성 설치"
SUDO=$(need_sudo)
$SUDO apt-get update -y
$SUDO DEBIAN_FRONTEND=noninteractive apt-get install -y \
  ${ZSH_PKG} git curl wget unzip ca-certificates fontconfig locales dconf-cli

# 로케일(UTF-8) 보장: glyph/심볼 문제 완화
if ! locale -a | grep -q "en_US.utf8"; then
  msg "en_US.UTF-8 로케일 생성"
  $SUDO locale-gen en_US.UTF-8
fi

# === 1. zsh 설치 및 기본셸 전환 ==================================================
msg "zsh 버전 확인"
ZV="$(${ZSH_PKG} --version || true)"
echo "  -> ${ZV:-not found}"
if ! command -v zsh >/dev/null 2>&1; then
  $SUDO apt-get install -y ${ZSH_PKG}
fi

if [ "${SHELL}" != "${ZSH_DEFAULT_SHELL}" ]; then
  msg "기본 셸을 zsh로 변경 (chsh)"
  if ! grep -q "^${ZSH_DEFAULT_SHELL}$" /etc/shells; then
    $SUDO sh -c "echo '${ZSH_DEFAULT_SHELL}' >> /etc/shells"
  fi
  chsh -s "${ZSH_DEFAULT_SHELL}" || warn "chsh 실패: 수동으로 'chsh -s ${ZSH_DEFAULT_SHELL}' 실행 필요"
else
  ok "이미 zsh가 기본 셸입니다."
fi

# === 2. oh-my-zsh 설치 (자동 업데이트 비활성) ====================================
if [ ! -d "${OMZ_DIR}" ]; then
  msg "oh-my-zsh 설치"
  git clone --depth 1 "${OMZ_REPO}" "${OMZ_DIR}"
  # 기본 .zshrc 배포
  cp "${OMZ_DIR}/templates/zshrc.zsh-template" "${ZSHRC}"
else
  ok "oh-my-zsh가 이미 설치되어 있습니다."
fi

# 특정 커밋으로 고정하고 싶을 때: OMZ_COMMIT 환경변수 사용
if [ -n "${OMZ_COMMIT:-}" ]; then
  msg "oh-my-zsh를 커밋 ${OMZ_COMMIT} 로 체크아웃"
  (cd "${OMZ_DIR}" && git fetch --depth 1 origin "${OMZ_COMMIT}" && git checkout -f "${OMZ_COMMIT}")
fi

# 자동 업데이트 끄기 (안정성 우선)
if ! grep -q "^DISABLE_AUTO_UPDATE=" "${ZSHRC}"; then
  echo 'DISABLE_AUTO_UPDATE="true"' >> "${ZSHRC}"
else
  sed -i 's/^DISABLE_AUTO_UPDATE=.*/DISABLE_AUTO_UPDATE="true"/' "${ZSHRC}"
fi
# compfix 경고 회피
if ! grep -q "^ZSH_DISABLE_COMPFIX=" "${ZSHRC}"; then
  echo 'ZSH_DISABLE_COMPFIX="true"' >> "${ZSHRC}"
fi

# === 3. powerlevel10k 테마 설치 및 적용 =========================================
if [ ! -d "${P10K_DIR}" ]; then
  msg "powerlevel10k(${P10K_VERSION_TAG}) 설치"
  mkdir -p "${THEMES_DIR}"
  git clone --depth 1 --branch "${P10K_VERSION_TAG}" "${P10K_REPO}" "${P10K_DIR}"
else
  ok "powerlevel10k가 이미 설치되어 있습니다."
fi

# ZSH_THEME 설정
if grep -q '^ZSH_THEME=' "${ZSHRC}"; then
  sed -i 's#^ZSH_THEME=.*#ZSH_THEME="powerlevel10k/powerlevel10k"#' "${ZSHRC}"
else
  echo 'ZSH_THEME="powerlevel10k/powerlevel10k"' >> "${ZSHRC}"
fi

# === 4. 유용 플러그인 설치 (autosuggestions, syntax-highlighting) ===============
msg "플러그인 설치: zsh-autosuggestions, zsh-syntax-highlighting"
mkdir -p "${PLUGINS_DIR}"
[ -d "${PLUGINS_DIR}/zsh-autosuggestions" ] || \
  git clone --depth 1 "${ZSH_AUTOSUGG_REPO}" "${PLUGINS_DIR}/zsh-autosuggestions"
[ -d "${PLUGINS_DIR}/zsh-syntax-highlighting" ] || \
  git clone --depth 1 "${ZSH_SYNTAX_REPO}" "${PLUGINS_DIR}/zsh-syntax-highlighting"

# plugins 라인 구성
if grep -q '^plugins=' "${ZSHRC}"; then
  sed -i 's/^plugins=.*/plugins=(git zsh-autosuggestions zsh-syntax-highlighting)/' "${ZSHRC}"
else
  echo 'plugins=(git zsh-autosuggestions zsh-syntax-highlighting)' >> "${ZSHRC}"
fi

# === 5. MesloLGS Nerd Font 설치 (아이콘 깨짐 해결) ==============================
msg "MesloLGS Nerd Font 설치"
mkdir -p "${FONTS_DIR}"
for f in "${FONT_FILES[@]}"; do
  if [ ! -f "${FONTS_DIR}/${f}" ]; then
    curl -fsSL "${FONTS_BASE_URL}/${f// /%20}" -o "${FONTS_DIR}/${f}"
  fi
done
fc-cache -f "${HOME}/.local/share/fonts" || true
ok "폰트 설치 완료: ${FONTS_DIR}"

# === 6. GNOME Terminal 자동 글꼴 설정 (가능한 경우) =============================
set_gnome_terminal_font() {
  if command -v gsettings >/dev/null 2>&1 && \
     gsettings list-schemas | grep -q "org.gnome.Terminal.Legacy.Profile"; then
    # 기본 프로파일 UUID 조회
    local base="org.gnome.Terminal.Legacy"
    local profile_list
    profile_list=$(gsettings get ${base}.ProfilesList list | tr -d "[]',")
    # 기본/첫 번째 프로파일 선택
    local default_uuid
    default_uuid=$(gsettings get ${base}.ProfilesList default | tr -d "'")
    local target_uuid="${default_uuid:-$(echo "${profile_list}" | awk '{print $1}')}"
    if [ -n "${target_uuid}" ]; then
      msg "GNOME Terminal 프로파일(${target_uuid})에 글꼴 적용: ${GNOME_FONT_NAME}"
      gsettings set ${base}.Profile:/org/gnome/terminal/legacy/profiles:/:${target_uuid}/ use-system-font false || true
      gsettings set ${base}.Profile:/org/gnome/terminal/legacy/profiles:/:${target_uuid}/ font "${GNOME_FONT_NAME}" || true
      ok "GNOME Terminal 글꼴 설정 완료"
    fi
  else
    warn "GNOME Terminal을 감지하지 못했습니다. GUI 환경이 아니거나 다른 터미널을 사용 중일 수 있습니다."
    echo "  -> 터미널 설정에서 글꼴을 '${GNOME_FONT_NAME/ 12/}' 로 수동 지정하세요."
  fi
}
set_gnome_terminal_font || true

# === 7. 편의 옵션 몇 가지 =======================================================
# 유용한 HIST, 키바인딩 등(필요 시 주석 해제)
append_if_missing() {
  local line="$1"
  grep -qxF "${line}" "${ZSHRC}" || echo "${line}" >> "${ZSHRC}"
}

append_if_missing 'export LANG=en_US.UTF-8'
append_if_missing 'export LC_ALL=en_US.UTF-8'
append_if_missing 'setopt HIST_IGNORE_DUPS'
append_if_missing 'bindkey "^H" backward-kill-word'   # Ctrl+Backspace 유사
append_if_missing '[ -f ~/.p10k.zsh ] && source ~/.p10k.zsh'

# === 8. 마무리 안내 =============================================================
msg "설치/설정이 완료되었습니다."
echo "  • 기본 셸: zsh ($(zsh --version))"
echo "  • oh-my-zsh: ${OMZ_DIR}"
echo "  • 테마: powerlevel10k (${P10K_VERSION_TAG})"
echo "  • 플러그인: zsh-autosuggestions, zsh-syntax-highlighting"
echo "  • 폰트: MesloLGS NF (GNOME Terminal 자동설정 시도)"
echo
echo "※ 첫 zsh 실행 시 powerlevel10k 마법사가 뜨면, 권장 설정(Recommended)을 선택하면 됩니다."
echo "※ oh-my-zsh 자동 업데이트는 비활성화되어 있습니다. 수동 업데이트:"
echo "   (cd ${OMZ_DIR} && git pull --ff-only)"
echo
ok "새 터미널을 열거나 'exec zsh'로 적용하세요."

