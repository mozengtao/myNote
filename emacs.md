- [A Guided Tour of Emacs](https://www.gnu.org/software/emacs/tour/)
- [Documentation](https://www.gnu.org/software/emacs/documentation.html)
- [GNU Emacs Reference Card](https://www.gnu.org/software/emacs/refcards/pdf/refcard.pdf)
- [emacsCoreCommands](https://gist.github.com/k2052/4089827)


```emacs
/************************************/
基本命令
C-x C-f      打开/新建文件
C-x C-s      保存当前缓冲区
C-x C-w      当前缓冲区另存为
C-x C-v      关闭当前Buffer并打开新文件
C-x i        光标处插入文件
C-x b        切换Buffer
C-x C-b      显示Buffer列表
C-x k        关闭当前Buffer
C-x C-c      关闭Emacs

/************************************/
窗口命令
C-x 2       水平分割窗格
C-x 3       垂直分割窗格
C-x 0       关闭当前窗口
C-x o       切换窗口
C-x 1       关闭其他窗口
C-x 5 2     新建窗口
C-x 5 f     新窗口中打开文件

/************************************/
光标移动命令
C-f         前进一个字符
C-b         后退一个字符
C-p         上一行
C-n         下一行
M-f         前进一个单词
M-b         后退一个单词
C-a         行首
C-e         行尾
C-v         下翻一页
M-v         上翻一页
M-<         文件头
M->         文件尾

/**********************************/
编辑命令
C-Space         设置开始标记
C-@             设置开始标记(C-space可能被操作系统拦截)
M-w             复制标记区内容
C-y             帖粘
M-u             使光标处的单词大写
M-l             使光标处的单词小写
M-c             使光标处单词首字母大写
C-k             删除一行

/**********************************/
搜索/替换命令
C-s          向下搜索
C-r          向上搜索
M-%          替换
-              space/y     替换当前匹配
-              Del/n          不要替换当前匹配
-              .                    仅替换当前匹配并退出
-              ,                    替换并暂停(按space或y继续)
-              !                    替换所有匹配
-              ^                   回到上一个匹配位置
-              return/q    退出替换
```