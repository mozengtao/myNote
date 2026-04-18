# AWK "Buffer-and-Judge" 模式

### 1. 模式核心逻辑
* **Buffer (暂存)**：使用变量存储当前行，因为当前行是否需要输出取决于后续行。
* **Judge (判定)**：读取新行时，检查是否满足特定条件（标志位、ID切换、结束符）。
* **Flush (清空)**：一旦判定成功，处理并输出 Buffer 内容，随后重置变量。

---

### 2. 典型示例

#### 示例 A：跨行合并（处理分行符/结束符）
**场景**：SQL 语句被拆分成多行，只有遇到分号 `;` 才算一条完整指令。

* **原始数据 (`input.txt`)**：
    ```text
    SELECT name FROM users
    WHERE status = 'active';
    DELETE FROM logs
    WHERE date < '2025-01-01';
    ```
* **实现代码**：
    ```awk
    {
        # Buffer: 持续累加行内容
        line_buf = (line_buf == "" ? $0 : line_buf " " $0)

        # Judge: 检查当前行是否以分号结尾
        if ($0 ~ /;$/) {
            print "[SQL Query]: " line_buf
            line_buf = "" # Flush: 清空缓冲
        }
    }
    ```
* **对应输出**：
    ```text
    [SQL Query]: SELECT name FROM users WHERE status = 'active';
    [SQL Query]: DELETE FROM logs WHERE date < '2025-01-01';
    ```

---

#### 示例 B：状态关联（捕获异常前的上下文）
**场景**：在日志中寻找导致 `CRASH` 的前一行指令。

* **原始数据 (`input.txt`)**：
    ```text
    OP: init_system
    OP: load_config
    CRASH: config_not_found
    OP: start_service
    CRASH: port_conflict
    ```
* **实现代码**：
    ```awk
    /^OP:/ { 
        # Buffer: 存下当前操作，等待看它是否会导致崩溃
        last_op = $0 
        next 
    }
    /^CRASH:/ {
        # Judge: 看到 CRASH，证明 buffer 里的上一个操作是因
        if (last_op != "") {
            print "Faulty Operation: [" last_op "] | Reason: [" $0 "]"
            last_op = "" # Flush
        }
    }
    ```
* **对应输出**：
    ```text
    Faulty Operation: [OP: load_config] | Reason: [CRASH: config_not_found]
    Faulty Operation: [OP: start_service] | Reason: [CRASH: port_conflict]
    ```

---

#### 示例 C：分组聚合（处理连续同 ID 数据）
**场景**：将属于同一个 ID 的分散属性（Name, Age, Tool）合并到一行显示。

* **原始数据 (`input.txt`)**：
    ```text
    ID_001,Name:Alice
    ID_001,Age:28
    ID_002,Name:Bob
    ID_002,Tool:Neovim
    ID_002,OS:Linux
    ```
* **实现代码**：
    ```awk
    BEGIN { FS="," }
    {
        # Judge: 如果当前 ID 与缓存 ID 不同，说明上一组结束了
        if (curr_id != "" && $1 != curr_id) {
            print "User " curr_id " Profile -> " attr_buf
            attr_buf = ""
        }
        curr_id = $1
        # Buffer: 累加属性字段
        attr_buf = (attr_buf == "" ? $2 : attr_buf " | " $2)
    }
    END { 
        # 重要：处理最后留在缓冲区的数据
        if (curr_id != "") print "User " curr_id " Profile -> " attr_buf 
    }
    ```
* **对应输出**：
    ```text
    User ID_001 Profile -> Name:Alice | Age:28
    User ID_002 Profile -> Name:Bob | Tool:Neovim | OS:Linux
    ```

---

#### 示例 D：层级块提取（处理嵌套 {} 结构）
**场景**：从具有嵌套层级的 HCL 或配置文件中，完整提取特定名称的配置块（如 check { ... }）

* **原始数据 (`input.txt`)**：
    ```text
    service {
      name = "ksqldb"
      check {
        name = "ksqldb http check"
        protocol = "http"
        check_restart {
          limit = 5
          grace = "90s"
        }
      }
    }
    ```
* **实现代码**：
    ```awk
    # 1. Trigger: 匹配到块起始标记
    /check \{/ {
        found = 1
        buffer = ""
        brace_count = 0
    }

    # 2. Buffer: 在 found 状态下，持续将行存入缓冲区并计算嵌套深度
    found {
        buffer = (buffer == "" ? $0 : buffer "\n" $0)

        # 使用 gsub 返回值动态计算当前行的大括号增量
        brace_count += gsub(/\{/, "{")
        brace_count -= gsub(/\}/, "}")

        # 3. Judge: 当大括号完全闭合（计数器归零），判定为块结束
        if (brace_count == 0) {
            print buffer
            found = 0  # 重置状态，准备匹配下一个块
        }
    }
    ```
* **对应输出**：
    ```text
    check {
        name = "ksqldb http check"
        protocol = "http"
        check_restart {
          limit = 5
          grace = "90s"
        }
      }
    ```

---

#### 示例 E：精确块内修改（结合重定向实现原文件覆盖）
**场景**：匹配特定配置块，仅对该块内的指定配置项（如 `option`）进行注释处理，并保持文件其他部分不变。

* **原始数据 (`dhcpd6.conf`)**：
    ```text
    host COOL_EN3400_ERM324_10 {
        hardware ethernet 00:18:48:05:8c:6f;
        option docsis.ccap-cores 2001:64:100::101;
    }
    host OTHER_HOST {
        option some-other-option;
    }
    ```
* **实现代码**：
    ```bash
    # 在多个 option 行下添加新行
    awk '
    # 1. Trigger: 进入目标 host 块
    /host COOL_EN3400_ERM324_10/ { found = 1 }

    # 2. Judge & Modify & Insert: 仅在 found 状态下且匹配到 option 时进行修改
    # 条件：处于 target 块中 + 包含 option + 且该行目前没有被注释 (!/^ *#/)
    found && /option/ && !/^ *#/ {
        # a. 提取当前行的缩进 (空格或制表符)
        match($0, /^[ \t]*/)
        indent = substr($0, RSTART, RLENGTH)

        # b. 注释掉当前行
        sub(/option/, "# option")

        # c. 构造新行：缩进 + 新配置
        new_line = indent "option new-config-item added-value;"

        # d. 立即打印修改后的当前行，并紧跟新行
        print $0
        print new_line

        # 跳过最后的全局 print，防止重复打印当前行
        next
    }

    # 3. Reset: 遇到该块结束的右大括号，关闭状态
    found && /\}/ { found = 0 }

    # 4. Global Action: 打印所有行（包括修改后和未触发修改的行）
    { print }
    ' dhcpd6.conf > dhcpd6.conf.tmp && mv dhcpd6.conf.tmp dhcpd6.conf

    # 只在整个 host 块结束前插入一行新配置
    awk '
    # 1. Trigger: 进入目标 host 块
    /host COOL_EN3400_ERM324_10/ { found = 1 }

    # 2. Modify: 处于块内时，注释所有未注释的 option
    found && /option/ && !/^ *#/ {
        # 捕获当前行的缩进，留作最后一行使用
        if (match($0, /^[ \t]*/)) {
            indent = substr($0, RSTART, RLENGTH)
        }
        sub(/option/, "# option")
    }

    # 3. Judge & Insert: 遇到右大括号时，先打印新行，再打印右大括号
    found && /\}/ {
        # 如果 indent 为空（可能块内没 option），可以设置默认缩进
        if (indent == "") indent = "    "

        print indent "option new-config-at-the-end value;"
        found = 0
    }

    # 4. Global Action: 打印所有行
    { print }
    ' dhcpd6.conf > dhcpd6.conf.tmp && mv dhcpd6.conf.tmp dhcpd6.conf
    ```
* **对应输出**（文件内容已变更）：
    ```text
    host COOL_EN3400_ERM324_10 {
        hardware ethernet 00:18:48:05:8c:6f;
        # option docsis.ccap-cores 2001:64:100::101;
    }
    host OTHER_HOST {
        option some-other-option;
    }
    ```

---

### 3. 使用建议
1.  **内存安全**：处理超大型文件且没有明确结束标志时，Buffer 可能会消耗过多内存。
2.  **遗留处理**：始终记得检查是否需要在 `END` 块中处理最后一份未刷新的 Buffer。
3.  **重置时机**：确保在每次 `print` 之后将 Buffer 变量置为空字符串 `""`。