# 07. 图形误读：图表也会"说谎"

## Why

图表的说服力很强，但正因为如此，一个选择不当的参数（bins 数量、y 轴范围、双 Y 轴缩放）就可能在读者脑子里制造出并不存在的结论。学会识别这些陷阱，和学会画图本身同样重要。

对应的 5 个可运行对比脚本都在 [pitfalls/](../pitfalls/) 目录。

## 误读 1：Y 轴 baseline —— [pitfall01_bar_baseline.py](../pitfalls/pitfall01_bar_baseline.py)

同一份延迟数据（5 个服务，实际差距仅 4ms/~4%），左图 `ylim` 从 0 开始，右图 `ylim` 从 95 开始：

| | Baseline=0（诚实） | Baseline!=0（误导） |
|---|---|---|
| 视觉印象 | 5 根柱子几乎一样高 | order 明显"高很多" |
| 真实差距 | 4ms（~4%） | 同样是 4ms |

**规则**：`ax.bar()` 靠"柱子高度"传递数值大小信息，一旦 baseline（`ylim` 下限）不是 0，柱子高度比例就不再等于数值比例。**排查方法**：`ax.get_ylim()`，检查下限是不是 0（或有没有充分的理由不是 0）。

## 误读 2：Histogram bins 数量 —— [pitfall02_histogram_bins.py](../pitfalls/pitfall02_histogram_bins.py)

同一份"双峰分布"数据（背后其实是两个不同群体），`bins=5/20/100` 三联对比：

- `bins=5`：区间太宽，两个峰被合并抹平，完全看不出双峰结构。
- `bins=20`：能看出明显的双峰形状——这份数据比较合适的选择。
- `bins=100`：区间太窄，图变得锯齿状，噪声掩盖了整体形状。

**规则**：没有"绝对正确"的 bins 数量，但要清楚"bins 本身会改变你看到的分布形状"，遇到关键结论时应该尝试多个 bins 值交叉验证。

## 误读 3：Outlier 对 mean 的影响 —— [pitfall03_outlier_mean_vs_median.py](../pitfalls/pitfall03_outlier_mean_vs_median.py)

200 个正常样本（mean≈100.1, median≈100.0），加入 3 个极端 outlier（2000/1800/2200ms）后：

```text
加入前: mean=100.1, median=100.0
加入后: mean=128.2, median=100.1
mean 变化: +28.1
median 变化: +0.1
```

**规则**：mean 对极端值全局敏感（哪怕极端值只占 1.5% 的样本量），median 对极端值高度稳健。**工程启示**：只看 mean 类监控指标，一次偶发严重超时就可能让指标"看起来"整体变差，需要同时看 median 和 P95/P99（见 [06_statistics_and_viz.md](06_statistics_and_viz.md)）。

## 误读 4：Correlation ≠ Causation —— [pitfall04_correlation_not_causation.py](../pitfalls/pitfall04_correlation_not_causation.py)

构造场景：真实驱动因素是"并发请求数(concurrency)"，它同时推高了 `cpu_usage` 和 `latency`。左图只看 `cpu_usage vs latency`（corr=0.92，看起来"cpu 导致了 latency"）；右图用颜色编码 `concurrency`，暴露出两者可能是被同一个第三方因素共同驱动的。

**规则**：scatter + 相关系数只能证明"两个变量一起变化的程度"，永远不能单独证明"谁导致了谁"。要验证因果关系需要受控实验（A/B test）或因果推断方法。

## 误读 5：双 Y 轴的视觉误导 —— [pitfall05_dual_axis_misleading.py](../pitfalls/pitfall05_dual_axis_misleading.py)

`series_a`（稳定上升趋势）和 `series_b`（纯随机游走，真实相关系数 ≈ -0.01，几乎无关）：

- 左图（双 Y 轴）：两个 y 轴各自独立缩放，`series_b` 被拉伸得看起来"也在往某个方向走"，容易让人误以为两者相关。
- 右图（标准化后单 Y 轴）：把两组数据都做 z-score 标准化后放到同一个 y 轴，可以看到 `series_b` 其实只是在 0 附近随机波动，和 `series_a` 的上升趋势毫无关系。

**规则**：任何时候看到双 Y 轴图表"两条线走势惊人一致"，先检查两个 y 轴各自的刻度范围（`ax1.get_ylim()`/`ax2.get_ylim()`），确认这种"一致"不是缩放选择造成的巧合。更稳妥的替代方案：标准化后放到同一 y 轴，或拆成独立子图（[ex06_multiple_subplots.py](../examples/ex06_multiple_subplots.py)）。

## 汇总排查清单

| 看到这种图 | 先检查 |
|---|---|
| Bar chart 差异看起来很戏剧化 | `ax.get_ylim()` 下限是不是 0 |
| Histogram 形状很"干净"或很"锯齿" | 换几个 `bins` 值看形状是否稳定 |
| 某个统计指标突然大幅变化 | 同时看 median/percentile，排查是否是极端值导致 |
| Scatter plot 相关系数很高就下因果结论 | 想一下有没有可能的"共同原因"变量 |
| 双 Y 轴图表两条线"惊人一致" | 检查两个 y 轴各自的缩放范围 |

## 下一步

除了图形本身可能误导人，pyplot 代码本身也有一批常见的"跑不出预期结果"的失败模式，见 [08_failure_and_debug.md](08_failure_and_debug.md)。
