[[nvim]]

[**vimllearn**](https://github.com/lymslive/vimllearn)  
[Builtin functions](https://vimhelp.org/builtin.txt.html)  
[Write a Vim script](https://vimdoc.sourceforge.net/htmldoc/usr_41.html)  

[Learn Vimscript the Hard Way](https://learnvimscriptthehardway.stevelosh.com/)  
[Learn Vim the Smart Way](https://learnvim.irian.to/)  
[Vim help files](https://vimhelp.org/#reference_toc)  


[Mapping keys in Vim](https://vim.fandom.com/wiki/Mapping_keys_in_Vim_-_Tutorial_(Part_1))  
[vim-unimpaired](https://github.com/tpope/vim-unimpaired/tree/master)  
[Buffers](https://evantravers.com/articles/series/git-gud-at-vim/)  
[Complete Guide to Buffers in Vim](https://linuxhandbook.com/vim-buffers/)  
[Buffers](https://mkaz.blog/working-with-vim/buffers)  
[Why I love Vim: It’s the lesser-known features that make it so amazing](https://www.freecodecamp.org/news/learn-linux-vim-basic-features-19134461ab85/)  
[]()  
[Vim Cheat Sheet](https://vim.rtorr.com/)  
[]()  

## vim 常用技巧
```bash
## VIM cmds
:r[ead] !{cmd}      # Execute {cmd} and insert its standard output below the cursor or the specified line.

## Session
A Session keeps the Views for all windows, plus the global settings.
:mks[ession][!] [file]
vim -S Session.vim
or
:source Session.vim

## viminfo
The viminfo file can be used to remember that information, which enables you to continue where you left off
:wv[iminfo][!] [file]
:rv[iminfo][!] [file]

## 从外部缓冲区粘贴
:set paste

## 重定向

# 重定向 stdout 到 vim
seq 3 | vim -     # use the - after the vim command to make it read from stdin

# 同时重定向 stdout 和 stderr 到 vim
snmpwalk -h |& vim -
snmpwalk -h  2>&1 | vim -

## vim + tmux

# 在 Vim 中按下 Ctrl + x 时，将光标所在行的内容通过 xargs 发送到 tmux 会话（第 0 个窗口）
nmap <C-x> :silent .w !xargs -0ri tmux send -t0 {}<cr>

## 搜索忽略大小写
/xxx\c

## 光标位置操作
[[  # 返回函数头部
]]  # 返回函数尾部
Ctrl + o  # 返回之前的光标位置
Ctrl + i  # 返回光标位置

## 删除
:1,10g/pattern/d  # 删除匹配行
:1,10g!/pattern/d # 删除非匹配行

## 插件
ctags
cscope
taglist
ctrlp

## cscope 快捷键
nmap <C-\>s :cs find s <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>g :cs find g <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>c :cs find c <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>t :cs find t <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>e :cs find e <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>f :cs find f <C-R>=expand("<cfile>")<CR><CR>
nmap <C-\>i :cs find i ^<C-R>=expand("<cfile>")<CR>$<CR>
nmap <C-\>d :cs find d <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>b :TlistToggle<CR><CR>

Ctrl + d
```

[vi/vim使用进阶](https://blog.easwy.com/archives/advanced-vim-skills-catalog/)  
[cscope快速教程](https://yiwenshao.github.io/2016/12/25/cscope%E5%BF%AB%E9%80%9F%E6%95%99%E7%A8%8B/)  
[Cscope Tutorial](https://courses.cs.washington.edu/courses/cse451/12sp/tutorials/tutorial_cscope.html)  
[CSCOPE manpage](https://cscope.sourceforge.net/cscope_man_page.html)  
[The Vim/Cscope tutorial](https://cscope.sourceforge.net/cscope_vim_tutorial.html)  
[vim-snippets](https://github.com/honza/vim-snippets)  
[Making a list of numbers](https://vim.fandom.com/wiki/Making_a_list_of_numbers)  
[Mapping keys in Vim](https://vim.fandom.com/wiki/Mapping_keys_in_Vim_-_Tutorial_(Part_1))  
[Repeat last change](https://vim.fandom.com/wiki/Repeat_last_change)  
[Vim Documentation](https://vim-jp.org/vimdoc-en/)  
[NVIM - help](https://neovim.io/doc/user/)  
[Vim: Keymapping Guide](https://www.meetgor.com/vim-keymaps/)  
[vim tips and tricks](https://www.cs.swarthmore.edu/oldhelp/vim/home.html)  
[learnbyexample](https://learnbyexample.github.io/tags/vim/)  
[Vim Cheat Sheet](https://vim.rtorr.com/)  
[vim-and-the-shell](https://vimways.org/2019/vim-and-the-shell/)  
[External Commands](https://learnvim.irian.to/basics/external_commands)  
[athame](https://github.com/ardagnir/athame)  
[Use Vim Mode On The Command Line In Bash](https://dev.to/brandonwallace/how-to-use-vim-mode-on-the-command-line-in-bash-fnn)  
[CSCOPE settings for vim](https://insidelinuxdev.net/~yuanjianpeng/config/cscope_maps.vim)  
[A guide on Neovim's LSP client](https://vonheikemen.github.io/devlog/tools/neovim-lsp-client-guide/)  
[使用 LSP 搭建代码补全环境](https://breezetemple.github.io/2019/12/25/vim-lsp/)  