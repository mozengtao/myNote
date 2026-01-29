## npm (Node Package Manager)

## npx (Node Package Executor)

- skills
```bash
npx skills --help

Usage: skills <command> [options]

Commands:
  find [query]      Search for skills interactively
  init [name]       Initialize a skill (creates <name>/SKILL.md or ./SKILL.md)
  add <package>     Add a skill package
                    e.g. vercel-labs/agent-skills
                         https://github.com/vercel-labs/agent-skills
  check             Check for available skill updates
  update            Update all skills to latest versions
  generate-lock     Generate lock file from installed skills

Add Options:
  -g, --global           Install skill globally (user-level) instead of project-level
  -a, --agent <agents>   Specify agents to install to
  -s, --skill <skills>   Specify skill names to install (skip selection prompt)
  -l, --list             List available skills in the repository without installing
  -y, --yes              Skip confirmation prompts
  --all                  Install all skills to all agents without any prompts

Options:
  --help, -h        Show this help message
  --version, -v     Show version number
  --dry-run         Preview changes without writing (generate-lock)

Examples:
  $ skills find                     # interactive search
  $ skills find typescript          # search by keyword
  $ skills find "react testing"    # search by phrase
  $ skills init my-skill
  $ skills add vercel-labs/agent-skills
  $ skills add vercel-labs/agent-skills -g
  $ skills add vercel-labs/agent-skills --agent claude-code cursor
  $ skills add vercel-labs/agent-skills --skill pr-review commit
  $ skills check
  $ skills update
  $ skills generate-lock --dry-run

Discover more skills at https://skills.sh/
```

- serve / http-server
```
作用： 瞬间把当前文件夹变成一个本地静态服务器。
效率点： 比如你刚写完一个 HTML/CSS 页面，不用配置任何环境，直接 npx serve 就能在浏览器通过 localhost:3000 访问
```

- json-server
```
作用： 只要一个 db.json 文件，它就能帮你生成一整套 RESTful API。
效率点： 后端接口还没写好？你自己 10 秒钟就能造个假的，带增删改查功能，前端开发不再等待。
```

- tldr
```
作用： 简化版的“命令说明书”。
效率点： 比如你想查 tar 命令怎么压缩文件，传统的 man tar 废话太多。直接 npx tldr tar，它只给你看最常用的 5 个例子，即看即用。
```

- fkill-cli
```
作用： 强行杀掉进程。
效率点： 以前你得查 PID 再 kill。现在 npx fkill 会弹出一个交互式列表，键盘上下选一下，回车，那个卡死的程序就拜拜了。
```

- other commands
```bash
# 1. 格式化所有 C 文件
npx clang-format -i **/*.{c,h,cpp,hpp}
npx clang-format -i --style=Google src/**/*.{c,h}
npx cpplint --exclude=src/third_party src/**/*.c
npx cspell "src/**/*.c" --reporter=txt > spell-errors.txt

# 2. 静态分析
npx cppcheck --enable=all --suppress=missingIncludeSystem src/

# 3. 生成编译数据库
npx bear -- make

# 4. 运行测试并生成覆盖率
npx gcovr --html-details coverage.html

# 5. 生成调用图
npx cflow -b -x src/main.c | npx dot -Tpng -o callgraph.png

# 6. 检查内存泄漏
npx valgrind --leak-check=full ./myprogram

# 7. 性能分析
npx perf record ./myprogram && npx perf report

# 8. 生成火焰图
npx perf record -F 99 -g ./myprogram && npx perf script | npx stackcollapse-perf.pl | npx flamegraph.pl > flame.svg

# 9. 编译为 WebAssembly
npx emcc hello.c -s WASM=1 -o hello.html

# 10. 生成文档
npx doxygen && npx live-server docs/html/
```