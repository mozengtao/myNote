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