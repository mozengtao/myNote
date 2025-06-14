## Commands
```powershell
# 查看 cmdlet 使用说明和示例
Get-Help Get-ChildItem -Full
Get-Help Get-Process -Examples

# 搜索可用命令
Get-Command *Service*

# 查看当前别名
Get-Alias | Where-Object {$_.Definition -eq 'Get-ChildItem'}

Get-ChildItem -Filter *.log -Recurse

# cat, awk
Get-Content file.txt | ForEach-Object { 
    ($_ -split '\s+')[-1] 
}

# 覆写和追加
"Hello World" | Set-Content .\hello.txt
"Another line" | Add-Content .\hello.txt

# pipe and filter
Get-Process | Where-Object {$_.CPU -gt 100} # 旧写法
等价于
Get-Process | Where-Object -FilterScript { $_.CPU -gt 100 }

Get-Process | Where-Object CPU -gt 100 # 新写法
Get-Process | Where-Object Name -like '*chrome*'

Get-Service | Where-Object Name -like 'Win*' -and Status -eq 'Running'

Get-Service | ? { $_.Status -eq 'Running' } # Where-Object 有两个常用别名 ? 和 Where

Get-Service | Out-File services.txt

Get-Service | Select-Object Name, Status
Get-ChildItem | Sort-Object Length -Descending
Get-ChildItem *.log | ForEach-Object { Rename-Item $_ -NewName ($_.Name + ".old") }

# function
function Get-DiskFree {
    Get-PSDrive -PSProvider FileSystem | Select-Object Name, Free
}

# dir
Get-ChildItem C:\Windows -Recurse -Include *.log

# cd
Set-Location "D:\Projects"

# copy
Copy-Item .\Report.docx -Destination \\Server\Share\ -Force

# rm
Remove-Item OldFiles\* -Recurse -Confirm:$false

# move
Move-Item report.docx report_old.docx

#
Get-Process chrome
Get-Service Win*
Get-Content App.log -Tail 50 -Wait

Get-ComputerInfo 

Get-LocalUser | Format-Table Name, Enabled, LastLogon

# Where-Object
Get-Process | Where-Object { $_.CPU -gt 10 }
Get-Process | Where-Object { $_.CPU -gt 10 -and $_.WorkingSet -gt 100MB }
Get-ChildItem | Where-Object { $_.Name -like "*chrome*" }
Get-ChildItem | Where-Object { $_.Length -gt 1MB }

$numbers = 1..10
$numbers | Where-Object { $_ -gt 5 }

Get-Process | Where-Object { $_.CPU -gt 0 } | Sort-Object -Property CPU -Descending | Select-Object -First 5

Get-Process | Where-Object { $_.CPU -gt 10 -and $_.WorkingSet -gt 50MB }
Get-Process | Where-Object { $_.CPU -gt 10 } | Where-Object { $_.Name -like "*app*" }

# 复杂属性计算
Get-Service | Where-Object {
  $_.Status -eq 'Running' -and 
  $_.StartType -eq 'Automatic'
}

# 使用对象方法
Get-ChildItem | Where-Object {
  $_.LastWriteTime.Date -eq (Get-Date).AddDays(-1).Date
}

# OR 逻辑条件
Get-EventLog System | Where-Object {
  ($_.EntryType -eq 'Error') -or 
  ($_.EntryType -eq 'Critical')
}

# 模式匹配
Get-Content log.txt | Where-Object { $_ -match 'ERROR [A-Z]{4}-\d{4}' }

# -Filter
# 文件数量：500,000个
Measure-Command { Get-ChildItem -Recurse -Filter "*.log" }    # 约 2.5秒
Measure-Command { Get-ChildItem -Recurse | Where Extension -eq '.log' } # 约 45秒
# 对于系统提供程序（文件/AD/注册表等），永远优先使用 -Filter；对于内存中的对象集合，使用 Where-Object 处理复杂逻辑
```


[Windows PowerShell commands](https://ss64.com/ps/)  
[PowerShell 101](https://learn.microsoft.com/en-us/powershell/scripting/learn/ps101/00-introduction?view=powershell-7.4)  
[PowerShell](https://github.com/lazywinadmin/PowerShell) #github  
[PowerShell](https://github.com/RamblingCookieMonster/PowerShell)  
-