# Repository Guidelines

## Project Structure & Module Organization
- `installer.py` is the automation entrypoint; keep argument parsing near the top and push system side effects into focused helpers.
- `suckless/` stores curated configs for dwm, terminals, dunst, tmux, wallpaper, and shell dotfiles; mirror the upstream directory layout when adding assets.
- `promt.txt` captures roadmap tasks—sync new behaviours with this file so future updates stay discoverable.

## Build, Test, and Development Commands
- `python3 -m py_compile installer.py` performs a quick syntax check after edits.
- `python3 installer.py -v` walks through prompts and prints trace output; run inside an Arch VM before submitting changes.
- `python3 installer.py -v -y -b ~/Pictures/wall.png` exercises the non-interactive path with a custom background and confirms default choices hold.
- `python3 installer.py -v --aur-helper paru` validates each helper path; repeat for `yay` and `pakku` when touching that logic.

## Coding Style & Naming Conventions
- Follow PEP 8 with four-space indents and snake_case helpers; keep dataclass definitions near the top (`installer.py:46-71`).
- Use `run_cmd`, `create_symlink`, and other utility wrappers instead of raw `subprocess.run` to keep error handling consistent.
- Document new prompts or flags beside the functions that consume them so the interactive flow remains obvious.

## Testing Guidelines
- Perform verbose dry runs after modifying package lists, prompts, or symlink logic; verify `.xinitrc` and `.bash_profile` append rather than clobber entries (`installer.py:307-341`).
- Confirm the installer clones and compiles `dwm`, `dmenu`, `st`, and optional `slstatus` from `git.suckless.org` (`installer.py:210-278`).
- Inspect the resulting symlinks under `~/.config` and dotfiles to ensure they target the `suckless/` assets (`installer.py:181-208`).

## Commit & Pull Request Guidelines
- Follow the repository’s concise, sentence-case commit subjects (e.g., “Wire up alacritty prompt”) and keep them ≤72 characters.
- In PRs, summarize behavioural changes, list manual test runs, and link related issues or TODOs; attach screenshots only when UI output changes.
- Call out any manual follow-up (e.g., `chsh`) so reviewers know which steps remain after merging.

## Security & Configuration Tips
- The installer runs with `sudo`; review every new package and system tweak before shipping.
- Keep personal data (backgrounds, keys) out of git; reference paths like `~/Pictures/wall.png` instead of committing real assets.
