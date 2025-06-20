```bash

Git管理的是修改，而不是文件

# git-worktree - Manage multiple working trees
git worktree add <path> <branch>      	    # 创建新工作树并检出指定分支
git worktree add <path> -b <new-branch>	    # 创建新工作树并新建分支
git worktree list	                          # 列出所有关联的工作树
git worktree remove <worktree>	            # 安全删除工作树
git worktree prune	                        # 清理无效工作树记录

# git-format-patch - Prepare patches for e-mail submission
it format-patch <commit>	                  # 从指定提交生成补丁（不含该提交）
git format-patch <start>..<end>	            # 生成区间提交的补丁（左开右闭）
git format-patch -n <commit>	              # 生成最近 n 个提交的补丁
git format-patch --stdout > all.patch	      # 输出到 stdout（适合管道操作）
git format-patch --cover-letter	            # 生成补丁封面（含修改综述）

# git-show - Show various types of objects
git show -1
git show --name-only -1
git show abcd123	              # 显示提交详情
git show main	                  # 显示分支最新提交
git show v1.0	                  # 显示标签详情
git show HEAD:README.md	        # 显示历史版本文件内容
git show HEAD^{tree}	          # 显示目录结构

# git-submodule - Initialize, update or inspect submodules
```

[Git教程](https://www.liaoxuefeng.com/wiki/896043488029600)  
[Pro Git](https://git-scm.com/book/zh/v2/)  
[Submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules)  
[Demystifying git submodules](https://www.cyberdemon.org/2024/03/20/submodules.html)  
[]()  
[]()  
[]()  
[]()  
[]()  