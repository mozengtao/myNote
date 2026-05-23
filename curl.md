# curl 系统化学习指南

## 🧠 学习路径概览

这是一个完整的 curl 学习体系，从心智模型到实战应用，帮您系统化掌握 curl 的本质与工程实践。

### 📚 学习文档结构

1. **[01-curl-mental-model.md](./curl/01-curl-mental-model.md)** - 心智模型构建
   - 从实际命令入手的语义拆解
   - curl 数据流转路径图解
   - curl = 远程函数调用抽象
   - 典型使用模式分类
   - 故障排查模式

2. **[02-deep-principles.md](./curl/02-deep-principles.md)** - 深入原理机制
   - HTTP 与 TCP 的关系
   - Connection reuse & Keep-Alive
   - 超时与重试机制设计
   - 高级特性与生产调优
   - libcurl 内部机制剖析

3. **[03-practical-exercises.md](./curl/03-practical-exercises.md)** - 实战练习
   - 练习1：GET 请求与参数处理
   - 练习2：POST API 与认证流程
   - 练习3：故障排查与调试技能
   - 综合实战项目

4. **[04-mental-model-summary.md](./curl/04-mental-model-summary.md)** - 心智模型总结
   - 统一心智模型
   - 五个最常用命令模板
   - 关键概念映射表
   - 实战决策树
   - 进阶学习建议

### 🎯 学习目标

完成这个学习体系后，您将能够：

✅ **curl = 网络调试瑞士军刀**
- 替代 Postman 进行 API 测试
- 调试生产环境网络问题
- 理解系统间的 HTTP 通信行为
- 构建自动化监控和检测脚本

✅ **建立系统化思维**
- 掌握 curl → HTTP → TCP → 网络栈 的完整路径
- 理解 curl 在分布式系统中的关键作用
- 形成分层调试和故障定位的思维模式

### 🚀 快速开始

推荐按顺序阅读，每个文档都有丰富的实例和 ASCII 图表，确保理论与实践结合。

---

## Tips

- 用 curl 访问 tldr.sh 的在线 API
```bash
curl cheat.sh/tar

npx tldr tar
```