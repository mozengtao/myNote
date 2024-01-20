- [[nvim]]
- ```bash
  # enable paste mode
  :set paste
  ```
- ```bash
  seq 3 | vim -
  (The key is to use the - after the vim command to make it read from stdin. This will open a new 
  buffer with the output of the previous command.)
  	-           The  file  to  edit  is read from stdin.  Commands are read from stderr,
                     which should be a TTY.
  
  
  ```
- ### 显示不可打印字符
	- ```vim
	  :set listchars=eol:¬,tab:>·,trail:~,extends:>,precedes:<,space:␣
	  :set list
	  ```
- ### 搜索忽略大小写
	- ```vim
	  /xxx\c
	  ```
- ### 快捷键
	- https://zhuanlan.zhihu.com/p/19632777
	- section backward: \[\[ (可用来返回函数头部)
	- 返回之前的光标位置: Ctrl + o
	- 返回比较新的光标位置: Ctrl + i
	- 选中区域后可执行替换命令，替换只在指定区域发生
### Global command
	- 删除匹配行: `:1,10g/pattern/d`
	- 删除不匹配行: `:1,10g!/pattern/d`
- ### 重定向编辑
	- `strace echo aaa | vim -`
### 插件
	- ctags
	- cscope
	- taglist
	- ctrlp
- ### 宏
	- start
		- `q + a`
	- stop
		- `q`
	- repeat
		- `N@a`
- `~/.vimrc`
	- ```bash
	  syntax enable
	  
	  nmap <C-\>s :cs find s <C-R>=expand("<cword>")<CR><CR>
	  nmap <C-\>g :cs find g <C-R>=expand("<cword>")<CR><CR>
	  nmap <C-\>c :cs find c <C-R>=expand("<cword>")<CR><CR>
	  nmap <C-\>t :cs find t <C-R>=expand("<cword>")<CR><CR>
	  nmap <C-\>e :cs find e <C-R>=expand("<cword>")<CR><CR>
	  nmap <C-\>f :cs find f <C-R>=expand("<cfile>")<CR><CR>
	  nmap <C-\>i :cs find i ^<C-R>=expand("<cfile>")<CR>$<CR>
	  nmap <C-\>d :cs find d <C-R>=expand("<cword>")<CR><CR>
	  nmap <C-\>b :TlistToggle<CR><CR>
	  
	  filetype plugin indent on
	  set autoindent
	  set smartindent
	  
	  set nu
	  set paste
	  set runtimepath^=~/.vim/bundle/ctrlp.vim
	  let g:ctrlp_map = '<c-p>'
	  set listchars=eol:¬,tab:>·,trail:~,extends:>,precedes:<,space:␣
	  ```
- [cscope快速教程](https://yiwenshao.github.io/2016/12/25/cscope%E5%BF%AB%E9%80%9F%E6%95%99%E7%A8%8B/)
- [The Vim/Cscope tutorial](https://cscope.sourceforge.net/cscope_vim_tutorial.html)
- [vim-snippets](https://github.com/honza/vim-snippets)
-