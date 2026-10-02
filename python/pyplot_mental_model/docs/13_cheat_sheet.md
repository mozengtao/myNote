# 13. Pyplot Cheat Sheet

## 决策链（每次分析任务，从这里开始）

```text
Question
   ↓
What relationship do I want to see?
   ↓
Choose visualization        （参考 03_chart_selection.md 的决策树）
   ↓
Prepare data                （NumPy/Pandas：load / clean）
   ↓
Compute statistics          （NumPy/Pandas：mean/median/percentile/groupby，参考 06）
   ↓
Create Figure/Axes          （fig, ax = plt.subplots()）
   ↓
Plot                        （ax.plot/bar/hist/scatter/boxplot）
   ↓
Configure                   （set_title/set_xlabel/set_ylabel/legend/grid）
   ↓
Inspect                     （ax.get_xlim()/get_ylim()，检查是否符合预期）
   ↓
Interpret                   （用一句话说清楚这张图回答了什么问题）
   ↓
Save                        （fig.savefig(..., dpi=150, bbox_inches="tight")）
```

## 核心 API 速查

**创建：**
```python
fig, ax = plt.subplots()
```

**趋势（时间序列）：**
```python
ax.plot(x, y, marker="o", label="...")
```

**类别比较：**
```python
ax.bar(categories, values)
```

**分布：**
```python
ax.hist(data, bins=30, edgecolor="black")
ax.boxplot(data, tick_labels=[...])
```

**两变量关系：**
```python
ax.scatter(x, y, alpha=0.6)
```

**统计标记（把统计量叠加到图上）：**
```python
ax.axhline(y_value, linestyle="--", label="...")
ax.axvline(x_value, linestyle="--", label="...")
```

**标签：**
```python
ax.set_title("...")
ax.set_xlabel("...")
ax.set_ylabel("...")
```

**范围：**
```python
ax.set_xlim(a, b)
ax.set_ylim(a, b)
```

**图例：**
```python
ax.legend()
```

**网格：**
```python
ax.grid(True, alpha=0.3)
```

**布局：**
```python
fig.tight_layout()
```

**保存：**
```python
fig.savefig("out.png", dpi=150, bbox_inches="tight")
```

**显示：**
```python
plt.show()
```

## 图形选择速查表

| 我想看... | 用这个 |
|---|---|
| 随时间的变化趋势 | `ax.plot()` |
| 不同类别的数值对比 | `ax.bar()` / `ax.barh()` |
| 一批数据的分布形状 | `ax.hist()` |
| 多组数据的分布对比 | `ax.boxplot()` |
| 两个变量之间的关系 | `ax.scatter()` |
| 各部分占整体的比例 | `ax.pie()` / 堆叠 `bar` |
| 两个单位差异很大的指标随同一 x 变化 | `ax.twinx()`（谨慎使用，见 07 章误读 5） |

## 排查速查表（图不对时）

```python
print(df.shape); print(df.dtypes); print(df.head()); print(df.describe())
print(x.shape, y.shape)
print(ax.get_xlim(), ax.get_ylim())
print(matplotlib.get_backend())
```

排查顺序：`Question -> Data -> Shape -> Statistics -> Plot Mapping -> Axes -> Rendering`（详见 [08_failure_and_debug.md](08_failure_and_debug.md)）。

## 三句话记住整个教程

1. `plt.plot()` 不是"立刻画线"，而是"在 current Axes 上创建/更新一个 Line2D 对象"，真正渲染发生在 `show()`/`savefig()`。
2. 工程代码优先用 `fig, ax = plt.subplots()` + `ax.xxx()` 的 OO 写法，而不是连续的裸 `plt.xxx()`。
3. NumPy/Pandas 负责理解和计算数据，Matplotlib 负责把数据和统计结果编码成人可以观察和理解的视觉结构；图形不是目的，回答问题才是目的。
