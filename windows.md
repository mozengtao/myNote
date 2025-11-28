## WSL
[Basic commands for WSL](https://learn.microsoft.com/en-us/windows/wsl/basic-commands)  
[]()  
[]()  
[]()  

- WSL访问Windows下的文件夹
```bash
# 通过/mnt/目录直接访问 （WSL会自动将Windows的驱动器挂载到/mnt/目录下）
cd /mnt/c/Users/$USER/Downloads

# 创建符号链接
ln -s /mnt/c/Users/$USER/Downloads ~/Downloads
cd ~/Downloads

# WSL 下打开 Windows 文件管理器
explorer.exe .
explorer.exe /mnt/c/Users/$USER/Desktop/

# Windows 下访问WSL文件
# 在Windows文件资源管理器地址栏输入：
\\wsl$\Ubuntu\home\用户名

# cmds
wsl --list --online
wsl --install	# install default distribution Ubuntu
wsl --install --distribution <distribution_name>
# wsl --install -d <distribution_name>

wsl --list --verbose
# wsl -l -v

# run a distribution  on the terminal
wsl -d <distribution_name>

wsl --terminate <running_distribution_name>
wsl --shutdown	# shut down the entire WSL system

wsl --version

wsl --status

wsl --set-version <distribution name> <versionNumber>
# wsl --set-version Ubuntu 2

# backup your installed distribution along with all the data
wsl --export <distribution_name> <file_name.tar>

wsl --import <Distribution Name> <Import location> <exported file>

wsl --unregister <distribution_name>

wsl --update


```


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
