# 文章处理任务流程文档

## 输入
文章URL：https://cloud.google.com/blog/topics/developers-practitioners/gemini-cli-custom-slash-commands

## 输出
1. 格式化后的原文article.md
2. 中英文双语版article-en-cn.md
3. 中文版article-cn.md
4. 文章中所有的图片资源

## 过程
**每完成一步，都必须更新progress.md**

### 步骤0: 生成笔记
- 仿照例子和当前任务生成笔记 progress.md

### 步骤1: 访问网站
- 访问上文输入中网址
- 必须使用 `lynx -dump -image_links URL` 命令访问网站
- 网站内容保存在raw.txt中

### 步骤2：下载图片
- 从raw.txt中提取文章相关图片链接
- 把图片链接写入 progress.md
- 逐一下载到 resources/ 文件夹
- 每下载完成一个图片，必须更新图片下载进度
- 你必须使用curl命令进行下载

### 步骤3：改写成markdown
- 把raw.txt改写成markdown格式
- 保存在article.md中
- 将article.md中的图片链接指向 resources/ 文件夹

### 步骤4：翻译成中英文
- 把article.md翻译成中英文对照
- 保存在article-en-cn.md中

### 步骤5：翻译成中文
- 提取article-en-cn.md中的中文
- 保存在article-cn.md中

---

## progress.md 笔记格式示例

```markdown
## 任务
[x] 步骤0: 生成笔记
[ ] 步骤1: 访问网站
[ ] 步骤2: 下载图片
[ ] 步骤3: 改写成markdown
[ ] 步骤4: 翻译成中英文
[ ] 步骤5: 翻译成中文

## 图片下载进度
[x] https://xxxx/yyy.png
[ ] https://foo/bar.png

## 当前任务
正在下载 https://foo/bar.png
```

## 目录结构说明
```
项目目录/
├── raw.txt              # 原始网站内容
├── article.md           # 格式化后的原文
├── article-en-cn.md     # 中英文双语版
├── article-cn.md        # 中文版
├── progress.md          # 任务进度跟踪
└── resources/           # 图片资源文件夹
    ├── image1.png
    ├── image2.jpg
    └── ...
```

## 执行顺序说明
1. 首先执行**步骤0**生成初始的progress.md文件
2. 按照步骤1-5的顺序依次执行
3. 每个步骤完成后，在progress.md中标记该步骤为完成 `[x]`
4. 步骤2（下载图片）需要单独跟踪每个图片的下载进度
5. 步骤4和步骤5是翻译流程，需确保翻译质量

## 注意事项
- 图片下载时必须使用相对路径引用
- 中英文翻译应保持专业术语的一致性
- 所有中间文件需妥善保存以便问题排查
- 如遇到网络问题，应有重试机制