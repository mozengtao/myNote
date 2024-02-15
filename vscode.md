- 文件内变量替换
	- Ctrl + F2
- 插件
	- **C Snippets**
	- **Markdown Preview Mermaid Support** (github.dev 可以work)
	- Increment Selection (Ctrl + Alt + i)
	- Remote - SSH
	```bash
		# 选择ssh的配置文件
		Remote - SSH: Open SSH Configuration File ...
		# 之后在配置文件里面添加需要自动登陆的配置
		Host lxd2
        HostName 192.168.233.35
        User morrism
        #PreferredAuthentications publickey
        IdentityFile ~/.ssh/id_rsa
	```
- 参考文档
- [User Interface](https://code.visualstudio.com/docs/getstarted/userinterface)
- [一个案例学会 VSCode Snippets，极大提高开发效率](https://zhuanlan.zhihu.com/p/457062272)
- [vscode进阶：运用代码片段提高效率](https://zhuanlan.zhihu.com/p/357377511)
- [VS Code 代码片段完全入门指南](https://www.freecodecamp.org/chinese/news/definitive-guide-to-snippets-visual-studio-code/)
- [**Snippets** in Visual Studio Code](https://code.visualstudio.com/docs/editor/userdefinedsnippets)
- [snippet-generator](https://snippet-generator.app/?description=&tabtrigger=&snippet=&mode=vscode) #online
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
- [VS Code Extension](https://github.com/topics/vscode-extension)
- [awesome-vscode](https://github.com/viatsko/awesome-vscode)
- [vscode-cpptools extention](https://github.com/microsoft/vscode-cpptools)
	- [C/C++ for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) (点击**Version History**菜单下进行下载)
	> 如果出现安装了C/C++插件后，仍然出现代码无法跳转的情况，可能的原因是C/C++中的Intelli Sense Engine被关闭了，将其设置为Enable之后应该就正常了，设置方法：File -> Preferences -> Settings，搜索"Intelli Sense Engine"，选择"C Cpp: Intelli Sense Engine(Controls the Intelligence provider)"，选择"**default**". (C++ intellisense插件，在某个版本(至少1.69版本后)已经被弃用了，其功能已经被集成在了C/C++插件中)
- [How to Install Visual Studio Code on Debian 10](https://linuxize.com/post/how-to-install-visual-studio-code-on-debian-10/)
- [How to Install Visual Studio Code on Ubuntu 20.04](https://linuxize.com/post/how-to-install-visual-studio-code-on-ubuntu-20-04/)