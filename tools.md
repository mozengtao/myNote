## fzf
```bash
# fuzzy search (default)
text
txt

# exact search
<<EOF
'text
EOF

# 
^text

#
text$


```

- fzf configuration under fish shell
```bash
# ~/.config/fish/conf.d/fzf.fish
# 先确保 fzf 在 PATH 中
if not contains ~/.fzf/bin $PATH
    set -gx PATH ~/.fzf/bin $PATH
end

# 检查 fzf 是否可用
if not command -v fzf > /dev/null
    echo "错误: fzf 未找到，请运行 ~/.fzf/install 安装"
    exit 1
end

# 加载 fzf 的键绑定函数
if test -f ~/.fzf/shell/key-bindings.fish
    source ~/.fzf/shell/key-bindings.fish
else
    echo "警告: 未找到 fzf 键绑定文件"
end

# 基础配置
set -gx FZF_DEFAULT_OPTS '--height 40% --layout=reverse --border --preview "bat --color=always {} 2>/dev/null || ls -la {}"'

# 使用 fd 提高性能
if command -v fd > /dev/null
    set -gx FZF_DEFAULT_COMMAND 'fd --type f --hidden --follow --exclude .git'
    set -gx FZF_ALT_C_COMMAND 'fd --type d --hidden --follow --exclude .git'
end

# 定义自定义函数（在调用之前先定义）
function fzf_file_search
    set -l file (fzf --preview 'bat --color=always {} 2>/dev/null || cat {}')
    if test -n "$file"
        commandline -i "$file"
    end
end

function fzf_fd_search
    if command -v fd > /dev/null
        set -l file (fd --type f --hidden --exclude .git | fzf --preview 'bat --color=always {}')
        if test -n "$file"
            commandline -i "$file"
        end
    else
        echo "fd 未安装，使用 find 替代"
        set -l file (find . -type f | fzf --preview 'bat --color=always {} 2>/dev/null || cat {}')
        if test -n "$file"
            commandline -i "$file"
        end
    end
end

function fzf_directory_search
    if command -v fd > /dev/null
        set -l dir (fd --type d --hidden --exclude .git | fzf --preview 'ls -la {}')
    else
        set -l dir (find . -type d | fzf --preview 'ls -la {}')
    end
    if test -n "$dir"
        cd "$dir"
        commandline -f repaint
    end
end

function fzf_kill_process
    set -l pid (ps aux | fzf | awk '{print $2}')
    if test -n "$pid"
        kill -9 $pid
    end
end

function fzf_git_files
    if git rev-parse --git-dir > /dev/null 2>&1
        set -l file (git ls-files | fzf --preview 'bat --color=always {} 2>/dev/null || cat {}')
        if test -n "$file"
            commandline -i "$file"
        end
    else
        echo "当前目录不是 Git 仓库"
    end
end

function fzf_git_branches
    if git rev-parse --git-dir > /dev/null 2>&1
        set -l branch (git branch -a | fzf | sed 's/^.* //' | sed 's#remotes/[^/]*/##')
        if test -n "$branch"
            git checkout $branch
        end
    else
        echo "当前目录不是 Git 仓库"
    end
end

# 配置键绑定（只有在函数存在时才绑定）
if functions -q fzf_configure_bindings
    fzf_configure_bindings --directory=\co --history=\cr --git_log=\cg\l --git_status=\cg\s --variables=\ce
else
    echo "使用自定义键绑定"
    
    # 自定义快捷键绑定
    bind \cp fzf_file_search
    bind \cf fzf_fd_search
    bind \ec fzf_directory_search
    bind \ek fzf_kill_process
    bind \cg\cf fzf_git_files
    bind \cg\cb fzf_git_branches
    
    # 添加一些标准绑定
    bind \cr 'history | fzf | read -l cmd; and commandline -rb "$cmd"; commandline -f repaint'
    bind \co 'cd (find . -type d | fzf); commandline -f repaint'
end

echo "fzf 配置加载完成"

# install
git clone --depth 1 https://github.com/junegunn/fzf.git ~/.fzf
~/.fzf/install

# source
source ~/.config/fish/conf.d/fzf.fish

vim (Ctrl + P)
```


[微信文件传输助手网页版](https://filehelper.weixin.qq.com/)  
[[makeself]]  
[[dot]]  
[gnuplot](https://gnuplot.sourceforge.net/demo/)  
[it-tools](https://it-tools.tech/) #online  
[it-tools](https://github.com/CorentinTh/it-tools) #github  