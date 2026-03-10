# Spec-Driven Development with Cursor

## 一、什么是 Spec-Driven Development

### 定义

Spec-Driven Development（规格驱动开发）是一种以 **结构化规格文档** 为核心驱动力的开发范式。开发者先编写一份精确的、机器可读的规格书（Spec），然后将其作为 AI 编程助手的唯一输入来源，让 AI 严格按照规格书生成代码。

它的核心理念可以用一句话概括：

> **先写 Spec，再写代码。Spec 是需求，也是验收标准。**

### 与传统开发方式的对比

| 维度 | 传统开发 | Prompt-Driven（对话式） | Spec-Driven |
|------|---------|----------------------|-------------|
| 需求载体 | PRD / 口头沟通 | 聊天消息 | 结构化 Markdown 文件 |
| 上下文一致性 | 依赖人的记忆 | 随对话漂移 | 始终锚定同一份文件 |
| 可复现性 | 低 | 极低 | 高——同一份 Spec 可反复生成 |
| 变更管理 | 改 PRD → 改代码 | 再说一遍 → 希望 AI 记住 | 改 Spec → 重新生成 |
| 多人协作 | 看文档/开会 | 各自聊各自的 | 共享同一份 Spec |

### 为什么它特别适合 AI 编程

1. **减少歧义**：AI 对自然语言理解有随机性，结构化 Spec 把模糊空间降到最低。
2. **上下文可控**：Spec 文件可以用 `@filename` 精确注入上下文，而不是依赖 AI 的记忆。
3. **分步执行**：Spec 天然拆分了任务步骤，AI 可以逐步完成，每步都有明确的验收标准。
4. **版本可追溯**：Spec 是纯文本文件，天然支持 Git 版本管理。

---

## 二、Spec 文件的写作框架

一份好的 Spec 应包含以下部分：

```markdown
# Spec: [组件/功能名称]

## 1. 功能目标
一句话说明做什么、用什么技术栈。

## 2. 状态逻辑
列出所有状态变量、类型和默认值。

## 3. 核心行为
用「动作 → 结果」的格式描述每个交互。

## 4. UI 规范
配色、字体、布局、动画等视觉细节。

## 5. 技术约束
框架选择、代码组织方式、依赖库等硬性要求。
```

### 写作原则

| 原则 | 说明 | 示例 |
|------|------|------|
| **具体化** | 避免"好看的界面"，给出 HEX 色值 | `背景色 #F0F4F8` |
| **类型化** | 用 TypeScript 类型语法描述状态 | `status: 'idle' \| 'running' \| 'paused'` |
| **行为化** | 用 "当...则..." 格式描述交互 | `点击"暂停"：计时停止，背景音暂停` |
| **约束化** | 明确架构要求 | `逻辑提取到 useTimer 自定义 Hook 中` |
| **可枚举** | 选项用列表穷举，不留"等等" | `提供 5min, 10min, 20min 三档快选按钮` |

---

## 三、完整实战示例：冥想计时器

### 第 1 步：编写 Spec 文件

创建 `meditation_spec.md`：

```markdown
# Spec: 冥想计时器组件

## 1. 功能目标
创建一个 React (Tailwind CSS) 冥想计时器，支持自定义时长和白噪音背景。

## 2. 状态逻辑
- **initialTime**: 默认 10 分钟 (600秒)。
- **timeLeft**: 当前剩余秒数。
- **status**: 'idle' | 'running' | 'paused' | 'finished'。
- **audio**: 播放状态 (boolean)。

## 3. 核心行为
- 点击"开始"：倒计时开始，播放背景音。
- 点击"暂停"：计时停止，背景音暂停。
- 倒计时结束：状态转为 'finished'，触发震动提示（模拟），停止音频。
- 提供 5min, 10min, 20min 三档快选按钮。

## 4. UI 规范
- 进度条：使用半圆环形进度条展示剩余时间。
- 配色：背景色 #F0F4F8，主色调（按钮）使用深蓝。
- 字体：使用系统默认无衬线字体。

## 5. 技术约束
- 使用 React + Lucide React 图标库。
- 逻辑提取到 `useTimer` 自定义 Hook 中。
```

### 第 2 步：在 Cursor 中发出提示词

打开 Cursor Chat（Agent 模式），输入：

```
请阅读 @meditation_spec.md
1. 根据 Spec 中的逻辑，首先创建 hooks/useTimer.ts 处理状态控制。
2. 然后创建 components/MeditationTimer.tsx。
3. 确保 UI 完全符合规格书中的配色和功能要求。
4. 使用 Tailwind CSS 进行样式开发。
```

### 第 3 步：AI 生成的代码

#### `hooks/useTimer.ts`

AI 严格按照 Spec 第 2 节定义了四个状态变量，按第 3 节实现了所有行为：

```typescript
export type TimerStatus = 'idle' | 'running' | 'paused' | 'finished';

export function useTimer(defaultDuration = 600): UseTimerReturn {
  const [initialTime, setInitialTime] = useState(defaultDuration);
  const [timeLeft, setTimeLeft] = useState(defaultDuration);
  const [status, setStatus] = useState<TimerStatus>('idle');
  const [audioPlaying, setAudioPlaying] = useState(false);
  // ...start, pause, resume, reset, setDuration
}
```

Spec 规定 → 代码对应：

| Spec 条目 | 代码实现 |
|-----------|---------|
| `status: 'idle' \| 'running' \| 'paused' \| 'finished'` | `TimerStatus` 联合类型 |
| `initialTime: 默认 10 分钟 (600秒)` | `defaultDuration = 600` |
| `倒计时结束 → 触发震动提示` | `navigator.vibrate([200, 100, 200])` |
| `逻辑提取到 useTimer 自定义 Hook 中` | 独立文件 `hooks/useTimer.ts` |

#### `components/MeditationTimer.tsx`

AI 按照 Spec 第 4 节的 UI 规范实现：

```typescript
// 背景色 #F0F4F8
<div className="bg-[#F0F4F8]">
  // 深蓝主色调
  <button className="bg-[#1E3A5F] text-white">
  // 系统默认无衬线字体
  <div className="font-sans">
  // 半圆环形进度条 — SVG 弧线实现
  <SemiCircleProgress progress={progress} />
```

### 第 4 步：验收

对照 Spec 逐项检查：

- [x] 状态逻辑：4 个状态变量全部实现
- [x] 开始/暂停行为完整
- [x] 倒计时结束触发震动 + 停止音频
- [x] 5/10/20 三档快选
- [x] 半圆环形进度条
- [x] 背景色 #F0F4F8、深蓝按钮
- [x] 系统无衬线字体
- [x] Lucide React 图标
- [x] 逻辑在 useTimer Hook 中

---

## 四、Cursor 提示词模板库

### 模板 1：从零创建（脚手架 + 实现）

```
请阅读 @xxx_spec.md
1. 初始化项目：使用 Vite + React + TypeScript + Tailwind CSS。
2. 根据 Spec 中的状态逻辑，创建 hooks/useXxx.ts。
3. 根据 Spec 中的 UI 规范，创建 components/XxxComponent.tsx。
4. 确保配色、字体、布局完全符合 Spec 第 4 节。
5. 使用 Tailwind CSS 进行样式开发。
```

### 模板 2：在已有项目中添加功能

```
请阅读 @feature_spec.md
- 在现有项目 @src/ 的基础上，按 Spec 添加新功能。
- 复用已有的 @src/hooks/useAuth.ts 认证逻辑。
- 新组件放在 src/components/Feature/ 目录下。
- 保持与现有代码风格一致。
```

### 模板 3：重构已有代码

```
请阅读 @refactor_spec.md 和当前实现 @src/components/OldComponent.tsx
- 按 Spec 中的新架构重构该组件。
- 将内联逻辑提取到自定义 Hook 中。
- 保留所有现有功能，不引入破坏性变更。
- 完成后运行 TypeScript 类型检查确认无错误。
```

### 模板 4：分步迭代

```
请阅读 @xxx_spec.md
本次只完成 Spec 中的「第 2 节：状态逻辑」和「第 3 节：核心行为」。
UI 部分暂时使用最简单的按钮和文字，我们下一轮再做 UI。
```

### 模板 5：验收 + 修复

```
请对照 @xxx_spec.md 检查 @src/components/Xxx.tsx 和 @src/hooks/useXxx.ts：
- 列出 Spec 中所有要求，逐项标注是否已实现。
- 对未实现的项，给出修复方案并执行。
```

---

## 五、进阶技巧

### 1. Spec 分层策略

对于复杂项目，将 Spec 拆分为多个文件：

```
specs/
├── architecture_spec.md    # 整体架构、目录结构、技术选型
├── auth_spec.md            # 认证模块
├── dashboard_spec.md       # 仪表盘模块
└── api_spec.md             # API 接口定义
```

提示词中按需引用：

```
请阅读 @specs/architecture_spec.md 了解项目整体结构，
然后按 @specs/auth_spec.md 实现认证模块。
```

### 2. 用 Spec 管理状态机

对于有复杂状态的功能，在 Spec 中画状态转移图：

```markdown
## 状态转移
idle --[点击开始]--> running
running --[点击暂停]--> paused
running --[倒计时归零]--> finished
paused --[点击继续]--> running
paused --[点击重置]--> idle
finished --[点击重置]--> idle
```

这种格式 AI 能精确理解，生成的代码不会遗漏状态转移分支。

### 3. 在 Spec 中内联类型定义

直接在 Spec 里写 TypeScript 接口，消除类型歧义：

```markdown
## 数据模型
​```typescript
interface MeditationSession {
  id: string;
  duration: number;       // 秒
  completedAt: Date;
  mood: 'calm' | 'focused' | 'anxious';
}
​```
```

### 4. Spec 驱动的迭代循环

```
编写 Spec v1 → AI 生成代码 → 验收 → 修改 Spec v2 → AI 重新生成 → ...
```

每次迭代只改 Spec 文件，AI 会基于更新后的 Spec 调整代码。Spec 文件用 Git 管理，变更历史清晰可追溯。

### 5. 组合 Cursor Rules 与 Spec

在 `.cursor/rules/` 中设置项目级规则（代码风格、命名规范），Spec 只描述功能需求。两者配合：

- **Cursor Rules**：控制 "怎么写"（风格、惯例）
- **Spec 文件**：控制 "写什么"（功能、行为）

---

## 六、Spec 质量检查清单

写完 Spec 后，用以下问题自检：

- [ ] 每个状态变量都有类型和默认值吗？
- [ ] 每个用户操作都有明确的「输入 → 输出」描述吗？
- [ ] 颜色值是否用 HEX/RGB 而非模糊词语（如"深蓝"需附带色值）？
- [ ] 是否指定了文件路径和代码组织方式？
- [ ] 是否列出了所有第三方依赖？
- [ ] 是否有边界情况的描述？（倒计时归零、网络断开等）
- [ ] 另一个开发者（或 AI）仅凭此 Spec 能否独立实现？

---

## 七、总结

Spec-Driven Development 的本质是把 **人类的思考前置到 Spec 编写阶段**，让 AI 专注于它最擅长的事：**根据精确描述生成高质量代码**。

```
思考质量（Spec） × 执行能力（AI） = 最终代码质量
```

Spec 写得越精确，AI 生成的代码就越接近你的预期。这不是多写了一步，而是把原本分散在聊天中的反复修正，集中到了一份可维护的文档里。
