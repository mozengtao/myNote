# ============================================================
# 插件：report
#
# 同样遵守 run {params} 约定。这个插件演示插件内部可以自由使用
# 任意 Tcl 概念（这里用了 foreach + expr）来完成自己的业务逻辑，
# 主程序完全不需要关心插件内部是怎么实现的。
# ============================================================
namespace eval ::plugins::report {
    namespace export run
}

proc ::plugins::report::run {params} {
    set title [dict get $params title]
    set rows  [dict get $params rows]

    set total 0
    foreach r $rows {
        incr total $r
    }

    return [format "%s: %d rows, sum=%d" $title [llength $rows] $total]
}
