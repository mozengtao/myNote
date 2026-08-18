# 可复用 FSM 引擎

本目录的代码从第 08 课的表驱动实现抽出，第 13 课再扩展为分层状态机。

**不要在第 01–07 课使用本引擎。** 那些课必须亲手写出 `if` / `switch` / `dispatch`，才能理解引擎在抽象什么。

| 文件 | 对应课 | 能力 |
|------|--------|------|
| `c/table_fsm.h` `c/table_fsm.c` | 08 | 扁平表驱动 |
| `cpp/table_fsm.hpp` | 08 | 同上 |
| `python/table_fsm.py` | 08 | 同上 |
| `c/hsm.h` `c/hsm.c` | 13 | 复合状态、entry/exit、history |
| `cpp/hsm.hpp` | 13 | 同上 |
| `python/hsm.py` | 13 | 同上 |

## HSM 骨架（第 13 课）

`hsm.h` / `hsm.c` / `hsm.hpp` / `hsm.py` 是学完 13a–13f 之后可带走的最小分层机：

| 字段 / 函数 | 对应子课 | 作用 |
|-------------|----------|------|
| `parent` | 13a | 复合状态：叶子永远是 `current`，父节点用来挂一条公共转移 |
| `entry` / `exit` | 13b | 进入、退出动作；外部转移按 LCA 先退出后进入 |
| `hsm_internal` | 13c | 内部转移：只跑动作，不退出、不重进 |
| `history` 槽 + `hsm_shallow_history` / `hsm_deep_history` | 13d | 浅历史记直接子状态，深历史沿槽走到叶子 |

**不要在 `lessons/13-hsm/` 里 `#include` / `import` 本目录。** 各子课的 `player_hsm` 必须自包含、能单独编译运行。引擎是对照物，不是依赖。

正交区域（13e）和伪状态（13f）没有做成通用调度器：教学上用两个「当前子状态」字段，以及显式的 choice / fork / join 分支即可。若你要在自己的程序里扩展本骨架，先保持「一个对象、一个 `current` 叶子」，再按 13e 加第二根指针。

各课 `code/` 里的主例是自包含的教学程序。引擎是「学完后可带走的骨架」，不是前面各课的依赖。
