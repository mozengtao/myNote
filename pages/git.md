```bash

Git管理的是修改，而不是文件

# 安装 git
sudo apt-get install git

# git 的基本配置， --global 表示这台机器上所有的Git仓库都会使用这个配置
git config --global user.name "Your Name"
git config --global user.email "email@example.com"

# 将指定目录变为Git可以管理的仓库
cd xxx
git init

# 把文件修改添加到暂存区
git add readme.txt
# 把暂存区的所有内容提交到当前分支
git commit -m "add readme.txt"

git log --oneline -3

# 回退到上一个版本，HEAD 指向的是当前的 commit_id
git reset --hard HEAD^
git reset --hard HEAD~1
git reset --hard commit_id

# 查看命令历史，方便回到未来的某个commit
git reflog

# 查看工作区修改
git diff HEAD -- readme.txt

# 查看暂存区修改
git diff --cached -- readme.txt

# 丢弃工作区的修改（1.修改后还没有被放到暂存区，撤销修改就回到和版本库一模一样的状态 2.已经添加到暂存区后，又作了修改，撤销修改就回到添加到暂存区后的状态）
git checkout -- readme.txt

# 丢弃暂存区的修改，修改会反应在工作区
git reset HEAD <file>

# 从版本库删除文件
git rm file

# 恢复版本库的删除文件
git reset HEAD file
git checkout -- file

# 关联本地仓库和github远程仓库，Git远程仓库的默认名为 origin
 git remote add origin git@github.com:USERNAME/learngit.git

# 推送本地仓库内容至远程仓库
#1 第一次推送master分支时，加上了-u参数，Git不但会把本地的master分支内容推送的远程新的master分支，还会把本地的master分支和远程的master分支关联起来，在以后的推送或者拉取时就可以简化命令
git push -u origin master  # 把当前分支master推送至远端
#2 之后只要本地作了提交，就可以通过如下命令进行提交
git push origin master

# 查看远程库信息
git remote -v

# 解除了本地仓库和远程仓库的绑定关系
git remote rm origin

# 更新本地 develop分支
git pull origin develop

# 设置 远程分支的auto track
git branch --set-upstream-to=origin/develop develop
执行该命令之后，更新本地develop分支的代码只需要执行
git pull

# clone 远端分支至本地
git clone https://github.com/mozengtao/mytest.git
git clone git@github.com:mozengtao/mytest.git


# 创建并切换分支
git checkout -b dev
git switch -c dev

# 查看本地分支
git branch

# 查看远端分支
git branch -r

# 删除本地分支
git branch -d dev
git branch -D dev

# 切换分支
git checkout dev
git switch dev

# 解决冲突
Git用<<<<<<<，=======，>>>>>>>标记出不同分支的内容
git add file1 file2 ...
git commit -m "commit msg"

# 查看分支的合并情况
git log --graph --pretty=oneline --abbrev-commit

# 暂存修改
git stash save "stash msg"
git stash list
git stash pop
git stash apply stash@{0}


# rebase
git rebase <upstream> <branch>
如果指定了 branch，则会先切换至 branch，之后再进行 rebase 操作

# 创建标签 (默认标签是打在最新提交的commit上)
git tag v1.0

# 基于历史提交的 commit id 创建标签
git tag v0.9 f52c633

# 查看标签 (标签不是按时间顺序列出，而是按字母排序)
git tag

# 查看标签信息
git show v1.0

# 创建带有说明的标签
git tag -a v0.1 -m "version 0.1 released" 1094adb

# 删除标签
git tag -d v0.1

# 推送某个标签到远程
git push origin v1.0

# 一次性推送全部尚未推送到远程的本地标签
git push origin --tags

# 如果标签已经推送到远程，要删除远程标签
#1 删除本地标签
git tag -d v0.9
#2 删除远程标签
git push origin :refs/tags/v0.9

# 自定义 Git

# Git 配置文件
~/.gitconfig

# Git 命令别名
git config --global alias.st status
git config --global alias.co checkout
git config --global alias.ci commit
git config --global alias.br branch
```

- ```bash
  #1 how to create the patch
  method 1:
  diff -u helloworld.c newhelloworld.c > helloworld.patch
  method 2:
  git format-patch -x  (replace x by the number of commits you want to extract a patch)
  ```
- ```bash
  # 常用命令
  # 显示修改内容
  git show -1
  # 只显示changeset对应的修改文件
  git show --name-only -1
  
  # 基础配置
  git config --global user.name 'XXX'
  git config --global user.email 'XXX'
  
  # 显示配置列表
  git config --list
  
  #  设置别名
  $ git config --global alias.st status
  $ git config --global alias.co checkout
  $ git config --global alias.br branch
  $ git config --global alias.ci commit
  $ git config --global alias.st status
  
  ~/.gitconfig
       User-specific configuration file. Also called "global" configuration file.
  
  #  取消别名
  git config --global --unset alias.别名
  
  # 删除远端分支
  git push origin -d  BRANCH_NAME

  # pull remote branch
  git pull origin sandbox/morrism/VCMTS-2777

  # 恢复git rm删除的文件
  git restore xxx
  ```
- [Git教程](https://www.liaoxuefeng.com/wiki/896043488029600)
- [Pro Git](https://git-scm.com/book/zh/v2/)