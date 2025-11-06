## git commit
```bash
# typical(non-merge) case
A -- B -- C (HEAD)

HEAD^					B
HEAD~ or HEAD~1			B
HEAD~2					A

# merge commit case
       D (feature)
      /
A -- B --------- C (HEAD)
         \
          E (merged)

C has 2 parents:
First parent: mainline (usually the branch you were on when you did git merge)
Second parent: the merged branch

HEAD^ or HEAD^1			B
HEAD^2					E
HEAD~1					B
HEAD~2					A
```

## bash example
```bash
#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

if [ "$#" -lt 3 ]; then
  printf 'Usage: %s <JIRA_TYPE(bug|story)> <JIRA_ID> <REPOS(cms, docsis-mac, ...)>\n' "$0" >&2
  exit 1
fi

declare -a vcmts_repos=("cms" "meta-vcore")

in_array() {
    local needle=$1
    shift
    local haystack=("$@")

    for e in "${haystack[@]}"; do
        if [[ $e == "$needle" ]]; then
            return 0  # found
        fi
    done
    return 1  # not found
}

JIRA_TYPE="$1"
JIRA_ID="$2"
shift
shift
if [[ "$JIRA_TYPE" != "bug" && "$JIRA_TYPE" != "story" ]]; then
  printf 'Invalid JIRA_TYPE: %s. Must be "bug" or "story".\n' "$JIRA_TYPE" >&2
  exit 1
fi

# Process repositories
declare -a repos
while [[ "$#" -gt 0 ]]; do
  if ! in_array "$1" "${vcmts_repos[@]}"; then
      echo "Unsupported repository: $1"
      exit 1
  fi
  repos+=("$1")
  shift
done

REPO_DIR="/home/morrism/code"
REVIEW_BRANCH="morris/story/$JIRA_ID"
SANDBOX_BRANCH="sandbox/morrism/$JIRA_ID"

for repo in "${repos[@]}"; do
  # Verify repository exists
  repo_path="$REPO_DIR/$repo"
  if [ ! -d "$repo_path" ]; then
    printf '%s does not exist\n' "$repo_path" >&2
    exit 1
  fi

  pushd "$repo_path" > /dev/null

  # Ensure there is no uncommitted changes
  if ! git diff --quiet --exit-code || ! git diff --cached --quiet --exit-code; then
    printf 'Uncommitted changes exist in %s. Please commit or stash them before running this script.\n' "$repo"
    popd > /dev/null
    exit 1
  fi

  # Ensure review branch exists locally
  if ! git rev-parse --verify --quiet "refs/heads/$REVIEW_BRANCH"; then
    printf 'Review branch %s does not exist in %s\n' "$REVIEW_BRANCH" "$repo"
    popd > /dev/null
    exit 1
  fi

  # Ensure we are on review branch  if not
  current_branch="$(git rev-parse --abbrev-ref HEAD)"
  if [ "$current_branch" != "$REVIEW_BRANCH" ]; then
    printf 'Checking out %s in %s\n' "$REVIEW_BRANCH" "$repo"
    git checkout "$REVIEW_BRANCH"
  fi

  # If sandbox branch exists, delete it
  if git show-ref --verify --quiet "refs/heads/$SANDBOX_BRANCH"; then
    printf 'Deleting %s and recreating from %s in %s\n' "$SANDBOX_BRANCH" "$REVIEW_BRANCH" "$repo"
    git branch -D "$SANDBOX_BRANCH"
  else
    printf 'Creating %s from %s in %s\n' "$SANDBOX_BRANCH" "$REVIEW_BRANCH" "$repo"
  fi

  # Create sandbox branch from review branch
  git checkout -b "$SANDBOX_BRANCH" "$REVIEW_BRANCH"

  # If the intent was to re-create the most recent commit only, a safer approach is:
  # git reset --mixed HEAD~1
  # if git diff --quiet --exit-code; then
  #   git commit --allow-empty -m "$(git log -1 --pretty=%s)"
  # else
  #   git commit -am "$(git log -1 --pretty=%s)"
  # fi

  # re-create the most recent commit
  commit_msg="$(git log -1 --pretty=%s)"
  git reset --mixed HEAD~1
  git commit -am "$commit_msg"

  git push -f origin "$SANDBOX_BRANCH"
  popd > /dev/null
done
```

## Basic
```bash

Git管理的是修改，而不是文件

#
git diff main -- <file-path>

git checkout main -- <file-path>
git restore --source main -- <file-path>

git checkout main -- file1.txt file2.txt
git restore --source main -- file1.txt file2.txt

git checkout main -- .
git restore --source main -- .

git reset HEAD <file-path>
git checkout main -- <file-path>

# Check if branch exists (returns exit code 0 if exists, 1 if not)
git show-ref --verify --quiet refs/heads/branch-name

# show current branch
git branch --show-current

# get commit message
git log --oneline -1 | awk '{print}'
0186ae7 Send upstream channel stats metrics to collectd

# 1 awk (clean and idiomatic)
git log --oneline -1 | awk '{$1=""; sub(/^ /,""); print}'

# 2
# -f2- → print from field 2 to end
git log --oneline -1 | cut -d' ' -f2-

# 3
git log -1 --pretty=%s

# delete branch
# delete local branch
git branch -d feature-branch
git branch -D feature-branch  # Force delete local branch

# delete remote branch
git push origin --delete feature-branch

# global ignore file
git config --global core.excludesfile ~/.gitignore_global
echo ".cache/" >> ~/.gitignore_global

# alias
git config --global alias.review '!f() { git review -t "$(git branch --show-current | xargs basename)"; }; f'
# test git alias works or not
git -c alias.review='!f(){ echo git review -t "$(git branch --show-current | xargs basename)"; }; f' review
# Use a default, override with first param
git config alias.review '!f(){ git review -t "${1:-$(git branch --show-current | xargs basename)}"; }; f'
# Usage:
git review            # uses current branch basename
git review my-topic   # overrides topic with "my-topic"

## revert a specific file
git log <file>
git restore --source=<hash> -- <file>
or
git checkout <hash> -- <file>
git commit -m "Revert file to old commit <hash>"

# disable the pager for a single git command invocation
git --no-pager diff
git --no-pager show -1

# -C <path>
Run as if git was started in <path> instead of the current working directory.
git -C cms status
git -C topPath -C subModulePath

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

# git-branch - List, create, or delete branches
git branch --show-current

# git-commit - Record changes to the repository
git commit --amend            # 修改最近提交的提交信息
git commit --amend --no-edit  # 将当前修改追加到上一次提交中，不修改提交信息

# git-checkout - Switch branches or restore working tree files
git checkout -- <file>  # 撤销工作区修改​​

# git-switch - Switch branches
git switch -            # 切回上一个分支
git checkout -          # 切回上一个分支

# git-restore - Restore working tree files
git restore <file>      # 撤销工作区修改
git checkout -- <file>  # 撤销工作区修改​​

# git-reflog - Manage reflog information
git reflog

# git-reset - Reset current HEAD to the specified state
git reset HEAD <file>         # 将文件移出暂存区，但保留修改
git reset --hard origin/main  # 强制重置到远程状态​

# git-log - Show commit logs
git log --graph --oneline --decorate --all

# git-rebase - Reapply commits on top of another base tip
git rebase -i <commit>    # 交互式变基
git rebase -i HEAD~3      # 合并最近3个提交

# git-clean - Remove untracked files from the working tree
git clean -fd           # ​​清理未跟踪文件
```

[Git教程](https://www.liaoxuefeng.com/wiki/896043488029600)  
[Pro Git](https://git-scm.com/book/zh/v2/)  
[Submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules)  
[Demystifying git submodules](https://www.cyberdemon.org/2024/03/20/submodules.html)  
[]()  
[Ultimate Guide to Git Commit Message Templates: Best Practices and Examples](https://axolo.co/blog/p/git-commit-messages-best-practices-examples)  
[How to Write Good Commit Messages: A Practical Git Guide](https://www.freecodecamp.org/news/writing-good-commit-messages-a-practical-guide/)  
[How to Write a Git Commit Message](https://cbea.ms/git-commit/)  
[Git commit message best practices](https://graphite.dev/guides/git-commit-message-best-practices)  
```bash
# format
subject
body
footer

# example
fix(parser): handle unexpected end-of-file error

Ensure that errors close the socket and reinitialize the connection,
preventing the client from hanging in an unstable state.

BREAKING CHANGE: env vars now take precedence over config files.


# subject format
<type>(<scope>): <description>

# subject example
feat(database): add retry logic to data fetch

# Types of commits
feat: Introduces a new feature.
fix: Patches a bug.
docs: Documentation-only changes.
style: Changes that do not affect the meaning of the code (white-space, formatting, etc).
refactor: A code change that neither fixes a bug nor adds a feature.
perf: Improves performance.
test: Adds missing tests or corrects existing tests.
chore: Changes to the build process or auxiliary tools and libraries such as documentation generation.

# The seven rules of a great Git commit message
1. Separate subject from body with a blank line
2. Limit the subject line to 50 characters
3. Capitalize the subject line
4. Do not end the subject line with a period
5. Use the imperative mood in the subject line
6. Wrap the body at 72 characters
7. Use the body to explain what and why vs. how
```