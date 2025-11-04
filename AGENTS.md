# Repository Guidelines

## Project Structure & Module Organization
- `installer.sh` is the automation entrypoint; keep argument parsing near the top and push system side effects into focused helpers. The script assumes `yay` or `paru` is present on the PATH before execution.
- `suckless/` stores curated configs for dwm, terminals, dunst, tmux, wallpaper, and shell dotfiles; mirror the upstream directory layout when adding assets.
- `promt.txt` captures roadmap tasks—sync new behaviours with this file so future updates stay discoverable.

## Build, Test, and Development Commands
- `bash -n installer.sh` performs a quick syntax check after edits.
- `bash installer.sh -v` runs the installer in verbose mode; no prompts appear, so defaults (yay helper, st terminal, slstatus) are applied automatically.
- `bash installer.sh -v -b ~/Pictures/wall.png` adds a custom wallpaper to the non-interactive flow.
- `bash installer.sh -v --aur-helper paru` verifies the alternative helper path; `yay` or `paru` must already be installed.

## Coding Style & Naming Conventions
- Follow standard bash guidelines: guard scripts with `set -euo pipefail`, indent consistently, and use snake_case for function names.
- Reuse helpers like `create_symlink`, `install_packages_with_helper`, and `append_line` rather than sprinkling ad-hoc shell commands.
- Document new prompts or flags beside the functions that consume them so the interactive flow remains obvious.

## Testing Guidelines
- Ensure the selected AUR helper (`yay` or `paru`) is installed prior to running the script; the installer relies exclusively on that helper for package operations.
- Perform verbose dry runs after modifying package lists, prompts, or symlink logic; verify `.xinitrc` and `.bash_profile` append rather than clobber entries.
- Confirm the package installs complete without prompts and the expected desktop tools (`dwm`, `dmenu`, `st`, optional `slstatus`, `surf`, `tabbed`) are present via `pacman -Qi`.
- Inspect the resulting symlinks under `~/.config` and dotfiles to ensure they target the `suckless/` assets.

## Commit & Pull Request Guidelines
- Follow the repository’s concise, sentence-case commit subjects (e.g., “Wire up alacritty prompt”) and keep them ≤72 characters.
- In PRs, summarize behavioural changes, list manual test runs, and link related issues or TODOs; attach screenshots only when UI output changes.
- Call out any manual follow-up (e.g., `chsh`) so reviewers know which steps remain after merging.

## Security & Configuration Tips
- The installer runs with `sudo`; review every new package and system tweak before shipping.
- Keep personal data (backgrounds, keys) out of git; reference paths like `~/Pictures/wall.png` instead of committing real assets.
