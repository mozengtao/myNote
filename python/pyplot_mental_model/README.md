# Pyplot 心智模型教程仓库

> 数据 → 数据整理 → 统计量 → 图形编码 → 可视化 → 观察 → 解释 → 验证

这套仓库不是 `matplotlib.pyplot` 的 API 手册，而是一套可以迁移到日常工作中的 **Mental Model 训练教程**。目标是让你在看到任何数据分析问题时，都能自然地按照下面这条链路思考：

```text
我要回答什么问题
      ↓
数据是什么结构
      ↓
应该计算什么统计量
      ↓
应该用什么图形表达
      ↓
如何验证图形没有误导我
      ↓
如何从图中得到结论
```

## 怎么使用这套仓库

1. 先读 `docs/00_overview.md` ~ `docs/03_chart_selection.md`，建立"pyplot 是什么、状态机 vs OO API、绘图 Lifecycle、图形选择决策树"这四个最基础的心智模型。
2. 对照 `examples/` 目录下的 8 个完整可运行示例，逐个运行、逐个对照 `docs/04_examples_walkthrough.md` 的讲解。
3. 系统过一遍 `docs/05_api_reference.md`，把 pyplot 常用 API 按"创建 / 绘图 / 标注 / 范围 / 图例 / 布局 / 保存"七大职责归类，而不是零散记忆。
4. 学习 `docs/06_statistics_and_viz.md` + `stats_markers/`，建立"统计量 → 可视化标记 → 人类理解"的链路。
5. 学习 `docs/07_misleading_charts.md` + `pitfalls/`，理解图形如何"说谎"，以及如何避免。
6. 遇到"图不对"时，先看 `docs/08_failure_and_debug.md` 的排查表和 Debug Mental Model，而不是直接改绘图代码。
7. 用 `workflows/log_analysis/` 和 `workflows/network_analysis/` 两个完整工程案例，体会 pyplot 在真实系统/网络工程中的价值。
8. 用 `templates/analysis_workflow_template.py` 作为以后新分析任务的起点骨架。
9. 做 `docs/11_exercises.md` 里的 10+ 递进练习，检验自己是否真正建立了心智模型，而不是记住了 API。
10. 最后回顾 `docs/12_mental_model_map.md`（总图）和 `docs/13_cheat_sheet.md`（速查表）。

## 目录导航

| 目录/文件 | 对应主题 |
|---|---|
| [docs/00_overview.md](docs/00_overview.md) | pyplot 是什么：Data → NumPy/Pandas → pyplot → Figure → Axes → Artist → Renderer |
| [docs/01_two_apis.md](docs/01_two_apis.md) | 状态机接口（pyplot）vs 面向对象接口（OO API） |
| [docs/02_lifecycle.md](docs/02_lifecycle.md) | 端到端绘图 Lifecycle（8 步） |
| [docs/03_chart_selection.md](docs/03_chart_selection.md) | "数据 → 图形"的选择决策树 |
| [docs/04_examples_walkthrough.md](docs/04_examples_walkthrough.md) + [examples/](examples/) | 8 个完整可运行示例的逐一讲解 |
| [docs/05_api_reference.md](docs/05_api_reference.md) | 按职责分类的系统 API 讲解 |
| [docs/06_statistics_and_viz.md](docs/06_statistics_and_viz.md) + [stats_markers/](stats_markers/) | 统计量如何变成可视化标记 |
| [docs/07_misleading_charts.md](docs/07_misleading_charts.md) + [pitfalls/](pitfalls/) | 5 类常见图形误读 |
| [docs/08_failure_and_debug.md](docs/08_failure_and_debug.md) | Failure Mode 排查表 + Debug Mental Model |
| [docs/09_workflow_template.md](docs/09_workflow_template.md) + [templates/](templates/) | 可复用的分析工作流模板 |
| [docs/10_state_model_recap.md](docs/10_state_model_recap.md) | pyplot State Mental Model 总结 |
| [docs/11_exercises.md](docs/11_exercises.md) | 10+ 递进练习 |
| [docs/12_mental_model_map.md](docs/12_mental_model_map.md) | 最终 Mental Model Map |
| [docs/13_cheat_sheet.md](docs/13_cheat_sheet.md) | Cheat Sheet |
| [workflows/log_analysis/](workflows/log_analysis/) | 工程案例一：Web 服务日志分析 |
| [workflows/network_analysis/](workflows/network_analysis/) | 工程案例二：网络性能分析 |
| [output/](output/) | 所有脚本运行后生成的 PNG 统一存放于此 |

## 环境约定

- 代码注释使用中文，但**图表内文字**（`title` / `xlabel` / `ylabel` / `legend` 等）统一使用英文——因为当前环境缺少中文字体，中文会在图片里变成方框乱码（参见根目录已有的 [plot_ds_us.py](../plot_ds_us.py) 中的说明）。
- 所有脚本都用 `fig, ax = plt.subplots()` 的面向对象（OO）写法，这是工程代码的推荐范式；只有在专门讲解"状态机接口"时才会展示裸 `plt.xxx()` 写法用于对比。
- 所有脚本都会 `fig.savefig(...)` 把图保存为 PNG，再调用 `plt.show()`。当前环境的 Matplotlib backend 是 `agg`（无 GUI），`plt.show()` 在这种 backend 下不会报错，只会打印一条无害提示。
- 运行任意脚本前，建议先激活/确认已安装 `numpy` `pandas` `matplotlib`：
  ```bash
  python3 -c "import numpy, pandas, matplotlib; print('ok')"
  ```

## 一句话总结

> NumPy/Pandas 负责理解和计算数据，Matplotlib 负责把数据和统计结果编码成人可以观察和理解的视觉结构。
