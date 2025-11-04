#!/usr/bin/env python3
"""
Arch-focused dwm workstation installer driven by Python.

Features:
* Prompts for preferred AUR helper, terminal stack, and optional slstatus.
* Installs required packages via the chosen helper before building suckless ports.
* Links dotfiles from the repository `suckless/` directory into $HOME.
* Installs oh-my-zsh plus curated plugins, and updates X init scripts safely.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import textwrap
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, List, Optional

# Package groups
BASE_PACKAGES = [
    "base-devel",
    "git",
    "curl",
    "vim",
    "zsh",
    "tmux",
    "feh",
    "dunst",
    "libnotify",
    "xorg-server",
    "xorg-xinit",
    "xorg-xset",
    "xorg-xrandr",
    "libx11",
    "libxinerama",
    "libxft",
    "webkit2gtk",
    "xcompmgr",
    "ttf-font-awesome",
    "pamixer",
    "surf",
    "tabbed",
    "virtualbox-guest-utils",
    "virtualbox-guest-modules-arch",
]

SHELL_PLUGINS = {
    "zsh-syntax-highlighting": "https://github.com/zsh-users/zsh-syntax-highlighting.git",
    "zsh-autosuggestions": "https://github.com/zsh-users/zsh-autosuggestions.git",
    "zsh-aur-install": "https://github.com/redxtech/zsh-aur-install.git",
    "zsh-vi-mode": "https://github.com/jeffreytse/zsh-vi-mode.git",
    "cd-ls": "https://github.com/zshzoo/cd-ls.git",
    "alias-tips": "https://github.com/djui/alias-tips.git",
    "zsh-completions": "https://github.com/zsh-users/zsh-completions.git",
}

SUCKLESS_REPOS = {
    "dwm": "https://git.suckless.org/dwm",
    "dmenu": "https://git.suckless.org/dmenu",
    "st": "https://git.suckless.org/st",
    "slstatus": "https://git.suckless.org/slstatus",
}


@dataclass
class InstallerConfig:
    background: Optional[Path] = None
    reboot_after: bool = False
    verbose: bool = False
    powersaver_off: bool = True
    assume_defaults: bool = False
    aur_helper: Optional[str] = None
    install_st: bool = True
    install_alacritty: bool = False
    install_slstatus: bool = True
    terminal_choice: Optional[str] = None
    repo_root: Path = field(default_factory=lambda: Path(__file__).resolve().parent)
    home: Path = field(default_factory=lambda: Path.home())


def log(msg: str, *, verbose: bool = True) -> None:
    if verbose:
        print(msg)


def run_cmd(
    argv: List[str],
    *,
    cwd: Optional[Path] = None,
    check: bool = True,
    capture_output: bool = False,
    text: bool = True,
) -> subprocess.CompletedProcess:
    if capture_output:
        result = subprocess.run(
            argv,
            cwd=str(cwd) if cwd else None,
            check=False,
            text=text,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    else:
        result = subprocess.run(
            argv,
            cwd=str(cwd) if cwd else None,
            check=False,
            text=text,
        )
    if check and result.returncode != 0:
        raise subprocess.CalledProcessError(result.returncode, argv, result.stdout, result.stderr)
    return result


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def backup_path(path: Path, *, verbose: bool) -> None:
    if not path.exists() and not path.is_symlink():
        return
    if path.is_symlink():
        path.unlink()
        if verbose:
            log(f"[*] Removed existing symlink {path}", verbose=True)
        return
    timestamp = int(time.time())
    backup = path.with_suffix(path.suffix + f".bak.{timestamp}")
    path.rename(backup)
    if verbose:
        log(f"[*] Backed up {path} to {backup}", verbose=True)


def create_symlink(target: Path, link_path: Path, *, verbose: bool) -> None:
    ensure_dir(link_path.parent)
    backup_path(link_path, verbose=verbose)
    link_path.symlink_to(target)
    if verbose:
        log(f"[*] Linked {link_path} -> {target}", verbose=True)


def prompt_menu(prompt: str, options: Iterable[str], default_index: int = 0) -> str:
    choices = list(options)
    while True:
        print(prompt)
        for idx, option in enumerate(choices, start=1):
            suffix = " (default)" if idx - 1 == default_index else ""
            print(f"  [{idx}] {option}{suffix}")
        response = input("> ").strip()
        if not response:
            return choices[default_index]
        if response.isdigit():
            position = int(response)
            if 1 <= position <= len(choices):
                return choices[position - 1]
        print("Invalid choice. Please try again.")


def prompt_yes_no(prompt: str, default: bool = True) -> bool:
    while True:
        choice = input(f"{prompt} ({'Y/n' if default else 'y/N'}): ").strip().lower()
        if not choice:
            return default
        if choice.startswith("y"):
            return True
        if choice.startswith("n"):
            return False
        print("Please answer with y or n.")


def enable_pacman_color(cfg: InstallerConfig) -> None:
    log("[*] Enabling pacman color output", verbose=cfg.verbose)
    command = ["sudo", "sed", "-i", "s/^#Color/Color/", "/etc/pacman.conf"]
    run_cmd(command, check=False)


def install_aur_helper(cfg: InstallerConfig, helper: str) -> str:
    helper = helper.lower()
    if helper not in {"yay", "paru", "pakku"}:
        raise ValueError("Supported AUR helpers: yay, paru, pakku")

    log(f"[*] Preparing to install {helper}", verbose=cfg.verbose)
    run_cmd(
        ["sudo", "pacman", "-S", "--needed", "--noconfirm", "git", "base-devel"],
        check=False,
    )

    build_root = cfg.home / "aur_builds"
    ensure_dir(build_root)
    repo_dir = build_root / helper
    if repo_dir.exists():
        log(f"[*] Updating existing {helper} clone", verbose=cfg.verbose)
        run_cmd(["git", "pull", "--ff-only"], cwd=repo_dir, check=False)
    else:
        run_cmd(["git", "clone", f"https://aur.archlinux.org/{helper}.git"], cwd=build_root)

    run_cmd(["makepkg", "-si", "--noconfirm"], cwd=repo_dir)
    if shutil.which(helper) is None:
        raise RuntimeError(f"{helper} was not found in PATH after installation.")
    return helper


def helper_install_command(helper: str) -> List[str]:
    base = [helper, "-S", "--needed", "--noconfirm"]
    if helper == "yay":
        base.extend(["--answerdiff", "None", "--answerclean", "None"])
    if helper == "paru":
        base.extend(["--skipreview"])
    return base


def install_packages_with_helper(
    cfg: InstallerConfig,
    helper: str,
    packages: List[str],
    optional: Optional[Iterable[str]] = None,
    fallbacks: Optional[dict[str, List[str]]] = None,
) -> None:
    if not packages:
        return
    log("[*] Installing required packages via {0}".format(helper), verbose=cfg.verbose)
    cmd = helper_install_command(helper) + packages
    result = run_cmd(cmd, check=False)
    if result.returncode == 0:
        return
    log("[!] Bulk install failed, retrying individually", verbose=True)
    optional_set = set(optional or [])
    missing: List[str] = []
    for package in packages:
        package_cmd = helper_install_command(helper) + [package]
        package_result = run_cmd(package_cmd, check=False)
        if package_result.returncode != 0:
            missing.append(package)
    fatal = [pkg for pkg in missing if pkg not in optional_set]
    handled: List[str] = []
    if optional_set and fallbacks:
        for pkg in list(missing):
            if pkg in optional_set and pkg in fallbacks:
                for fallback in fallbacks[pkg]:
                    log(f"[*] Attempting fallback {fallback} for {pkg}", verbose=True)
                    fallback_cmd = helper_install_command(helper) + [fallback]
                    if run_cmd(fallback_cmd, check=False).returncode == 0:
                        handled.append(pkg)
                        break
        fatal = [pkg for pkg in missing if pkg not in handled and pkg not in optional_set]
    if fatal:
        raise RuntimeError(
            "Failed to install the following packages via {0}: {1}".format(
                helper, ", ".join(fatal)
            )
        )
    for pkg in missing:
        if pkg not in fatal:
            log(f"[!] Optional package {pkg} could not be installed.", verbose=True)


def setup_zsh(cfg: InstallerConfig) -> None:
    ohmyzsh = cfg.home / ".oh-my-zsh"
    if not ohmyzsh.exists():
        log("[*] Installing oh-my-zsh", verbose=cfg.verbose)
        cmd = (
            "export RUNZSH=no KEEP_ZSHRC=yes && "
            "sh -c \"$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)\""
        )
        run_cmd(["bash", "-lc", cmd])

    plugins_root = ohmyzsh / "custom" / "plugins"
    ensure_dir(plugins_root)
    for name, url in SHELL_PLUGINS.items():
        dest = plugins_root / name
        if dest.exists():
            run_cmd(["git", "pull", "--ff-only"], cwd=dest, check=False)
            continue
        run_cmd(["git", "clone", url, name], cwd=plugins_root)

    repo_zshrc = cfg.repo_root / "suckless" / ".zshrc"
    target_zshrc = cfg.home / ".zshrc"
    if repo_zshrc.exists():
        create_symlink(repo_zshrc, target_zshrc, verbose=cfg.verbose)
    log(
        "Reminder: set zsh as your default shell with `chsh -s /usr/bin/zsh` if you have not already.",
        verbose=True,
    )


def setup_vim(cfg: InstallerConfig) -> None:
    repo_vimrc = cfg.repo_root / "suckless" / ".vimrc"
    if repo_vimrc.exists():
        create_symlink(repo_vimrc, cfg.home / ".vimrc", verbose=cfg.verbose)


def link_configs(cfg: InstallerConfig) -> None:
    repo_config_root = cfg.repo_root / "suckless" / ".config"
    if not repo_config_root.exists():
        return
    target_root = cfg.home / ".config"
    ensure_dir(target_root)

    config_dirs = [
        "dunst",
        "dwm",
        "tmux",
        "wallpaper",
    ]
    if cfg.install_alacritty:
        config_dirs.append("alacritty")

    for name in config_dirs:
        source = repo_config_root / name
        if source.exists():
            create_symlink(source, target_root / name, verbose=cfg.verbose)


def resolve_background(cfg: InstallerConfig, background_arg: Optional[str]) -> Optional[Path]:
    if background_arg:
        path = Path(background_arg).expanduser().resolve()
        if not path.exists():
            raise FileNotFoundError(f"Background image {path} does not exist.")
        return path

    wallpaper_dir = cfg.home / ".config" / "wallpaper"
    candidate = wallpaper_dir / "tn.jpg"
    if candidate.exists():
        return candidate

    repo_candidate = cfg.repo_root / "suckless" / ".config" / "wallpaper" / "tn.jpg"
    if repo_candidate.exists():
        return repo_candidate
    return None


def gather_terminal_choice(cfg: InstallerConfig, cli_choice: Optional[str]) -> None:
    if cli_choice:
        choice = cli_choice
    elif cfg.assume_defaults or not sys.stdin.isatty():
        choice = "st"
    else:
        choice = prompt_menu(
            "Select which terminal setup to install:",
            ["st", "alacritty", "both"],
            default_index=0,
        )
    cfg.install_st = choice in {"st", "both"}
    cfg.install_alacritty = choice in {"alacritty", "both"}


def gather_slstatus_choice(cfg: InstallerConfig, cli_enable: Optional[bool]) -> None:
    if cli_enable is not None:
        cfg.install_slstatus = cli_enable
        return
    if cfg.assume_defaults or not sys.stdin.isatty():
        cfg.install_slstatus = True
        return
    cfg.install_slstatus = prompt_yes_no("Install slstatus ?", default=True)


def gather_aur_helper(cfg: InstallerConfig, cli_helper: Optional[str]) -> str:
    if cli_helper:
        return cli_helper.lower()
    if cfg.assume_defaults or not sys.stdin.isatty():
        return "yay"
    selection = prompt_menu(
        "Choose an AUR helper to install:",
        ["yay", "paru", "pakku"],
        default_index=0,
    )
    return selection.lower()


def clone_repo(url: str, destination: Path, *, verbose: bool) -> None:
    if destination.exists():
        log(f"[*] Updating {destination.name}", verbose=verbose)
        run_cmd(["git", "pull", "--ff-only"], cwd=destination, check=False)
        return
    ensure_dir(destination.parent)
    log(f"[*] Cloning {url}", verbose=verbose)
    run_cmd(["git", "clone", url, destination.name], cwd=destination.parent)


def clone_suckless(cfg: InstallerConfig) -> None:
    suckless_root = cfg.home / "Suckless"
    ensure_dir(suckless_root)
    clone_repo(SUCKLESS_REPOS["dwm"], suckless_root / "dwm", verbose=cfg.verbose)
    clone_repo(SUCKLESS_REPOS["dmenu"], suckless_root / "dmenu", verbose=cfg.verbose)
    if cfg.install_st:
        clone_repo(SUCKLESS_REPOS["st"], suckless_root / "st", verbose=cfg.verbose)
    if cfg.install_slstatus:
        clone_repo(SUCKLESS_REPOS["slstatus"], suckless_root / "slstatus", verbose=cfg.verbose)


def apply_dwm_config(cfg: InstallerConfig) -> None:
    source = cfg.repo_root / "suckless" / ".config" / "dwm" / "config.h"
    if not source.exists():
        return
    dwm_dir = cfg.home / "Suckless" / "dwm"
    target = dwm_dir / "config.h"
    target_def = dwm_dir / "config.def.h"
    shutil.copy2(source, target)
    shutil.copy2(source, target_def)

    if not cfg.install_alacritty and cfg.install_st:
        for path in (target, target_def):
            text = path.read_text()
            text = text.replace('"alacritty"', '"st"')
            path.write_text(text)


def build_suckless(cfg: InstallerConfig) -> None:
    programs = ["dmenu", "dwm"]
    if cfg.install_st:
        programs.insert(0, "st")
    if cfg.install_slstatus:
        programs.append("slstatus")

    for program in programs:
        path = cfg.home / "Suckless" / program
        if not path.exists():
            continue
        log(f"[*] Building {program}", verbose=cfg.verbose)
        run_cmd(["sudo", "make", "clean", "install"], cwd=path)


def append_line(path: Path, line: str, *, contains: Optional[str] = None, verbose: bool = False) -> None:
    if path.exists():
        contents = path.read_text().splitlines()
        if contains and any(contains in entry for entry in contents):
            return
        if not contains and any(entry == line.strip() for entry in contents):
            return
    ensure_dir(path.parent)
    with path.open("a") as handle:
        handle.write(line if line.endswith("\n") else f"{line}\n")
    if verbose:
        log(f"[*] Added `{line.strip()}` to {path}", verbose=True)


def configure_startup(cfg: InstallerConfig) -> None:
    xinitrc = cfg.home / ".xinitrc"
    append_line(xinitrc, "xcompmgr &", contains="xcompmgr", verbose=cfg.verbose)
    append_line(xinitrc, "dunst &", contains="dunst", verbose=cfg.verbose)
    if cfg.powersaver_off:
        append_line(xinitrc, "xset s off -dpms &", contains="xset s off", verbose=cfg.verbose)
    if cfg.background:
        append_line(
            xinitrc,
            f"feh --bg-fill {cfg.background} &",
            contains="feh --bg-fill",
            verbose=cfg.verbose,
        )
    if cfg.install_slstatus:
        append_line(xinitrc, "slstatus &", contains="slstatus", verbose=cfg.verbose)
    append_line(xinitrc, "exec dwm", contains="exec dwm", verbose=cfg.verbose)

    bash_profile = cfg.home / ".bash_profile"
    append_line(bash_profile, "startx", contains="startx", verbose=cfg.verbose)


def countdown_and_reboot() -> None:
    print("SYSTEM WILL REBOOT IN")
    for remaining in range(5, 0, -1):
        print(f"{remaining} {'*' * remaining}")
        time.sleep(1)
    print("SYSTEM REBOOTING NOW")
    run_cmd(["sudo", "reboot"], check=False)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Configure an Arch Linux environment with dwm, st, and related tooling."
    )
    parser.add_argument("-b", "--background", help="Background image path passed to feh", type=str)
    parser.add_argument("-r", "--reboot", action="store_true", help="Reboot automatically after completion")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose logging")
    parser.add_argument(
        "-p",
        "--powersaver",
        dest="powersaver",
        action="store_true",
        help="Keep power saver enabled (default disables screen blanking)",
    )
    parser.add_argument("-y", "--assume-yes", action="store_true", help="Run non-interactively with defaults")
    parser.add_argument(
        "--aur-helper",
        choices=["yay", "paru", "pakku"],
        help="Specify the AUR helper to install (skips prompt)",
    )
    parser.add_argument(
        "--terminal",
        choices=["st", "alacritty", "both"],
        help="Preselect terminal installation choice",
    )
    parser.add_argument(
        "--slstatus",
        dest="slstatus",
        action="store_true",
        help="Force installation of slstatus (skips prompt)",
    )
    parser.add_argument(
        "--no-slstatus",
        dest="slstatus",
        action="store_false",
        help="Skip installing slstatus (skips prompt)",
    )
    parser.set_defaults(slstatus=None)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    cfg = InstallerConfig(
        reboot_after=args.reboot,
        verbose=args.verbose,
        powersaver_off=not args.powersaver,
        assume_defaults=args.assume_yes,
    )
    cfg.background = resolve_background(cfg, args.background)

    gather_terminal_choice(cfg, args.terminal)
    gather_slstatus_choice(cfg, args.slstatus)
    helper_choice = gather_aur_helper(cfg, args.aur_helper)

    enable_pacman_color(cfg)
    helper = install_aur_helper(cfg, helper_choice)

    packages = list(BASE_PACKAGES)
    if not cfg.install_alacritty:
        packages = [pkg for pkg in packages if pkg != "alacritty"]
    else:
        packages.append("alacritty")
    if not cfg.install_slstatus and "slstatus" in packages:
        packages.remove("slstatus")

    optional_packages = {"virtualbox-guest-modules-arch"}
    fallback_map = {"virtualbox-guest-modules-arch": ["virtualbox-guest-dkms"]}
    install_packages_with_helper(cfg, helper, packages, optional_packages, fallback_map)

    setup_zsh(cfg)
    setup_vim(cfg)
    link_configs(cfg)

    clone_suckless(cfg)
    apply_dwm_config(cfg)
    build_suckless(cfg)
    configure_startup(cfg)

    print("-" * 60)
    if cfg.reboot_after:
        countdown_and_reboot()
    else:
        print("Installation complete. Log out or reboot for changes to take effect.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        print(f"Command failed (exit code {error.returncode}): {' '.join(error.cmd)}", file=sys.stderr)
        if error.stdout:
            print(error.stdout, file=sys.stderr)
        if error.stderr:
            print(error.stderr, file=sys.stderr)
        sys.exit(error.returncode)
    except Exception as exception:
        print(f"Error: {exception}", file=sys.stderr)
        sys.exit(1)
