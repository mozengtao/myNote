#!/usr/bin/env tclsh
# ============================================================
# 场景 2：键值配置文件管理器（INI-like config）
#
# 运行方式：
#   tclsh config_manager.tcl
#
# 对应 tcl_mental_model.md 的章节：
#   - 13-15 Dict / Nested Dict
#   - 16-17 Array（用于对比，说明为什么这里选 Dict）
#   - 23  split
#   - 26  regexp
#   - 27  switch
#   - 34  proc（含默认参数）
#   - 38  namespace（模块级状态用 variable 管理）
# ============================================================

namespace eval config {
    # 模块级状态：整份配置，结构是 Nested Dict
    #   data
    #     +-- server   -> dict(host=..., port=...)
    #     +-- database -> dict(host=..., port=..., user=...)
    variable data [dict create]

    namespace export load save get put sectionExists dump
}

# ------------------------------------------------------------
# 解析单行配置文本，返回一个描述该行类型的 Dict：
#   注释/空行   -> {type blank}
#   [section]  -> {type section name xxx}
#   key=value  -> {type kv key xxx value xxx}
# ------------------------------------------------------------
proc config::parseLine {line} {
    set line [string trim $line]

    if {$line eq "" || [string index $line 0] eq "#"} {
        return [dict create type blank]
    }
    if {[regexp {^\[(.+)\]$} $line -> section]} {
        return [dict create type section name $section]
    }
    if {[regexp {^([^=]+)=(.*)$} $line -> key value]} {
        return [dict create type kv key [string trim $key] value [string trim $value]]
    }
    return [dict create type unknown]
}

# ------------------------------------------------------------
# 从磁盘文件加载配置，覆盖 variable data
# ------------------------------------------------------------
proc config::load {path} {
    variable data
    set data [dict create]
    set currentSection ""

    set fp [open $path r]
    while {[gets $fp line] >= 0} {
        set parsed [parseLine $line]
        switch -- [dict get $parsed type] {
            section {
                set currentSection [dict get $parsed name]
                if {![dict exists $data $currentSection]} {
                    dict set data $currentSection [dict create]
                }
            }
            kv {
                if {$currentSection eq ""} {
                    error "config error: key=value 出现在任何 \[section\] 之前 -> $line"
                }
                dict set data $currentSection \
                    [dict get $parsed key] [dict get $parsed value]
            }
            default {
                # blank / comment / unknown：忽略
            }
        }
    }
    close $fp
    return
}

# ------------------------------------------------------------
# 读取配置项，支持默认值（proc 的默认参数）
# ------------------------------------------------------------
proc config::get {section key {default ""}} {
    variable data
    if {[dict exists $data $section $key]} {
        return [dict get $data $section $key]
    }
    return $default
}

# ------------------------------------------------------------
# 写入/修改配置项
#
# 注意：这里特意不叫 config::set —— 如果叫 set，由于本 proc
# 是在 ::config 命名空间下定义的，命名空间内其它 proc（比如
# config::load 里的 `set data ...`、`set currentSection ...`）
# 在解析不带 :: 前缀的命令时，会优先在当前命名空间（::config）
# 里找到这个自定义的 `set`，而不是内置的 set 命令，从而整个模块
# 全部坏掉。这是 namespace 实践中一个容易踩的坑，所以改名为 put。
# ------------------------------------------------------------
proc config::put {section key value} {
    variable data
    dict set data $section $key $value
    return
}

proc config::sectionExists {section} {
    variable data
    return [dict exists $data $section]
}

# ------------------------------------------------------------
# 把当前配置重新序列化成文本
# ------------------------------------------------------------
proc config::dump {} {
    variable data
    set text ""
    dict for {section kv} $data {
        append text "\[$section\]\n"
        dict for {key value} $kv {
            append text "$key=$value\n"
        }
        append text "\n"
    }
    return $text
}

proc config::save {path} {
    set fp [open $path w]
    puts -nonewline $fp [dump]
    close $fp
    return
}

# ============================================================
# 主流程：写一份示例配置 -> 加载 -> 读取 -> 修改 -> 另存
# ============================================================

set sampleConfigText {# 示例配置文件
[server]
host=127.0.0.1
port=8080

[database]
host=db.local
port=5432
user=admin
}

set srcPath [file join [file dirname [info script]] app.conf]
set dstPath [file join [file dirname [info script]] app.saved.conf]

set fp [open $srcPath w]
puts -nonewline $fp $sampleConfigText
close $fp

config::load $srcPath

puts "server.host = [config::get server host]"
puts "server.port = [config::get server port]"
puts "database.user = [config::get database user]"
puts "cache.ttl（不存在，走默认值） = [config::get cache ttl 300]"

# 修改一个已有配置项，并新增一个新的 section
config::put server port 9090
config::put cache enabled true

config::save $dstPath

puts "\n===== 修改后另存的配置内容 ($dstPath) ====="
puts -nonewline [config::dump]
