# Repository Guidelines

## Project Structure & Module Organization
- `installer.c` is the only automation entrypoint; keep CLI parsing at the top and isolate side effects in focused helpers.
- `suckless/` stores curated configs for dwm, terminals, dunst, tmux, wallpaper, and shell dotfiles; mirror the upstream directory layout when adding assets.
- `promt.txt` captures roadmap tasks—sync new behaviours with this file so future updates stay discoverable.

## Build, Test, and Development Commands
- `gcc installer.c -o installer` rebuilds the binary; rerun after any structural change.
- `./installer -v` walks through prompts and prints trace output; run inside an Arch VM before submitting changes.
- `./installer -v -y -b ~/Pictures/wall.png` exercises the non-interactive path with a custom background and confirms default choices hold.
- `./installer -v -d` forces official suckless sources (matches production expectation).

## Coding Style & Naming Conventions
- Stick with two-space indentation, snake_case helpers, and keep public structs near the top (`installer.c:16-42`).
- Route all shell execution through `run_cmd` / `run_shell` and emit verbose logs via `log_verbose` (`installer.c:44-92`).
- Document new prompts beside their handling function so the interactive flow stays obvious.

## Testing Guidelines
- Perform verbose dry runs after modifying package lists, prompts, or symlink logic; verify `.xinitrc` and `.bash_profile` append rather than clobber entries (`installer.c:708-735`).
- Confirm the installer clones and compiles `dwm`, `dmenu`, `st`, and optional `slstatus` from `git.suckless.org` (`installer.c:541-655`).
- Inspect the resulting symlinks under `~/.config` and dotfiles to ensure they target the `suckless/` assets (`installer.c:485-535`).

## Commit & Pull Request Guidelines
- Follow the repository’s concise, sentence-case commit subjects (e.g., “Wire up alacritty prompt”) and keep them ≤72 characters.
- In PRs, summarize behavioural changes, list manual test runs, and link related issues or TODOs; attach screenshots only when UI output changes.
- Call out any manual follow-up (e.g., `chsh`) so reviewers know which steps remain after merging.

## Security & Configuration Tips
- The installer runs with `sudo`; review every new package and system tweak before shipping.
- Keep personal data (backgrounds, keys) out of git; reference paths like `~/Pictures/wall.png` instead of committing real assets.
