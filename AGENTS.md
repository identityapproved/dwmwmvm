# Repository Guidelines

## Project Structure & Module Organization
- `installer.sh` is the automation entrypoint; keep argument parsing near the top and push system side effects into focused helpers.
- `suckless/` stores curated configs for dwm, terminals, dunst, tmux, wallpaper, and shell dotfiles; mirror the upstream directory layout when adding assets.
- `promt.txt` captures roadmap tasks—sync new behaviours with this file so future updates stay discoverable.

## Build, Test, and Development Commands
- `bash -n installer.sh` performs a quick syntax check after edits.
- `bash installer.sh -v` walks through prompts and prints trace output; run inside an Arch VM before submitting changes.
- `bash installer.sh -v -y -b ~/Pictures/wall.png` exercises the non-interactive path with a custom background and confirms default choices hold.
- `bash installer.sh -v --aur-helper paru` validates each helper path; repeat for `yay` and `pakku` when touching that logic.

## Coding Style & Naming Conventions
- Follow standard bash guidelines: guard scripts with `set -euo pipefail`, indent consistently, and use snake_case for function names.
- Reuse helpers like `create_symlink`, `install_packages_with_helper`, and `append_line` rather than sprinkling ad-hoc shell commands.
- Document new prompts or flags beside the functions that consume them so the interactive flow remains obvious.

## Testing Guidelines
- Perform verbose dry runs after modifying package lists, prompts, or symlink logic; verify `.xinitrc` and `.bash_profile` append rather than clobber entries.
- Confirm the installer clones and compiles `dwm`, `dmenu`, `st`, and optional `slstatus` from `git.suckless.org`.
- Inspect the resulting symlinks under `~/.config` and dotfiles to ensure they target the `suckless/` assets.

## Commit & Pull Request Guidelines
- Follow the repository’s concise, sentence-case commit subjects (e.g., “Wire up alacritty prompt”) and keep them ≤72 characters.
- In PRs, summarize behavioural changes, list manual test runs, and link related issues or TODOs; attach screenshots only when UI output changes.
- Call out any manual follow-up (e.g., `chsh`) so reviewers know which steps remain after merging.

## Security & Configuration Tips
- The installer runs with `sudo`; review every new package and system tweak before shipping.
- Keep personal data (backgrounds, keys) out of git; reference paths like `~/Pictures/wall.png` instead of committing real assets.
