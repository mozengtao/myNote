# 04. 八个完整示例逐一讲解

对应代码全部在 [examples/](../examples/) 目录，均可直接 `python3 examples/exXX_xxx.py` 独立运行，运行后会在 `output/` 目录生成对应 PNG。本文档不重复贴代码，只讲每个示例里"为什么这么写"。

## 示例 1：Line Plot —— [ex01_line_plot_timeseries.py](../examples/ex01_line_plot_timeseries.py)

**这张图在回答**："过去 30 天流量是怎么变化的？"

| API/参数 | 9 问要点 |
|---|---|
| `days, traffic` | x/y 数据关系：x 是有序时间序号，y 是随之变化的数值——这正是"时间序列"的定义 |
| `marker="o"` | 在折线上标出每个真实观测点，避免"这是连续采样"的错觉；数据点稀疏时（比如按天）几乎总该加 marker |
| `linewidth` | 纯视觉样式，多条线叠加时可用不同粗细区分主次 |
| `label="Traffic"` | 给这条线起名字，是后面 `ax.legend()` 显示图例文字的**唯一数据来源**——不写 `label` 就不会出现在图例里 |
| `ax.legend()` | 从当前 Axes 收集所有带 `label` 的 Artist，生成图例框；最容易犯的错误是"legend 是空的"——原因几乎总是忘了写 `label=` |
| `ax.grid(True, alpha=0.3)` | `alpha` 控制网格线透明度；网格线太重会抢视觉焦点，太浅又看不清，`0.3` 是常用折中值 |
| `figsize=(10, 5)` | Figure 的物理尺寸（英寸），影响最终图片的长宽比和字体相对大小 |

**核心结论**：Line Plot 的核心任务是观察趋势、变化和时间关系，不是精确对比某两个具体数值。

## 示例 2：Bar Chart —— [ex02_bar_chart_category.py](../examples/ex02_bar_chart_category.py)

**这张图在回答**："哪个服务的请求量最高？"

- `Category -> X, Value -> Y`：`services` 是离散类别，`requests` 是对应的数值。
- 为什么类别数据用 `bar` 而不是 `line`：类别之间没有"顺序"和"连续性"（`auth` 和 `user` 之间不存在"第 1.5 个服务"），用折线会暗示一种不存在的连续过渡关系。
- `ax.grid(axis="y", alpha=0.3)`：只画水平网格线，因为 bar chart 关心的是"高度"的比较，垂直网格线在这里没有意义。
- 常见错误：Y 轴不从 0 开始会夸大柱子间的相对差异（详见 [07_misleading_charts.md](07_misleading_charts.md)）。

## 示例 3：Histogram —— [ex03_histogram_distribution.py](../examples/ex03_histogram_distribution.py)

**这张图在回答**："HTTP 延迟大概集中在什么范围？有没有长尾？"

- 数据流：`Raw Samples -> bins -> frequency -> distribution`。
- `bins=30`：把连续数值切成 30 个等宽区间。`bins` 太少会把多个小峰合并抹平；`bins` 太多会让每个柱子样本量太少，图变得锯齿状充满噪声（对比示例见 [pitfalls/pitfall02_histogram_bins.py](../pitfalls/pitfall02_histogram_bins.py)）。
- `edgecolor="black"`：给每个柱子加边框，方便区分相邻柱子，纯视觉优化。
- 分布的三个关键特征：center（对应 mean/median）、spread（对应 std/IQR）、skew（是否左右对称）。

## 示例 4：Scatter —— [ex04_scatter_relationship.py](../examples/ex04_scatter_relationship.py)

**这张图在回答**："CPU 使用率和延迟之间有没有关系？"

- 变量角色：`x=cpu` 是自变量/解释变量，`y=latency` 是因变量/响应变量，每个点是一次独立观测。
- `alpha=0.6`：让重叠的点变深色，间接反映点的密度，点数多时比不透明散点更容易看出聚集区域。
- 需要观察的四个现象：correlation（整体走向）、outlier（孤立远离点）、cluster（是否分成几团）、trend（线性/非线性）。
- **强调**：这张图只能说明"统计相关"，不能说明"因果关系"——也许是第三个因素（如并发请求数）同时影响了两者。

## 示例 5：Boxplot —— [ex05_boxplot_multigroup.py](../examples/ex05_boxplot_multigroup.py)

**这张图在回答**："三个服务的延迟分布分别是什么样？谁的波动更大？"

- `tick_labels=[...]`：注意 Matplotlib 3.9 起原来的 `labels` 参数改名为 `tick_labels`（3.11 起旧名彻底移除），教程按新写法编写。
- Boxplot 一次性编码 5 个统计量：min（胡须下端）、Q1（箱下边）、median（箱中横线）、Q3（箱上边）、max（胡须上端），箱外散点是 outliers。
- `IQR = Q3 - Q1`：衡量"中间 50% 数据"的分散程度，比 `std` 更不容易被极端值影响。
- Boxplot 是比较**多个分布**的工具，而不仅仅是"画一个箱子"；只看均值（如用 bar）会丢失分布形状和波动信息。

## 示例 6：多子图 —— [ex06_multiple_subplots.py](../examples/ex06_multiple_subplots.py)

**这张图在回答**："requests / latency / errors 三个指标是否同时出现异常？"

- `plt.subplots(3, 1, sharex=True)`：一次性创建 Figure 和 3 个 Axes，`sharex=True` 让三个子图共享同一个 x 轴刻度/缩放，方便上下对齐比较同一天的数据。
- `axes` 是一个长度为 3 的 `numpy.ndarray`（一维，因为 `ncols=1`），用 `axes[0]/axes[1]/axes[2]` 下标显式操作各自的 Axes——这正是 [01_two_apis.md](01_two_apis.md) 里说的"OO API 不依赖当前指针"的价值所在。
- `for ax in axes: ax.grid(...)`：批量对多个 Axes 做同样的配置，OO API 下用一个循环即可完成，pyplot 状态机写法很难做到这么清晰。
- **核心价值**：`plt.subplots()` 是复杂绘图（dashboard、多指标对比）的核心入口。

## 示例 7：双 Y 轴 —— [ex07_dual_yaxis.py](../examples/ex07_dual_yaxis.py)

**这张图在回答**："requests 和 error_rate 是否在同一时间点同时变化？"

- `ax2 = ax1.twinx()`：创建一个与 `ax1` 共享 x 轴、但拥有独立 y 轴的新 Axes。对象关系：`One Figure -> shared X -> {Axes1->Y1, Axes2->Y2}`。
- 图例陷阱：`ax1.legend()` 默认只收集 `ax1` 上的线，`ax2` 的线不会自动出现；必须手动 `get_legend_handles_labels()` 后拼接再传给 `legend()`。
- **为什么容易误导**：两条线各自独立缩放的 y 轴，视觉上的"高低/交叉"完全可能是缩放比例的巧合，不代表两指标真的强相关（完整讨论 + 对比图见 [07_misleading_charts.md](07_misleading_charts.md)）。
- 什么时候该用：确实需要在同一 x 轴上对齐观察两个单位/量级差异很大的指标的**走势**（不是比较绝对大小）时。

## 示例 8：Dashboard —— [ex08_dashboard_web_service.py](../examples/ex08_dashboard_web_service.py)

**这张图在回答**："这个 Web 服务过去 24 小时整体健康吗？"（一张图回答 5 个子问题）

- 用 `pandas` 生成模拟日志字段（`timestamp/service/status_code/latency_ms/request_size`），体现"NumPy/Pandas 做计算，Matplotlib 做呈现"的分工。
- `matplotlib.gridspec.GridSpec(3, 2)` + `fig.add_subplot(gs[2, :])`：实现"前两行 2x2、最后一行合并成一整行"的不规则布局——比 `plt.subplots(3,2)` 更灵活，能精确控制每个 Axes 占据的网格范围。
- `df.groupby("hour")`：统计计算完全交给 Pandas；Matplotlib 只接收算好的 `requests_per_hour` / `error_rate_per_hour` 序列来画图。
- `ax_latency_dist.axvline(p50/p95/p99, ...)`：把统计量叠加到分布图上，这是 [06_statistics_and_viz.md](06_statistics_and_viz.md) 的核心手法，只看均值会掩盖长尾问题。
- `fig.suptitle(...)` + `fig.tight_layout(rect=[0,0,1,0.97])`：`suptitle` 是整个 Figure 级别的大标题（区别于每个 Axes 自己的 `set_title`），加了 `suptitle` 后要给 `tight_layout` 的 `rect` 参数留出顶部空间，否则大标题会和子图标题重叠。

**Dashboard 阅读顺序建议**：先看流量整体走势 -> 对照同期错误率是否升高 -> 看延迟分布的 P50/P95/P99 定位"多数用户体验"与"最差用户体验"的差距 -> 定位是哪个服务拖慢了整体 -> 验证请求体大小与延迟的关联假设。

## 下一步

看完 8 个完整示例后，回过头系统梳理一遍 pyplot 常用 API，按职责分类而不是零散记忆，见 [05_api_reference.md](05_api_reference.md)。
