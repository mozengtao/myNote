- [[nvim]]
- ```bash
  # 读取外部命令的输出到当前编辑文件
  :r!date
  (查看帮助命令":help :r!")

  # Incrementing selected numbers
  my_array[0] = 0;
  "Y6p" (copy the line and paste it six times)
  my_array[0] = 0;
  my_array[0] = 0;
  my_array[0] = 0;
  my_array[0] = 0;
  my_array[0] = 0;
  my_array[0] = 0;
  my_array[0] = 0;
  With the cursor on the first 0 in the first line, start a blockwise select by pressing "Ctrl-V" 
  Move the cursor down(j) to select the first column of zeros,(Exclude first line by pressing "o") then   press "g Ctrl-A"
  my_array[1] = 0;
  my_array[2] = 0;
  my_array[3] = 0;
  my_array[4] = 0;
  my_array[5] = 0;
  my_array[6] = 0;
  my_array[7] = 0;

  # Repeat last change
  The "." command repeats the last change made in normal mode
  The "@:" command repeats the last command-line change (a command invoked with ":")

  # 结合tmux把当前行的内容作为命令发送给指定的窗口
  :.w !xargs -0ri tmux send -t4 {}
  :silent .w !xargs -0ri tmux send -t4 {}
  :nmap <enter> :silent .w !xargs -0ri tmux send -t4 {}<cr>j	// 使用回车键自动执行当前行命令并到下一行
  :nmap <enter> :silent .w !awk '{print $NF}' \| xargs -i tmux neww git difftool -y {}<cr>
  （tmux环境下 Ctrl + q 可以显示pane number）

  :nmap <C-x> :silent .w !xargs -0ri tmux send -t1 {}<cr>
  
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
- [Making a list of numbers](https://vim.fandom.com/wiki/Making_a_list_of_numbers)
- [Mapping keys in Vim](https://vim.fandom.com/wiki/Mapping_keys_in_Vim_-_Tutorial_(Part_1))
- [Repeat last change](https://vim.fandom.com/wiki/Repeat_last_change)
- [Vim Documentation](https://vim-jp.org/vimdoc-en/)
- [NVIM - help](https://neovim.io/doc/user/)