# 05. 系统讲解 pyplot 常用 API（按职责分类）

## Why

零散记忆 `plt.xxx()` 的结果是：换个场景就想不起来该用哪个函数。按"职责"分类之后，你只需要记住 7 个职责分组，遇到新需求先问"我现在要做的事属于哪个分组"，再在分组里找对应函数。

七个职责分组：

```text
1. Figure / Axes 创建与获取   —— 我需要先有一张画布/一个绘图区域
2. Basic Plotting            —— 我需要把数据画成图形
3. Labels / Annotation       —— 我需要给图加文字说明
4. Axis / Range              —— 我需要控制坐标轴的范围/刻度
5. Grid / Legend             —— 我需要辅助线/图例
6. Layout                    —— 我需要解决布局问题（重叠/裁剪）
7. Save                      —— 我需要把图导出成文件
```

下面每一组都按"这组解决什么问题 -> 核心 API -> 9 问要点"讲解。

---

## 1. Figure / Axes：创建 / 获取绘图对象

**解决什么问题**：在画任何东西之前，必须先有一个"画布"（Figure）和至少一个"绘图区域"（Axes）。

| API | 作用 | 位置 | 输入 | 输出/副作用 |
|---|---|---|---|---|
| `plt.figure()` | 创建一个新 Figure（可以不含任何 Axes） | Lifecycle 第 2 步之前 | `figsize`, `dpi` 等 | 返回 `Figure` 对象，并设为 current Figure |
| `plt.subplots(nrows, ncols)` | **最常用**：一次性创建 Figure + 一组 Axes | Lifecycle 第 2 步 | `nrows/ncols/figsize/sharex/sharey` | 返回 `(fig, ax)` 或 `(fig, axes数组)` |
| `plt.subplot(nrows, ncols, index)` | 在 pyplot 状态机风格下，逐个添加子图（旧写法，OO 场景推荐用 `subplots`） | 同上 | 位置参数 | 创建并激活一个 Axes 为 current Axes |
| `plt.gca()` | Get Current Axes：拿到"当前"的 Axes | 任意时刻 | 无 | 返回一个 `Axes` 对象（没有则自动创建） |
| `plt.gcf()` | Get Current Figure：拿到"当前"的 Figure | 任意时刻 | 无 | 返回一个 `Figure` 对象（没有则自动创建） |

**关系**：`plt.subplots()` 内部相当于先 `plt.figure()` 再多次 `add_subplot()`，是二者的便捷封装，也是本教程统一采用的写法。

**实际项目场景**：几乎每个绘图脚本的第一行有效代码都是 `fig, ax = plt.subplots(...)`。

**最容易犯的错误 / 排查**：
- 忘记接收返回值，之后又想用 `ax`——解决办法：始终写全 `fig, ax = plt.subplots()`。
- `plt.subplots(2, 2)` 返回的 `axes` 是二维数组，误用 `axes[0]` 而不是 `axes[0, 0]`——用 `print(axes.shape)` 排查。

---

## 2. Basic Plotting：把数据编码成图形

**解决什么问题**：把已经整理好的数据，转换成人眼可读的视觉元素。所有绘图函数遵循同一个心智模型：

```text
Input Data（x, y 或 单列数值）
      ↓
Visual Encoding（位置/高度/颜色/大小 编码数值大小）
      ↓
Graph（Line2D / Rectangle / PathCollection 等 Artist）
```

| API | Input | Visual Encoding | 典型场景 |
|---|---|---|---|
| `ax.plot(x, y)` | 两个等长的序列 | 用**折线的位置**编码 y 值，x 决定顺序 | 时间序列 |
| `ax.scatter(x, y)` | 两个等长的序列 | 用**点的位置**编码 (x,y)，可选颜色/大小编码第三维 | 两变量关系 |
| `ax.bar(x, height)` | 类别 + 数值 | 用**柱子高度**编码数值 | 类别比较 |
| `ax.barh(y, width)` | 类别 + 数值 | 用**柱子长度**（水平）编码数值，类别名较长时比 `bar` 更易读 | 类别名很长的比较 |
| `ax.hist(data, bins=)` | 单列数值 | 先分桶统计频数，再用**柱子高度**编码频数 | 单变量分布 |
| `ax.boxplot(data)` | 一组或多组数值数组 | 用**箱体位置/高度**编码 Q1/median/Q3，用胡须编码范围 | 分布/多组比较 |
| `ax.errorbar(x, y, yerr=)` | x, y, 误差量 | 在 `plot` 基础上叠加**误差棒**编码不确定性 | 带误差范围的测量值（例如像 [plot_ds_us.py](../../plot_ds_us.py) 中"去掉最大最小值取均值"场景，若想同时展示波动范围就适用） |

**最容易犯的错误**：
- `ax.plot()` 传入的 `x` 和 `y` 长度不一致 -> `ValueError: x and y must have same first dimension`；用 `print(x.shape, y.shape)` 排查。
- 把 `ax.hist()` 的原始样本误当成"已经统计好的频数"传进去（应该传原始数值列表，而不是频数表）。

---

## 3. Labels / Annotation：给图加文字说明

**解决什么问题**：图形本身只有形状/位置，必须靠文字告诉读者"这是什么"。

| API | 作用 | 输入 | 关系 |
|---|---|---|---|
| `ax.set_title(s)` | 设置这个 Axes 的标题 | 字符串 | 修改 `Axes.title`（一个 `Text` Artist） |
| `ax.set_xlabel(s)` / `ax.set_ylabel(s)` | 设置坐标轴含义说明 | 字符串 | 修改对应 `Axis.label` |
| `ax.text(x, y, s)` | 在数据坐标 `(x, y)` 处放一段自由文字 | 坐标 + 字符串 | 新增一个 `Text` Artist |
| `ax.annotate(s, xy=, xytext=)` | 比 `text` 更强大：可以画一个指向具体点的箭头再加文字 | 目标点坐标 + 文字坐标 | 新增 `Annotation`（`Text` 的子类）+ 可选箭头 |

**实际项目场景**：`text`/`annotate` 常用来标出"这个点是异常值""这是全年最高点"等需要精确指向某个数据点的说明；日常的坐标轴/标题说明用 `set_title`/`set_xlabel`/`set_ylabel` 就够了。

**最容易犯的错误**：`ax.text()` 用的是**数据坐标**（和 x/y 轴刻度一个单位系统），如果坐标轴范围变了，文字位置也会跟着"漂移"；如果想要文字固定在图的某个相对位置（比如右上角），要用 `transform=ax.transAxes` 切换成"相对坐标"（0~1）。

---

## 4. Axis / Range：坐标轴范围与刻度

**核心认知**：**数据范围和显示范围不是一回事**。数据本身的最大最小值是固定的，但你可以让坐标轴显示比数据范围更宽或更窄的区间。

| API | 作用 | 常见用途 |
|---|---|---|
| `ax.set_xlim(a, b)` / `ax.set_ylim(a, b)` | 设置坐标轴**显示范围** | 聚焦某个区间；或强制 bar chart 的 y 轴从 0 开始 |
| `ax.set_xticks([...])` / `ax.set_yticks([...])` | 设置刻度**位置** | 只在关心的几个数值处显示刻度 |
| `ax.set_xticklabels([...])` / `ax.set_yticklabels([...])` | 设置刻度处显示的**文字** | 把数值刻度换成可读的类别名（现代写法更推荐 `ax.set_xticks(pos, labels)` 一次设置） |

**最容易犯的错误**：
- 以为 `set_xlim` 会"裁剪数据"——实际上数据没有变化，只是那部分数据没有被显示在当前视窗里。用 `ax.get_xlim()` 排查当前实际显示范围是否符合预期。
- bar chart 默认 `ylim` 通常从 0 开始（Matplotlib 的默认行为），但叠加了 `set_ylim` 后可能被意外改变——如果发现 bar 图"视觉上被夸大了差异"，先检查是不是 `ylim` 没有从 0 开始（详见 [07_misleading_charts.md](07_misleading_charts.md)）。

---

## 5. Grid / Legend：辅助线与图例

| API | 作用 | 数据来源 |
|---|---|---|
| `ax.grid(True, alpha=)` | 在 Axes 上叠加网格线，帮助视线对齐刻度 | 无需额外数据，纯视觉辅助 |
| `ax.legend()` | 生成图例框 | **完全依赖**每个 Artist 创建时传入的 `label=` 参数：`ax.plot(x, y, label="CPU")` -> 之后 `ax.legend()` 才会显示 "CPU" 这一项 |

```text
label="CPU"
   ↓ （创建 Artist 时绑定）
Artist（如 Line2D）内部记录 self._label = "CPU"
   ↓ （legend() 被调用时）
ax.legend() 遍历 ax 上所有 Artist，收集有 label 且不以 "_" 开头的项
   ↓
生成一个 Legend Artist，加入 Axes
```

**最容易犯的错误**：图例为空/缺项，几乎总是因为对应 Artist 没有传 `label=`；双 Y 轴场景（[ex07_dual_yaxis.py](../examples/ex07_dual_yaxis.py)）里 `ax1.legend()` 不会自动包含 `ax2` 的线，需要手动用 `get_legend_handles_labels()` 合并。

---

## 6. Layout：解决重叠与裁剪

**解决什么问题**：默认布局下，标题/坐标轴标签/多个子图之间经常互相重叠或被裁剪到画布外。

| API | 作用 | 使用建议 |
|---|---|---|
| `fig.tight_layout()` | 自动调整 Figure 内所有 Axes 的位置/边距，让标签、标题不重叠、不被裁剪 | **默认首选**，几乎每个示例的最后布局步骤都用它 |
| `fig.subplots_adjust(left=, right=, top=, bottom=, wspace=, hspace=)` | 手动精确控制边距和子图间距 | 当 `tight_layout()` 效果不理想，或需要给 `suptitle`/自定义图例留出精确空间时使用 |

对象关系：

```text
Figure
 ├── Axes（每个 Axes 有自己的位置 bbox）
 ├── title / suptitle
 ├── labels
 └── layout constraints（tight_layout / subplots_adjust 调整的就是这些约束）
```

**实际项目场景**：多子图 dashboard（[ex08_dashboard_web_service.py](../examples/ex08_dashboard_web_service.py)）里用了 `fig.suptitle()` 后，必须搭配 `fig.tight_layout(rect=[0, 0, 1, 0.97])` 手动留出顶部空间，否则大标题会和子图标题重叠。

---

## 7. Save：导出成文件

| API | 作用 |
|---|---|
| `plt.savefig(...)` | 保存 current Figure（pyplot 状态机风格） |
| `fig.savefig(...)` | 保存指定的 `fig` 对象（OO 风格，本教程统一使用） |

```python
fig.savefig(
    "latency.png",
    dpi=150,             # 每英寸像素数，越高图越清晰但文件越大
    bbox_inches="tight",  # 裁掉多余的空白边距，同时避免标签被裁掉一半
)
```

**格式选择**：

| 格式 | 特点 | 什么时候用 |
|---|---|---|
| PNG | 位图，固定分辨率，文件小，通用性最好 | 网页展示、日常报告、Dashboard 截图（本教程默认用这个） |
| SVG | 矢量图，无限缩放不失真，文件通常更小（图形简单时） | 需要嵌入网页并支持缩放、需要用矢量编辑软件（如 Illustrator）二次编辑 |
| PDF | 矢量图，适合印刷 | 论文/正式报告需要嵌入 LaTeX/Word 并保证打印质量 |

**最容易犯的错误**：
- 保存的图不完整（标签被裁掉）——加 `bbox_inches="tight"`。
- 先 `plt.show()` 再 `savefig()`：在某些交互式 backend 下，`show()` 之后 Figure 可能已被关闭，导致 `savefig` 保存出空白图；**规则：先 `savefig()`，再 `show()`**（本教程所有示例都遵循这个顺序）。

---

## 下一步

系统过完 API 分类后，下一步是把"统计计算"和"可视化"两件事清楚分开——Matplotlib 本身不做统计计算，见 [06_statistics_and_viz.md](06_statistics_and_viz.md)。
