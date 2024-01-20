- 帮助文档
	- [AutoHotkey Document](https://wyagd001.github.io/zh-cn/docs/AutoHotkey.htm)
-
- 语法
	- 热字串语法
		- \:\:热子串\:\:扩展后子串
		  background-color:: red
		  collapsed:: true
			- 举例 1
			  ```ahk
			  ::,pc::电脑
			  
			  :O:,pc::电脑		;不输出触发子元
			  ```
			- 举例 2
			  ```ahk
			  ::,addr::
			  (
			  山东省
			  青岛市
			  市南区
			  )
			  ```
		- 热子串选项
		  collapsed:: true
			- |*选项*|*功能*|*范例*|
			  |*|不需要触发字元|:*:,qd::青岛|
			  |O|不输出触发字元|:O:,pc::电脑|
			  |C|区分大小写|:C:,pc::电脑|
			  |R|原样输出|:*R:,dear::各位同仁{enter}|
			  |B0|扩展后不删除热子串|:*B0:<li>::</li>{left 5}|
			- 特定应用程序下的热子串
			  collapsed:: true
				- 举例
				  collapsed:: true
					- ```ahk
					  #IfWinActive ahk_calss Notepad++
					  ::,t1::输出到Notepad++
					  #IfWinActive
					  ::,t1::输出到其他应用程序uto
					  ```
				- ((63703ea6-45e9-4c2a-8644-8dd314ba077a))
	-
	- 热键语法
	  collapsed:: true
		- 按键\:\: 执行的动作
		  background-color:: red
		  collapsed:: true
			- 举例
			  collapsed:: true
				- `Windows + n` 打开notepad++
				  collapsed:: true
					- ```ahk
					  #n::run notepad++		;#=Windows
					  ```
	-
	- 指引 Directive 的作用
	  collapsed:: true
		- 设置脚本档案的执行环境
		- 指引命令
		  collapsed:: true
			- |*指引*|功能说明|
			  |\#SingleInstance Fore|强制只使用一个执行实例，是否覆盖的对话窗不再出现|
			  |\#IfWinActive ahk_class 应用程序类别|指定应用程序是否拥有输入焦点|
			  |\#NoEnv|初始环境设定，建议使用以取得更佳效果|
			-
-
- 开机自启动设置
	- ![1668302239517.png](../assets/1668302239517_1668302249513_0.png)
	- 通过鼠标右键添加ahk文件的快捷键至启动文件夹
-
- autogui
	- 下载链接
		- https://sourceforge.net/projects/autogui/
	- AutoHotkey 脚本的测试工具，被测试脚本的后缀民为 .ahk
	- 测试语法
		- 单行
		  collapsed:: true
			- ```ahk
			  ; test area
			  ```
		- 多行
		  collapsed:: true
			- ```ahk
			  /*
			  test area
			  */
			  ```
	- 如何获取应用程序的类别名称
	  id:: 63703ea6-45e9-4c2a-8644-8dd314ba077a
		- 通过 AutoGUI 应用中的 Tools -> Configure Tools ... -> New -> 选择 AutoHotkey 安装目录下的 WindowSpy.ahk 的文件路径如 C:\Program Files\AutoHotkey\WindowSpy.ahk -> 点击 OK 后会生成 “Display Name:” 指定的工具名称
		- 之后运行Tools -> WindowSpy，就会显示当前应用程序的类别名称
-