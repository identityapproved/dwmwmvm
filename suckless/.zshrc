# If you come from bash you might have to change your $PATH. export 
# PATH=$HOME/bin:/usr/local/bin:$PATH

# Path to your oh-my-zsh installation.
export ZSH="$HOME/.oh-my-zsh"

# one was loaded, run: echo $RANDOM_THEME See 
# https://github.com/ohmyzsh/ohmyzsh/wiki/Themes
# https://github.com/romkatv/powerlevel10k
ZSH_THEME="random"

zstyle ':omz:update' mode auto # disable automatic updates zstyle 

# Custom plugins
# https://github.com/zsh-users/zsh-syntax-highlighting/blob/master/INSTALL.md
# https://github.com/zsh-users/zsh-autosuggestions/blob/master/INSTALL.md
# https://github.com/redxtech/zsh-aur-install
# https://github.com/zshzoo/cd-ls
# https://github.com/jeffreytse/zsh-vi-mode
plugins=(git pip python archlinux zsh-aur-install zsh-syntax-highlighting zsh-autosuggestions zsh-vi-mode tmux cd-ls alias-tips themes)

# https://github.com/zsh-users/zsh-completions
fpath+=${ZSH_CUSTOM:-${ZSH:-~/.oh-my-zsh}/custom}/plugins/zsh-completions/src

# https://github.com/ohmyzsh/ohmyzsh
source $ZSH/oh-my-zsh.sh
source ~/.oh-my-zsh/custom/plugins/alias-tips/alias-tips.plugin.zsh

export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"  # This loads nvm
[ -s "$NVM_DIR/bash_completion" ] && \. "$NVM_DIR/bash_completion"  # This loads nvm bash_completion
