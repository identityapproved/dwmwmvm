set scrolloff=8
" Tab Options
set tabstop=2 
set softtabstop=2
set shiftwidth=2
set expandtab

" Disable backups and swap files
set nobackup
set nowritebackup
set noswapfile
set nocompatible

" Indentation
set smartindent

" Numbers
set number
set relativenumber

" Fix splitting
set splitbelow splitright

set hidden " Open other files if current one is not saved

set ignorecase " Ignore case when searching
set smartcase  " When searching try to be smart about cases
set hlsearch! " Don't highlight search term
set incsearch  " Jumping search

" Always show the status line
set laststatus=2

" Horisontal/vertical line on cursor location
set cursorline
" set cursorcolumn

set encoding=UTF-8

let data_dir = has('nvim') ? stdpath('data') . '/site' : '~/.vim'
if empty(glob(data_dir . '/autoload/plug.vim'))
  silent execute '!curl -fLo '.data_dir.'/autoload/plug.vim --create-dirs  https://raw.githubusercontent.com/junegunn/vim-plug
  autocmd VimEnter * PlugInstall --sync | source $MYVIMRC
endif

" Install vim-plug if not found
if empty(glob('~/.vim/autoload/plug.vim'))
  silent !curl -fLo ~/.vim/autoload/plug.vim --create-dirs
    \ https://raw.githubusercontent.com/junegunn/vim-plug/master/plug.vim
endif

" Run PlugInstall if there are missing plugins
autocmd VimEnter * if len(filter(values(g:plugs), '!isdirectory(v:val.dir)'))
  \| PlugInstall --sync | source $MYVIMRC
\| endif

call plug#begin('~/.vim/plugged')

Plug 'ghifarit53/tokyonight-vim'

Plug 'vim-airline/vim-airline'
Plug 'vim-airline/vim-airline-themes'
" Plug 'bling/vim-bufferline'

Plug 'psliwka/vim-smoothie'
Plug 'Yggdroot/indentLine'

" Plug 'mhinz/vim-sayonara', { 'on': 'Sayonara' }

Plug 'ryanoasis/vim-devicons'
Plug 'mattn/emmet-vim'

" Python
Plug 'vim-python/python-syntax'
Plug 'davidhalter/jedi-vim'

Plug 'preservim/nerdtree'
Plug 'tiagofumo/vim-nerdtree-syntax-highlight'
Plug 'PhilRunninger/nerdtree-buffer-ops'
Plug 'PhilRunninger/nerdtree-visual-selection'
Plug 'Xuyuanp/nerdtree-git-plugin'

Plug 'preservim/nerdcommenter'

Plug 'sheerun/vim-polyglot'

call plug#end()

" Airline
let g:airline#extensions#tabline#enabled = 1 " Enable the list of buffers
let g:airline#extensions#tabline#formatter = 'unique_tail_improved' " f/p/file-name.js
let g:airline_powerline_fonts = 1
let g:airline_theme='tokyonight'

" Theme
set termguicolors
let g:tokyonight_style = 'night' " available: night, storm
let g:tokyonight_enable_italic = 1
let g:tokyonight_transparent_background = 1
let g:tokyonight_menu_selection_background = 'blue'
colorscheme tokyonight

" Indentation
let g:indentLine_char_list = ['|', '¦', '┆', '┊']

let mapleader = " "
nnoremap <leader>pv :Vex<CR>
nnoremap <leader>ps :Sex<CR>
nnoremap <silent><leader><CR> :so ~/.vimrc \| :PlugInstall<CR>
nnoremap <leader>pf :GFiles<CR>
nnoremap <C-p> :Files<CR>
nnoremap <C-j> :cnext<CR>
nnoremap <C-k> :cprev<CR>

nnoremap <silent><F7> :set hlsearch!<CR>

vnoremap <leader>p "_dP
vnoremap <leader>y "+y
nnoremap <leader>y "+cy
nnoremap <leader>Y gg"+yG 
" nnoremap <C-b> <Esc>:Lex<CR>:vertical resize 23<CR>
nnoremap <silent><C-b> :NERDTreeToggle<CR>

nnoremap <PageUp>   :bprevious<CR>
nnoremap <PageDown> :bnext<CR>

vnoremap J :m '>+1<CR>gv=gv
vnoremap K :m '<-2<CR>gv=gv

" Emmet shortcuts
let g:user_emmet_mode='n'
let g:user_emmet_leader_key=','

