#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 4096

typedef enum {
  AUR_NONE = 0,
  AUR_YAY,
  AUR_PARU,
  AUR_PAKKU
} AurHelper;

typedef struct {
  bool default_dwm;
  bool reboot_after;
  bool verbose;
  bool powersaver_off;
  const char *background_arg;
  char background[PATH_MAX];
  bool install_st;
  bool install_alacritty;
  bool install_slstatus;
  bool assume_defaults;
  AurHelper aur_helper;
  char repo_root[PATH_MAX];
} InstallerConfig;

typedef struct {
  bool cloned_st;
  bool cloned_slstatus;
  int code;
} RepoBuildStatus;

static void log_verbose(const InstallerConfig *cfg, const char *fmt, ...) {
  if (!cfg->verbose)
    return;
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
}

static int run_cmd(char *const argv[], const char *cwd) {
  pid_t pid = fork();
  if (pid == 0) {
    if (cwd && chdir(cwd) != 0) {
      perror("chdir");
      exit(1);
    }
    execvp(argv[0], argv);
    perror("execvp");
    exit(1);
  } else if (pid < 0) {
    perror("fork");
    return -1;
  } else {
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  }
}

static int run_shell(const char *cmd, const char *cwd) {
  pid_t pid = fork();
  if (pid == 0) {
    if (cwd && chdir(cwd) != 0) {
      perror("chdir");
      exit(1);
    }
    execl("/bin/bash", "bash", "-lc", cmd, (char *)NULL);
    perror("execl");
    exit(1);
  } else if (pid < 0) {
    perror("fork");
    return -1;
  } else {
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  }
}

static const char *get_home(void) {
  const char *home = getenv("HOME");
  if (!home) {
    fprintf(stderr, "Error: HOME not set.\n");
    exit(1);
  }
  return home;
}

static bool file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static bool dir_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void ensure_dir(const char *path) {
  if (dir_exists(path))
    return;
  char *const args[] = {"mkdir", "-p", (char *)path, NULL};
  if (run_cmd(args, NULL) != 0) {
    fprintf(stderr, "Error: Failed to create directory %s\n", path);
    exit(1);
  }
}

static void ensure_parent_dir(const char *path) {
  char buffer[PATH_MAX];
  strncpy(buffer, path, sizeof(buffer));
  buffer[PATH_MAX - 1] = '\0';
  char *slash = strrchr(buffer, '/');
  if (!slash)
    return;
  *slash = '\0';
  if (buffer[0] == '\0')
    return;
  ensure_dir(buffer);
}

static void backup_path(const char *path, bool verbose) {
  struct stat st;
  if (lstat(path, &st) != 0)
    return;

  if (S_ISLNK(st.st_mode)) {
    if (unlink(path) != 0 && errno != ENOENT) {
      perror("unlink");
      exit(1);
    }
    if (verbose)
      printf("[*] Removed existing symlink %s\n", path);
    return;
  }

  char backup[PATH_MAX];
  time_t now = time(NULL);
  snprintf(backup, sizeof(backup), "%s.bak.%ld", path, (long)now);
  if (rename(path, backup) != 0) {
    perror("rename");
    exit(1);
  }
  if (verbose)
    printf("[*] Backed up %s to %s\n", path, backup);
}

static void create_symlink(const char *target, const char *linkpath,
                           bool verbose) {
  ensure_parent_dir(linkpath);
  backup_path(linkpath, verbose);
  if (symlink(target, linkpath) != 0) {
    perror("symlink");
    exit(1);
  }
  if (verbose)
    printf("[*] Linked %s -> %s\n", linkpath, target);
}

static void determine_repo_root(InstallerConfig *cfg) {
  char exe_path[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
  if (len < 0) {
    perror("readlink");
    exit(1);
  }
  exe_path[len] = '\0';
  char *slash = strrchr(exe_path, '/');
  if (slash) {
    *slash = '\0';
  }
  if (!realpath(exe_path, cfg->repo_root)) {
    perror("realpath");
    exit(1);
  }
}

static void resolve_background(InstallerConfig *cfg) {
  cfg->background[0] = '\0';
  if (!cfg->background_arg)
    return;

  if (cfg->background_arg[0] == '~') {
    snprintf(cfg->background, sizeof(cfg->background), "%s%s", get_home(),
             cfg->background_arg + 1);
  } else {
    strncpy(cfg->background, cfg->background_arg, sizeof(cfg->background));
    cfg->background[sizeof(cfg->background) - 1] = '\0';
  }
}

static int prompt_menu(const char *prompt, const char *const options[],
                       int count, int default_idx) {
  char input[32];
  while (true) {
    printf("%s\n", prompt);
    for (int i = 0; i < count; i++) {
      printf("  [%d] %s%s\n", i + 1, options[i],
             i == default_idx ? " (default)" : "");
    }
    printf("> ");
    fflush(stdout);
    if (!fgets(input, sizeof(input), stdin))
      return default_idx;
    if (input[0] == '\n' || input[0] == '\0')
      return default_idx;
    char *end = NULL;
    long value = strtol(input, &end, 10);
    if (value >= 1 && value <= count)
      return (int)(value - 1);
    printf("Invalid choice. Please try again.\n");
  }
}

static bool prompt_yes_no(const char *prompt, bool default_yes) {
  char input[16];
  while (true) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(input, sizeof(input), stdin))
      return default_yes;
    if (input[0] == '\n' || input[0] == '\0')
      return default_yes;
    char c = tolower((unsigned char)input[0]);
    if (c == 'y')
      return true;
    if (c == 'n')
      return false;
    printf("Please answer with y or n.\n");
  }
}

static void gather_user_choices(InstallerConfig *cfg) {
  if (cfg->assume_defaults || !isatty(STDIN_FILENO)) {
    if (!cfg->install_st && !cfg->install_alacritty)
      cfg->install_st = true;
    return;
  }

  const char *term_options[] = {"st", "alacritty", "both"};
  int default_idx = 0;
  if (cfg->install_st && cfg->install_alacritty)
    default_idx = 2;
  else if (!cfg->install_st && cfg->install_alacritty)
    default_idx = 1;

  int choice =
      prompt_menu("Select terminal setup to install:", term_options, 3,
                  default_idx);
  switch (choice) {
  case 0:
    cfg->install_st = true;
    cfg->install_alacritty = false;
    break;
  case 1:
    cfg->install_st = false;
    cfg->install_alacritty = true;
    break;
  case 2:
  default:
    cfg->install_st = true;
    cfg->install_alacritty = true;
    break;
  }

  cfg->install_slstatus = prompt_yes_no(
      "Install slstatus (status bar companion)? [Y/n]: ", cfg->install_slstatus);

  const char *aur_options[] = {"Skip AUR helper", "Install yay",
                               "Install paru", "Install pakku"};
  int aur_choice =
      prompt_menu("Install an AUR helper to simplify future installs?",
                  aur_options, 4, 0);
  cfg->aur_helper = (AurHelper)aur_choice;
}

static void enable_pacman_color(const InstallerConfig *cfg) {
  log_verbose(cfg, "[*] Enabling colored pacman output");
  const char *cmd = "sudo sed -i 's/^#Color/Color/' /etc/pacman.conf";
  if (run_shell(cmd, NULL) != 0) {
    fprintf(stderr, "Warning: Failed to enable pacman color output.\n");
  }
}

static void install_dependencies(const InstallerConfig *cfg) {
  const char *packages[] = {
      "base-devel",       "git",                 "curl",
      "vim",              "zsh",                 "tmux",
      "feh",              "dunst",               "libnotify",
      "xorg-server",      "xorg-xinit",          "xorg-xset",
      "xorg-xrandr",      "libx11",              "libxinerama",
      "libxft",           "webkit2gtk",          "xcompmgr",
      "ttf-font-awesome", "pamixer",             "surf",
      "tabbed",           "virtualbox-guest-utils",
      "virtualbox-guest-modules-arch", NULL};

  char cmd[BUF_SIZE] = "sudo pacman -Syu --needed --noconfirm";
  for (int i = 0; packages[i]; i++) {
    strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, packages[i], sizeof(cmd) - strlen(cmd) - 1);
  }
  if (cfg->install_alacritty) {
    strncat(cmd, " alacritty", sizeof(cmd) - strlen(cmd) - 1);
  }

  log_verbose(cfg, "[*] Installing base dependencies via pacman");
  if (run_shell(cmd, NULL) != 0) {
    fprintf(stderr,
            "Error: Failed to install dependencies via pacman. Command: %s\n",
            cmd);
    exit(1);
  }
}

static void install_virtualbox_support(const InstallerConfig *cfg) {
  log_verbose(cfg, "[*] Enabling VirtualBox guest services");
  char *const args[] = {"sudo", "systemctl", "enable", "--now", "vboxservice",
                        NULL};
  if (run_cmd(args, NULL) != 0) {
    fprintf(stderr,
            "Warning: Failed to enable VirtualBox guest services. Continue "
            "manually with `sudo systemctl enable --now vboxservice`.\n");
  }
}

static void install_aur_helper(const InstallerConfig *cfg) {
  if (cfg->aur_helper == AUR_NONE)
    return;

  const char *home = get_home();
  char build_dir[PATH_MAX];
  snprintf(build_dir, sizeof(build_dir), "%s/aur_builds", home);
  ensure_dir(build_dir);

  char *const base[] = {"sudo", "pacman", "-S", "--needed", "git", "base-devel",
                        NULL};
  if (run_cmd(base, NULL) != 0) {
    fprintf(stderr, "Error: Failed to install base packages for AUR helper\n");
    exit(1);
  }

  const char *repo_url = NULL;
  const char *repo_dir = NULL;
  switch (cfg->aur_helper) {
  case AUR_YAY:
    repo_url = "https://aur.archlinux.org/yay.git";
    repo_dir = "yay";
    break;
  case AUR_PARU:
    repo_url = "https://aur.archlinux.org/paru.git";
    repo_dir = "paru";
    break;
  case AUR_PAKKU:
    repo_url = "https://aur.archlinux.org/pakku.git";
    repo_dir = "pakku";
    break;
  default:
    return;
  }

  char target_dir[PATH_MAX];
  snprintf(target_dir, sizeof(target_dir), "%s/%s", build_dir, repo_dir);

  if (dir_exists(target_dir)) {
    log_verbose(cfg, "[*] Updating existing %s repository", repo_dir);
    char *const pull[] = {"git", "pull", "--ff-only", NULL};
    if (run_cmd(pull, target_dir) != 0) {
      fprintf(stderr, "Error: Failed to update %s\n", target_dir);
      exit(1);
    }
  } else {
    log_verbose(cfg, "[*] Cloning %s", repo_url);
    char *const clone_cmd[] = {"git", "clone", (char *)repo_url, NULL};
    if (run_cmd(clone_cmd, build_dir) != 0) {
      fprintf(stderr, "Error: Failed to clone %s\n", repo_url);
      exit(1);
    }
  }

  char *const makepkg[] = {"makepkg", "-si", "--noconfirm", NULL};
  log_verbose(cfg, "[*] Building %s", repo_dir);
  if (run_cmd(makepkg, target_dir) != 0) {
    fprintf(stderr, "Error: Failed to build %s with makepkg\n", repo_dir);
    exit(1);
  }
}

static void setup_zsh(const InstallerConfig *cfg) {
  const char *home = get_home();
  char ohmyzsh_dir[PATH_MAX];
  snprintf(ohmyzsh_dir, sizeof(ohmyzsh_dir), "%s/.oh-my-zsh", home);

  if (!dir_exists(ohmyzsh_dir)) {
    log_verbose(cfg, "[*] Installing oh-my-zsh");
    const char *cmd =
        "export RUNZSH=no KEEP_ZSHRC=yes;"
        "sh -c \"$(curl -fsSL "
        "https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/"
        "install.sh)\"";
    if (run_shell(cmd, home) != 0) {
      fprintf(stderr, "Error: Failed to install oh-my-zsh\n");
      exit(1);
    }
  }

  char custom_plugins[PATH_MAX];
  snprintf(custom_plugins, sizeof(custom_plugins), "%s/custom/plugins",
           ohmyzsh_dir);
  ensure_dir(custom_plugins);

  struct {
    const char *name;
    const char *url;
  } plugins[] = {{"zsh-syntax-highlighting",
                  "https://github.com/zsh-users/zsh-syntax-highlighting.git"},
                 {"zsh-autosuggestions",
                  "https://github.com/zsh-users/zsh-autosuggestions.git"},
                 {"zsh-aur-install",
                  "https://github.com/redxtech/zsh-aur-install.git"},
                 {"zsh-vi-mode",
                  "https://github.com/jeffreytse/zsh-vi-mode.git"},
                 {"cd-ls", "https://github.com/zshzoo/cd-ls.git"},
                 {"alias-tips", "https://github.com/djui/alias-tips.git"},
                 {"zsh-completions",
                  "https://github.com/zsh-users/zsh-completions.git"},
                 {NULL, NULL}};

  for (int i = 0; plugins[i].name; i++) {
    char plugin_path[PATH_MAX];
    snprintf(plugin_path, sizeof(plugin_path), "%s/%s", custom_plugins,
             plugins[i].name);
    if (dir_exists(plugin_path)) {
      char *const pull[] = {"git", "pull", "--ff-only", NULL};
      run_cmd(pull, plugin_path);
    } else {
      char *const clone[] = {"git", "clone", (char *)plugins[i].url,
                             (char *)plugins[i].name, NULL};
      if (run_cmd(clone, custom_plugins) != 0) {
        fprintf(stderr, "Error: Failed to clone %s\n", plugins[i].url);
        exit(1);
      }
    }
  }

  char zshrc_source[PATH_MAX];
  snprintf(zshrc_source, sizeof(zshrc_source), "%s/suckless/.zshrc",
           cfg->repo_root);
  char zshrc_target[PATH_MAX];
  snprintf(zshrc_target, sizeof(zshrc_target), "%s/.zshrc", home);
  if (file_exists(zshrc_source))
    create_symlink(zshrc_source, zshrc_target, cfg->verbose);

  if (cfg->verbose) {
    printf("[*] Remember to set zsh as your default shell with "
           "`chsh -s /usr/bin/zsh` if you have not done so.\n");
  }
}

static void setup_vim(const InstallerConfig *cfg) {
  const char *home = get_home();
  char vimrc_source[PATH_MAX];
  snprintf(vimrc_source, sizeof(vimrc_source), "%s/suckless/.vimrc",
           cfg->repo_root);
  if (!file_exists(vimrc_source))
    return;
  char vimrc_target[PATH_MAX];
  snprintf(vimrc_target, sizeof(vimrc_target), "%s/.vimrc", home);
  create_symlink(vimrc_source, vimrc_target, cfg->verbose);
}

static void link_wallpaper(InstallerConfig *cfg) {
  const char *home = get_home();
  char source[PATH_MAX];
  snprintf(source, sizeof(source), "%s/suckless/.config/wallpaper",
           cfg->repo_root);
  if (!dir_exists(source))
    return;

  char target[PATH_MAX];
  snprintf(target, sizeof(target), "%s/.config/wallpaper", home);
  create_symlink(source, target, cfg->verbose);

  if (cfg->background[0] == '\0') {
    char wallpaper[PATH_MAX];
    snprintf(wallpaper, sizeof(wallpaper), "%s/tn.jpg", target);
    if (file_exists(wallpaper)) {
      strncpy(cfg->background, wallpaper, sizeof(cfg->background));
      cfg->background[sizeof(cfg->background) - 1] = '\0';
    }
  }
}

static void link_configs(InstallerConfig *cfg) {
  const char *home = get_home();
  char config_root[PATH_MAX];
  snprintf(config_root, sizeof(config_root), "%s/.config", home);
  ensure_dir(config_root);

  char repo_config_root[PATH_MAX];
  snprintf(repo_config_root, sizeof(repo_config_root),
           "%s/suckless/.config", cfg->repo_root);

  struct {
    const char *name;
    bool required;
  } dirs[] = {{"dunst", true},
              {"dwm", true},
              {"tmux", true},
              {"alacritty", cfg->install_alacritty},
              {NULL, false}};

  for (int i = 0; dirs[i].name; i++) {
    if (!dirs[i].required)
      continue;
    char source[PATH_MAX];
    snprintf(source, sizeof(source), "%s/%s", repo_config_root, dirs[i].name);
    if (!dir_exists(source))
      continue;
    char target[PATH_MAX];
    snprintf(target, sizeof(target), "%s/%s", config_root, dirs[i].name);
    create_symlink(source, target, cfg->verbose);
  }

  link_wallpaper(cfg);
}

static RepoBuildStatus clone_repos(const InstallerConfig *cfg) {
  RepoBuildStatus status = {.cloned_st = false,
                            .cloned_slstatus = false,
                            .code = cfg->default_dwm ? 2 : 1};

  const char *home = get_home();
  char suckless_dir[PATH_MAX];
  snprintf(suckless_dir, sizeof(suckless_dir), "%s/Suckless", home);
  ensure_dir(suckless_dir);

  struct {
    const char *name;
    const char *url;
    bool enabled;
  } repos[] = {{"dwm", "https://git.suckless.org/dwm", true},
               {"dmenu", "https://git.suckless.org/dmenu", true},
               {"st", "https://git.suckless.org/st", cfg->install_st},
               {"slstatus", "https://git.suckless.org/slstatus",
                cfg->install_slstatus},
               {NULL, NULL, false}};

  for (int i = 0; repos[i].name; i++) {
    if (!repos[i].enabled)
      continue;
    char repo_path[PATH_MAX];
    snprintf(repo_path, sizeof(repo_path), "%s/%s", suckless_dir,
             repos[i].name);
    if (dir_exists(repo_path)) {
      log_verbose(cfg, "[*] Updating %s", repo_path);
      char *const pull[] = {"git", "pull", "--ff-only", NULL};
      if (run_cmd(pull, repo_path) != 0) {
        fprintf(stderr, "Error: Failed to pull latest changes for %s\n",
                repo_path);
        exit(1);
      }
    } else {
      log_verbose(cfg, "[*] Cloning %s", repos[i].url);
      char *const clone[] = {"git", "clone", (char *)repos[i].url, NULL};
      if (run_cmd(clone, suckless_dir) != 0) {
        fprintf(stderr, "Error: Failed to clone %s\n", repos[i].url);
        exit(1);
      }
    }

    if (strcmp(repos[i].name, "st") == 0)
      status.cloned_st = true;
    if (strcmp(repos[i].name, "slstatus") == 0)
      status.cloned_slstatus = true;
  }

  return status;
}

static void apply_dwm_config(const InstallerConfig *cfg) {
  const char *home = get_home();
  char dwm_dir[PATH_MAX];
  snprintf(dwm_dir, sizeof(dwm_dir), "%s/Suckless/dwm", home);
  if (!dir_exists(dwm_dir))
    return;

  char source[PATH_MAX];
  snprintf(source, sizeof(source), "%s/suckless/.config/dwm/config.h",
           cfg->repo_root);
  if (!file_exists(source))
    return;

  char dest[PATH_MAX];
  snprintf(dest, sizeof(dest), "%s/config.h", dwm_dir);
  char *const cp_cmd[] = {"cp", (char *)source, (char *)dest, NULL};
  if (run_cmd(cp_cmd, NULL) != 0) {
    fprintf(stderr, "Error: Failed to copy dwm config\n");
    exit(1);
  }

  char dest_def[PATH_MAX];
  snprintf(dest_def, sizeof(dest_def), "%s/config.def.h", dwm_dir);
  char *const cp_def_cmd[] = {"cp", (char *)source, (char *)dest_def, NULL};
  if (run_cmd(cp_def_cmd, NULL) != 0) {
    fprintf(stderr, "Error: Failed to copy dwm default config\n");
    exit(1);
  }

  if (!cfg->install_alacritty && cfg->install_st) {
    const char *replace_cmd =
        "sed -i 's/\"alacritty\"/\"st\"/g' config.h config.def.h";
    if (run_shell(replace_cmd, dwm_dir) != 0) {
      fprintf(stderr, "Warning: Failed to update dwm terminal command.\n");
    }
  }
}

static void compile_selected(const InstallerConfig *cfg,
                             RepoBuildStatus status) {
  const char *home = get_home();
  const char *targets[5];
  int count = 0;

  if (cfg->install_st && status.cloned_st)
    targets[count++] = "st";
  targets[count++] = "dmenu";
  targets[count++] = "dwm";
  if (cfg->install_slstatus && status.cloned_slstatus)
    targets[count++] = "slstatus";

  for (int i = 0; i < count; i++) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/Suckless/%s", home, targets[i]);
    log_verbose(cfg, "[*] Building %s", path);
    char *const make_cmd[] = {"sudo", "make", "clean", "install", NULL};
    if (run_cmd(make_cmd, path) != 0) {
      fprintf(stderr, "Error: Failed to compile %s\n", targets[i]);
      exit(1);
    }
  }
}

static bool file_contains(const char *path, const char *needle) {
  FILE *fp = fopen(path, "r");
  if (!fp)
    return false;
  char line[1024];
  bool found = false;
  while (fgets(line, sizeof(line), fp)) {
    if (strstr(line, needle)) {
      found = true;
      break;
    }
  }
  fclose(fp);
  return found;
}

static void append_line_if_missing(const InstallerConfig *cfg, const char *path,
                                   const char *line, const char *needle) {
  if (needle && file_contains(path, needle))
    return;
  if (!needle && file_exists(path)) {
    FILE *fp_check = fopen(path, "r");
    if (fp_check) {
      char buffer[1024];
      while (fgets(buffer, sizeof(buffer), fp_check)) {
        if (strcmp(buffer, line) == 0) {
          fclose(fp_check);
          return;
        }
      }
      fclose(fp_check);
    }
  }

  FILE *fp = fopen(path, "a");
  if (!fp) {
    perror("fopen");
    exit(1);
  }
  fputs(line, fp);
  fclose(fp);
  if (cfg->verbose)
    printf("[*] Appended \"%s\" to %s\n", line, path);
}

static void init_files(const InstallerConfig *cfg, RepoBuildStatus status) {
  const char *home = get_home();
  char xinit[PATH_MAX];
  snprintf(xinit, sizeof(xinit), "%s/.xinitrc", home);

  append_line_if_missing(cfg, xinit, "xcompmgr &\n", "xcompmgr");

  if (cfg->powersaver_off)
    append_line_if_missing(cfg, xinit, "xset s off -dpms &\n", "xset s off");

  append_line_if_missing(cfg, xinit, "dunst &\n", "dunst");

  if (cfg->background[0] != '\0' && file_exists(cfg->background)) {
    char feh_line[PATH_MAX + 32];
    snprintf(feh_line, sizeof(feh_line), "feh --bg-fill %s &\n",
             cfg->background);
    append_line_if_missing(cfg, xinit, feh_line, "feh --bg-fill");
  }

  if (cfg->install_slstatus && status.cloned_slstatus)
    append_line_if_missing(cfg, xinit, "slstatus &\n", "slstatus");

  append_line_if_missing(cfg, xinit, "exec dwm\n", "exec dwm");

  char bash_profile[PATH_MAX];
  snprintf(bash_profile, sizeof(bash_profile), "%s/.bash_profile", home);
  append_line_if_missing(cfg, bash_profile, "startx\n", "startx");
}

static void countdown_and_reboot(void) {
  printf("SYSTEM WILL REBOOT IN\n");
  for (int i = 5; i > 0; i--) {
    printf("%d %.*s\n", i, i, ".....");
    fflush(stdout);
    sleep(1);
  }
  printf("SYSTEM REBOOTING NOW\n");
  fflush(stdout);
  sleep(1);
  system("sudo reboot");
}

static void usage(void) {
  printf("Usage: installer [OPTIONS]\n");
  printf("Options:\n");
  printf("  -b <PATH>     Background image path (uses feh)\n");
  printf("  -d            Install base dwm from suckless.org\n");
  printf("  -r            Reboot after completion\n");
  printf("  -v            Verbose output\n");
  printf("  -p            Enable powersaver mode (default disabled)\n");
  printf("  -y            Assume defaults (non-interactive)\n");
  printf("  -h            Show this help\n");
}

int main(int argc, char *argv[]) {
  InstallerConfig cfg = {.default_dwm = false,
                         .reboot_after = false,
                         .verbose = false,
                         .powersaver_off = true,
                         .background_arg = NULL,
                         .background = {0},
                         .install_st = true,
                         .install_alacritty = false,
                         .install_slstatus = true,
                         .assume_defaults = false,
                         .aur_helper = AUR_NONE,
                         .repo_root = {0}};

  int opt;
  while ((opt = getopt(argc, argv, "b:drvpyh")) != -1) {
    switch (opt) {
    case 'b':
      cfg.background_arg = optarg;
      break;
    case 'd':
      cfg.default_dwm = true;
      break;
    case 'r':
      cfg.reboot_after = true;
      break;
    case 'v':
      cfg.verbose = true;
      break;
    case 'p':
      cfg.powersaver_off = false;
      break;
    case 'y':
      cfg.assume_defaults = true;
      break;
    case 'h':
    default:
      usage();
      return 0;
    }
  }

  determine_repo_root(&cfg);
  resolve_background(&cfg);
  gather_user_choices(&cfg);
  enable_pacman_color(&cfg);
  install_dependencies(&cfg);
  install_virtualbox_support(&cfg);
  install_aur_helper(&cfg);
  setup_zsh(&cfg);
  setup_vim(&cfg);
  link_configs(&cfg);

  RepoBuildStatus status = clone_repos(&cfg);
  apply_dwm_config(&cfg);
  compile_selected(&cfg, status);
  init_files(&cfg, status);

  printf("------------------------------------------------------------\n");
  if (cfg.reboot_after)
    countdown_and_reboot();

  printf("REBOOT OR LOG OUT FOR CHANGES TO TAKE EFFECT!\n");
  return 0;
}
