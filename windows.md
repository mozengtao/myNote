
## 应用开机自启动
```bash
# 开机自启动 wsl

Win + r -> shell:startup -> 打开目录 C:\Users\morrism\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup

创建文件 start-wsl.vbs:
Set ws = CreateObject("Wscript.Shell")
ws.run """C:\Program Files\WindowsApps\CanonicalGroupLimited.Ubuntu_2404.1.68.0_x64__79rhkp1fndgsc\ubuntu.exe""", 1, false

# PowerShell 下 "ubuntu.exe" 的具体路径可以通过如下命令进行查询
Get-ChildItem -Path "C:\Program Files\WindowsApps" -Recurse -Filter "ubuntu.exe" -ErrorAction SilentlyContinue

# WBScript 已经被废弃，对于其他任务建议优先优先使用 PowerShell 进行处理
```

[Windows VBScript commands](https://ss64.com/vb/)  

[Mega Collection of PowerShell Scripts](https://github.com/fleschutz/PowerShell/tree/main) #github  


- winget
	- windows下的命令行安装应用程序工具
- [Shell Commands for Windows 10: Navigating Common Folders in Windows](https://www.mirazon.com/shell-commands-for-windows-10-navigating-common-folders-in-windows/)
- 添加开机自启动应用
	```
	1.Win + R: shell:startup
	2.添加应用的快捷方式到打开的文件夹中
	```
- 快捷键
	```
	Win + x			// 快速打开windows诊断工具
	Win + Tab		// 在一个屏幕下显示和切换所有打开的窗口
	Alt + Tab		// 切换窗口
	Ctrl+Shift+F	// 中文输入法 简体和繁体切换
	```
