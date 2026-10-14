# ============================================================
# 插件：backup
#
# 每个插件只需要遵守一个约定：
#   在自己的 namespace 下提供一个 run {params} 过程，
#   接收一个 Dict 作为参数，返回一个描述执行结果的字符串
#   （失败时用 error 抛出）。
#
# 插件内部的变量、辅助 proc 都关在自己的 namespace 里，
# 不会和其他插件、主程序发生命名冲突。
# ============================================================
namespace eval ::plugins::backup {
    namespace export run
}

proc ::plugins::backup::run {params} {
    set source [dict get $params source]
    set target [dict get $params target]

    # 真实场景这里会是 file copy / 数据库 dump / rsync 等，
    # 本例只做字符串模拟，重点在"插件如何被主程序统一调度"。
    return "backed up '$source' -> '$target'"
}
