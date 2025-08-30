
[**vscode online editor**](https://vscode.dev/)
[Documentation](https://code.visualstudio.com/docs)  
[Keyboard shortcuts](https://code.visualstudio.com/docs/configure/keybindings)  
[User and workspace settings](https://code.visualstudio.com/docs/configure/settings)  
[Ten VSCode Settings that I Always Use](https://codefinity.com/blog/Ten-VSCode-Settings-that-I-Always-Use)  
[https://peerlist.io/blog/engineering/vscode-settingsjson-a-complete-guide](https://peerlist.io/blog/engineering/vscode-settingsjson-a-complete-guide)  
[Best VS Code extensions to use, settings to customize, shortcuts you should know, & more](https://www.software.com/src/top-vs-code-extensions-settings-shortcuts-and-more)  
[]()  
[]()  
[]()  

## plugins
C/C++
	The C/C++ extension adds language support for C/C++ to Visual Studio Code, including editing (IntelliSense) and debugging features.
	C/C++: Switch Header/Source
Blockman
	Highlight Nested Code Blocks
Python
	A Visual Studio Code extension with rich support for the Python language
Remote - SSH
	The Remote - SSH extension lets you use any remote machine with a SSH server as your development environment.
C/C++ Extension Pack
	The best VSCode extensions to for C/C++ programmers in one extension pack.
vscode-icons
Bearded Theme
Bearded Icons
Bookmarks
	Mark and Jump
clangd
Prettier - Code formatter
GitLens — Git supercharged
vscode-pdf
Multiple cursor case preserve
Code Runner
TODO Highlight
Todo Tree
Material Icon Theme
Code Spell Checker
Trailing Spaces
highlight



```bash
# settings.json
Bracket Pair Colorization: Enabled

"editor.renderWhitespace": "all",
"editor.cursorBlinking": "expand",
"notebook.stickyScroll.enabled": true,
"editor.cursorSmoothCaretAnimation": "on",

"editor.insertSpaces": false,
"editor.detectIndentation": false,
"editor.tabSize": 4,
"files.exclude": {
	"**/.git": true,
	"**/.svn": true,
}

#
Minimap

# enlarge font size
Ctrl +

# command palette
Ctrl + p
# run command
Ctrl + p
	>
Ctrl + Shift + P
# search symbol in current file
Ctrl + p
	@
Ctrl + Shift + .
# search global symbol
Ctrl + p
	#
# go to line
Ctrl + g
# move word-by-word
Ctrl + <-
Ctrl + ->
# find match and edit
Ctrl + d
# set multiple cursors to edit
Alt + Click
# move line
Alt + 向上箭头
Alt + 向下箭头
# move & copy
Alt + Shift + 向上箭头
Alt + Shift + 向下箭头
# highlight line by line
Ctrl + l
# toggle comments
Ctrl + /
# toggle terminal
Ctrl + `
# toggleStickyScroll
toggleStickyScroll(鼠标滚动时顶上显示函数名称)

# fold and unfold code block
展开所有代码块：Ctrl + K, Ctrl + 0
折叠所有代码块：Ctrl + K, Ctrl + J

# symbol replacement
Ctrl + F2	# 文件内变量替换
```

[User Interface](https://code.visualstudio.com/docs/getstarted/userinterface)  
[一个案例学会 VSCode Snippets，极大提高开发效率](https://zhuanlan.zhihu.com/p/457062272)  
[vscode进阶：运用代码片段提高效率](https://zhuanlan.zhihu.com/p/357377511)  
[VS Code 代码片段完全入门指南](https://www.freecodecamp.org/chinese/news/definitive-guide-to-snippets-visual-studio-code/)  
[**Snippets** in Visual Studio Code](https://code.visualstudio.com/docs/editor/userdefinedsnippets)  
[snippet-generator](https://snippet-generator.app/?description=&tabtrigger=&snippet=&mode=vscode) #online  
```bash
	"Your Snippet Name": {
		"prefix": "yoursnippet",
		"body": [
			"Your snippet code here",
			"function ${1:funcName}(${2:params})",
			"\t$0",
			"}",
		],
		"description": "Your snippet description",
		"scope": "c,c++",
	}

	编辑光标按 $1,$2,$3...$0 的顺序跳转，$0 是光标最后可切换位置
```
[VS Code Extension](https://github.com/topics/vscode-extension)  
[awesome-vscode](https://github.com/viatsko/awesome-vscode)  
[vscode-cpptools extention](https://github.com/microsoft/vscode-cpptools)  
	- [C/C++ for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) (点击**Version History**菜单下进行下载)
	> 如果出现安装了C/C++插件后，仍然出现代码无法跳转的情况，可能的原因是C/C++中的Intelli Sense Engine被关闭了，将其设置为Enable之后应该就正常了，设置方法：File -> Preferences -> Settings，搜索"Intelli Sense Engine"，选择"C Cpp: Intelli Sense Engine(Controls the Intelligence provider)"，选择"**default**". (C++ intellisense插件，在某个版本(至少1.69版本后)已经被弃用了，其功能已经被集成在了C/C++插件中)
[How to Install Visual Studio Code on Debian 10](https://linuxize.com/post/how-to-install-visual-studio-code-on-debian-10/)  
[How to Install Visual Studio Code on Ubuntu 20.04](https://linuxize.com/post/how-to-install-visual-studio-code-on-ubuntu-20-04/)  
[Language Server Extension Guide](https://code.visualstudio.com/api/language-extensions/language-server-extension-guide)  
[Language Server Protocol](https://microsoft.github.io/language-server-protocol/)  
[]()  
[]()  
[]()  