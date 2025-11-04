#!/usr/bin/env bash
# Minimal Arch dwm workstation installer written in Bash.
# See `installer.sh --help` for usage.

set -euo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
REPO_ROOT="$SCRIPT_DIR"
HOME_DIR="${HOME}"

BACKGROUND_PATH=""
REBOOT_AFTER="false"
VERBOSE="false"
POWERSAVER_OFF="true"
ASSUME_DEFAULTS="false"
AUR_HELPER="yay"
INSTALL_ST="true"
INSTALL_ALACRITTY="false"
INSTALL_SLSTATUS="true"

# package lists
BASE_PACKAGES=(
  base-devel
  git
  curl
  vim
  zsh
  tmux
  feh
  dunst
  libnotify
  xorg-server
  xorg-xinit
  xorg-xset
  xorg-xrandr
  libx11
  libxinerama
  libxft
  webkit2gtk
  xcompmgr
  ttf-font-awesome
  pamixer
  dwm
  dmenu
  st
  slstatus
  surf
  tabbed
  virtualbox-guest-utils
  virtualbox-guest-modules-arch
)

SHELL_PLUGINS=(
  "zsh-syntax-highlighting https://github.com/zsh-users/zsh-syntax-highlighting.git"
  "zsh-autosuggestions https://github.com/zsh-users/zsh-autosuggestions.git"
  "zsh-aur-install https://github.com/redxtech/zsh-aur-install.git"
  "zsh-vi-mode https://github.com/jeffreytse/zsh-vi-mode.git"
  "cd-ls https://github.com/zshzoo/cd-ls.git"
  "alias-tips https://github.com/djui/alias-tips.git"
  "zsh-completions https://github.com/zsh-users/zsh-completions.git"
)

die() {
  echo "Error: $*" >&2
  exit 1
}

log() {
  echo "$*"
}

log_verbose() {
  if [[ "$VERBOSE" == "true" ]]; then
    echo "$*"
  fi
}

usage() {
  cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  -b, --background PATH   Set background image (used by feh).
  -r, --reboot            Reboot automatically after completion.
  -v, --verbose           Enable verbose logging.
  -p, --powersaver        Keep powersaver enabled (default disables).
  -y, --assume-yes        (Deprecated) retained for compatibility; installer is non-interactive by default.
      --aur-helper NAME   Choose AUR helper (yay or paru).
      --terminal MODE     Choose terminal: st, alacritty, or both.
      --slstatus          Force slstatus installation.
      --no-slstatus       Skip slstatus installation.
  -h, --help              Show this help and exit.
EOF
}

ensure_dir() {
  mkdir -p "$1"
}

backup_path() {
  local path="$1"
  if [[ ! -e "$path" && ! -L "$path" ]]; then
    return
  fi
  if [[ -L "$path" ]]; then
    rm -f "$path"
    log_verbose "[*] Removed existing symlink $path"
    return
  fi
  local backup="${path}.bak.$(date +%s)"
  mv "$path" "$backup"
  log_verbose "[*] Backed up $path to $backup"
}

create_symlink() {
  local target="$1"
  local link="$2"
  ensure_dir "$(dirname "$link")"
  backup_path "$link"
  ln -s "$target" "$link"
  log_verbose "[*] Linked $link -> $target"
}

append_line() {
  local file="$1"
  local line="$2"
  local needle="${3:-}"
  ensure_dir "$(dirname "$file")"
  if [[ -f "$file" ]]; then
    if [[ -n "$needle" ]]; then
      if grep -Fq "$needle" "$file"; then
        return
      fi
    else
      if grep -Fxq "$line" "$file"; then
        return
      fi
    fi
  fi
  echo "$line" >>"$file"
  log_verbose "[*] Appended '$line' to $file"
}

run_or_warn() {
  if ! "$@"; then
    log "[!] Command failed: $*"
    return 1
  fi
}

resolve_background() {
  local arg="$1"
  if [[ -n "$arg" ]]; then
    local expanded
    expanded="$(realpath -m "$arg")" || die "Unable to resolve background: $arg"
    [[ -f "$expanded" ]] || die "Background image not found: $expanded"
    BACKGROUND_PATH="$expanded"
    return
  fi
  local candidate="$HOME_DIR/.config/wallpaper/tn.jpg"
  if [[ -f "$candidate" ]]; then
    BACKGROUND_PATH="$candidate"
    return
  fi
  local repo_candidate="$REPO_ROOT/suckless/.config/wallpaper/tn.jpg"
  if [[ -f "$repo_candidate" ]]; then
    BACKGROUND_PATH="$repo_candidate"
  fi
}

ensure_helper() {
  local helper="$1"
  case "$helper" in
    yay|paru) ;;
    *) die "Unsupported AUR helper: $helper" ;;
  esac
  if ! command -v "$helper" >/dev/null 2>&1; then
    die "$helper not found. Install it manually before running this script."
  fi
}

build_helper_cmd() {
  local helper="$1"
  local -n ref="$2"
  ref=("$helper" "-S" "--needed" "--noconfirm")
  if [[ "$helper" == "yay" ]]; then
    ref+=("--answerdiff" "None" "--answerclean" "None")
  elif [[ "$helper" == "paru" ]]; then
    ref+=("--skipreview")
  fi
}

install_packages_with_helper() {
  local helper="$1"; shift
  local packages=("$@")
  if [[ "${#packages[@]}" -eq 0 ]]; then
    return
  fi
  log "[*] Installing packages via $helper"
  local cmd=()
  build_helper_cmd "$helper" cmd
  if ! "${cmd[@]}" "${packages[@]}"; then
    log "[!] Bulk installation failed, retrying individually"
    local optional=(virtualbox-guest-modules-arch)
    local -A fallback=([virtualbox-guest-modules-arch]="virtualbox-guest-dkms")
    local missing=()
    for pkg in "${packages[@]}"; do
      if "${cmd[@]}" "$pkg"; then
        continue
      fi
      missing+=("$pkg")
    done
    local fatal=()
    for pkg in "${missing[@]}"; do
      local handled="false"
      if [[ -n "${fallback[$pkg]:-}" ]]; then
        if "${cmd[@]}" "${fallback[$pkg]}"; then
          log "[*] Installed fallback ${fallback[$pkg]} for $pkg"
          handled="true"
        fi
      fi
      if [[ "$handled" == "false" && ! " ${optional[*]} " =~ " $pkg " ]]; then
        fatal+=("$pkg")
      elif [[ "$handled" == "false" ]]; then
        log "[!] Optional package $pkg could not be installed"
      fi
    done
    if [[ "${#fatal[@]}" -gt 0 ]]; then
      die "Failed to install required packages: ${fatal[*]}"
    fi
  fi
}

setup_zsh() {
  local ohmyzsh="$HOME_DIR/.oh-my-zsh"
  if [[ ! -d "$ohmyzsh" ]]; then
    log "[*] Installing oh-my-zsh"
    local installer_cmd='export RUNZSH=no KEEP_ZSHRC=yes; sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)"'
    run_or_warn bash -lc "$installer_cmd" || die "oh-my-zsh installation failed"
  fi
  local plugins_root="$ohmyzsh/custom/plugins"
  ensure_dir "$plugins_root"
  for entry in "${SHELL_PLUGINS[@]}"; do
    local name url
    name="$(cut -d' ' -f1 <<<"$entry")"
    url="$(cut -d' ' -f2 <<<"$entry")"
    local dest="$plugins_root/$name"
    if [[ -d "$dest/.git" ]]; then
      (cd "$dest" && git pull --ff-only) || true
    else
      (cd "$plugins_root" && git clone "$url" "$name") || die "Failed to clone $name"
    fi
  done
  local repo_zshrc="$REPO_ROOT/suckless/.zshrc"
  if [[ -f "$repo_zshrc" ]]; then
    create_symlink "$repo_zshrc" "$HOME_DIR/.zshrc"
  fi
  log "Reminder: set zsh as default shell with 'chsh -s /usr/bin/zsh' if needed."
}

setup_vim() {
  local repo_vimrc="$REPO_ROOT/suckless/.vimrc"
  if [[ -f "$repo_vimrc" ]]; then
    create_symlink "$repo_vimrc" "$HOME_DIR/.vimrc"
  fi
}

link_configs() {
  local repo_config="$REPO_ROOT/suckless/.config"
  [[ -d "$repo_config" ]] || return
  local target_root="$HOME_DIR/.config"
  ensure_dir "$target_root"
  local mandatory=(dunst dwm tmux wallpaper)
  local optional=()
  if [[ "$INSTALL_ALACRITTY" == "true" ]]; then
    optional+=(alacritty)
  fi
  for dir in "${mandatory[@]}" "${optional[@]}"; do
    local source="$repo_config/$dir"
    if [[ -d "$source" ]]; then
      create_symlink "$source" "$target_root/$dir"
    fi
  done
  if [[ -z "$BACKGROUND_PATH" ]]; then
    local fallback="$target_root/wallpaper/tn.jpg"
    if [[ -f "$fallback" ]]; then
      BACKGROUND_PATH="$fallback"
    fi
  fi
}

configure_startup() {
  local xinit="$HOME_DIR/.xinitrc"
  append_line "$xinit" "xcompmgr &" "xcompmgr"
  append_line "$xinit" "dunst &" "dunst"
  if [[ "$POWERSAVER_OFF" == "true" ]]; then
    append_line "$xinit" "xset s off -dpms &" "xset s off"
  fi
  if [[ -n "$BACKGROUND_PATH" ]]; then
    append_line "$xinit" "feh --bg-fill $BACKGROUND_PATH &" "feh --bg-fill"
  fi
  if [[ "$INSTALL_SLSTATUS" == "true" ]]; then
    append_line "$xinit" "slstatus &" "slstatus"
  fi
  append_line "$xinit" "exec dwm" "exec dwm"

  local bash_profile="$HOME_DIR/.bash_profile"
  append_line "$bash_profile" "startx" "startx"
}

countdown_and_reboot() {
  echo "SYSTEM WILL REBOOT IN"
  for ((i = 5; i > 0; i--)); do
    printf "%d %s\n" "$i" "$(printf '%*s' "$i" '' | tr ' ' '.')"
    sleep 1
  done
  echo "SYSTEM REBOOTING NOW"
  sudo reboot || true
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -b|--background)
        [[ $# -ge 2 ]] || die "Missing value for $1"
        BACKGROUND_PATH="$2"
        shift 2
        ;;
      -r|--reboot)
        REBOOT_AFTER="true"
        shift
        ;;
      -v|--verbose)
        VERBOSE="true"
        shift
        ;;
      -p|--powersaver)
        POWERSAVER_OFF="false"
        shift
        ;;
      -y|--assume-yes)
        ASSUME_DEFAULTS="true"
        shift
        ;;
      --aur-helper)
        [[ $# -ge 2 ]] || die "Missing value for --aur-helper"
        AUR_HELPER="${2,,}"
        shift 2
        ;;
      --terminal)
        [[ $# -ge 2 ]] || die "Missing value for --terminal"
        case "${2,,}" in
          st)
            INSTALL_ST="true"
            INSTALL_ALACRITTY="false"
            ;;
          alacritty)
            INSTALL_ST="false"
            INSTALL_ALACRITTY="true"
            ;;
          both)
            INSTALL_ST="true"
            INSTALL_ALACRITTY="true"
            ;;
          *)
            die "Invalid terminal option: $2"
            ;;
        esac
        shift 2
        ;;
      --slstatus)
        INSTALL_SLSTATUS="true"
        shift
        ;;
      --no-slstatus)
        INSTALL_SLSTATUS="false"
        shift
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        die "Unknown argument: $1"
        ;;
    esac
  done
}

main() {
  parse_args "$@"
  resolve_background "$BACKGROUND_PATH"
  ensure_helper "$AUR_HELPER"

  local packages=("${BASE_PACKAGES[@]}")
  if [[ "$INSTALL_ALACRITTY" == "true" ]]; then
    packages+=(alacritty)
  fi
  if [[ "$INSTALL_SLSTATUS" != "true" ]]; then
    local filtered=()
    for pkg in "${packages[@]}"; do
      [[ "$pkg" == "slstatus" ]] && continue
      filtered+=("$pkg")
    done
    packages=("${filtered[@]}")
  fi
  install_packages_with_helper "$AUR_HELPER" "${packages[@]}"

  setup_zsh
  setup_vim
  link_configs

  configure_startup

  log "------------------------------------------------------------"
  if [[ "$REBOOT_AFTER" == "true" ]]; then
    countdown_and_reboot
  else
    log "Installation complete. Log out or reboot for changes to take effect."
  fi
}

main "$@"
