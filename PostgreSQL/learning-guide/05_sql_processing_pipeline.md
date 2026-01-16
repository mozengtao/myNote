# Section 5: SQL Processing Pipeline

This section traces how a SQL query transforms from text to results.
Understanding this pipeline is essential for debugging, optimization, and
extending PostgreSQL.

---

## 5.1 Overview: From SQL Text to Execution

```
                         SQL Query
                             |
                             v
+------------------------------------------------------------------+
|                         PARSER                                   |
|  src/backend/parser/                                             |
|  - Lexical analysis (scan.l)                                     |
|  - Syntax analysis (gram.y)                                      |
|  Output: Raw Parse Tree                                          |
+------------------------------------------------------------------+
                             |
                             v
+------------------------------------------------------------------+
|                        ANALYZER                                  |
|  src/backend/parser/analyze.c                                    |
|  - Name resolution (tables, columns)                             |
|  - Type checking and coercion                                    |
|  - Semantic validation                                           |
|  Output: Query Tree                                              |
+------------------------------------------------------------------+
                             |
                             v
+------------------------------------------------------------------+
|                        REWRITER                                  |
|  src/backend/rewrite/                                            |
|  - View expansion                                                |
|  - Rule application                                              |
|  Output: Rewritten Query Tree(s)                                 |
+------------------------------------------------------------------+
                             |
                             v
+------------------------------------------------------------------+
|                        PLANNER                                   |
|  src/backend/optimizer/                                          |
|  - Generate access paths                                         |
|  - Estimate costs                                                |
|  - Choose optimal plan                                           |
|  Output: Plan Tree                                               |
+------------------------------------------------------------------+
                             |
                             v
+------------------------------------------------------------------+
|                        EXECUTOR                                  |
|  src/backend/executor/                                           |
|  - Initialize plan nodes                                         |
|  - Fetch tuples                                                  |
|  - Return results                                                |
|  Output: Query Results                                           |
+------------------------------------------------------------------+

SQL处理流水线概览：

1. PARSER（解析器）
   输入：SQL文本
   输出：原始解析树
   功能：词法分析 + 语法分析
   位置：src/backend/parser/

2. ANALYZER（分析器）
   输入：原始解析树
   输出：查询树
   功能：语义分析、名称解析、类型检查
   位置：src/backend/parser/analyze.c

3. REWRITER（重写器）
   输入：查询树
   输出：重写后的查询树
   功能：视图展开、规则应用
   位置：src/backend/rewrite/

4. PLANNER（计划器）
   输入：重写后的查询树
   输出：执行计划树
   功能：生成候选计划、代价估算、选择最优
   位置：src/backend/optimizer/

5. EXECUTOR（执行器）
   输入：执行计划树
   输出：查询结果
   功能：执行计划、返回元组
   位置：src/backend/executor/
```

---

## 5.2 Stage 1: Parsing

### Lexical Analysis (Scanning)

```
Input: "SELECT name FROM users WHERE id = 1"

Lexer Output (Token Stream):
+--------+--------+--------+--------+--------+--------+--------+--------+
| SELECT | IDENT  | FROM   | IDENT  | WHERE  | IDENT  |   =    | ICONST |
|        | "name" |        |"users" |        |  "id"  |        |   1    |
+--------+--------+--------+--------+--------+--------+--------+--------+

词法分析：

词法分析器(Lexer)将SQL文本分解为标记(Token)流：
- 关键字：SELECT, FROM, WHERE
- 标识符：name, users, id
- 运算符：=
- 常量：1

源文件：src/backend/parser/scan.l (Flex格式)

Flex规则示例：
  {identifier}    { return IDENT; }
  {integer}       { return ICONST; }
  "SELECT"        { return SELECT; }
```

The lexer is defined in `src/backend/parser/scan.l` (Flex format):

```c
/* From scan.l - Example rules */
{identifier}    {
                    /* Identifier token */
                    SET_YYLLOC();
                    yylval->str = pstrdup(yytext);
                    return IDENT;
                }

{integer}       {
                    /* Integer constant */
                    SET_YYLLOC();
                    yylval->ival = atol(yytext);
                    return ICONST;
                }
```

### Syntax Analysis (Parsing)

```
Token Stream
     |
     v
+----+----+
|  gram.y |  Bison grammar
+----+----+
     |
     v
Parse Tree

Example Parse Tree for: SELECT name FROM users WHERE id = 1

                    SelectStmt
                    /    |    \
                   /     |     \
         targetList  fromClause  whereClause
              |          |            |
         ResTarget   RangeVar      A_Expr
              |          |         /  |  \
         ColumnRef   "users"   "="  ColumnRef  A_Const
              |                        |          |
           "name"                    "id"         1

语法分析：

语法分析器(Parser)根据语法规则将标记流构建为解析树。

源文件：src/backend/parser/gram.y (Bison格式)

解析树节点类型（定义在src/include/nodes/parsenodes.h）：
- SelectStmt: SELECT语句节点
- ResTarget: 目标列表项
- RangeVar: 表引用
- ColumnRef: 列引用
- A_Expr: 表达式
- A_Const: 常量

这是"原始"解析树，还未进行语义分析。
此时不知道"users"表是否存在，"name"列是什么类型。
```

The grammar is defined in `src/backend/parser/gram.y`:

```yacc
/* From gram.y - Simplified example */
simple_select:
    SELECT target_list
    FROM from_clause
    WHERE where_clause
    {
        SelectStmt *n = makeNode(SelectStmt);
        n->targetList = $2;
        n->fromClause = $4;
        n->whereClause = $6;
        $$ = (Node *) n;
    }
    ;
```

---

## 5.3 Stage 2: Analysis (Semantic Analysis)

```
Raw Parse Tree
     |
     | (analyze.c)
     v
Query Tree

Transformations:
+------------------------------------------------------------------+
| 1. Name Resolution                                               |
|    - Look up table "users" in system catalogs                    |
|    - Verify column "name" exists in "users"                      |
|    - Verify column "id" exists in "users"                        |
+------------------------------------------------------------------+
| 2. Type Resolution                                               |
|    - Determine "name" is type TEXT                               |
|    - Determine "id" is type INTEGER                              |
|    - Verify constant 1 is compatible with INTEGER                |
+------------------------------------------------------------------+
| 3. Coercion                                                      |
|    - Insert type conversion nodes if needed                      |
|    - Example: comparing int4 with int8 needs coercion            |
+------------------------------------------------------------------+
| 4. Function Resolution                                           |
|    - Resolve operator "=" to appropriate function                |
|    - int4eq for integer equality                                 |
+------------------------------------------------------------------+

语义分析阶段：

1. 名称解析
   - 在系统目录中查找表"users"
   - 验证列"name"存在于表"users"中
   - 验证列"id"存在于表"users"中
   - 如果找不到，报错

2. 类型解析
   - 确定"name"的类型是TEXT
   - 确定"id"的类型是INTEGER
   - 确定常量1与INTEGER兼容

3. 类型强制转换
   - 如果类型不匹配，插入转换节点
   - 例如：int4与int8比较需要转换

4. 函数解析
   - 将运算符"="解析为具体函数
   - int4eq用于整数相等比较

分析后的查询树包含了所有类型信息和目录引用。
```

Key file: `src/backend/parser/analyze.c`

```c
/*
 * parse_analyze() - Main entry point for semantic analysis
 *
 * Transforms a raw parse tree into a Query tree by:
 * 1. Expanding * in SELECT lists
 * 2. Resolving table and column references
 * 3. Resolving function/operator references
 * 4. Adding type coercions as needed
 */
Query *
parse_analyze(RawStmt *parseTree, ...)
{
    ParseState *pstate = make_parsestate(NULL);
    Query *query = transformTopLevelStmt(pstate, parseTree);
    /* ... */
    return query;
}
```

---

## 5.4 Stage 3: Rewriting

```
Query Tree
     |
     | (rewriter)
     v
Rewritten Query Tree(s)

Example: Querying a View

CREATE VIEW active_users AS
    SELECT * FROM users WHERE active = true;

SELECT name FROM active_users WHERE id > 100;

Before Rewrite:
SelectStmt
  targetList: name
  fromClause: active_users   <-- View reference
  whereClause: id > 100

After Rewrite:
SelectStmt
  targetList: name
  fromClause: users          <-- Actual table
  whereClause: (active = true) AND (id > 100)  <-- Merged conditions

重写阶段：

视图展开示例：

假设有视图：
CREATE VIEW active_users AS
    SELECT * FROM users WHERE active = true;

查询：
SELECT name FROM active_users WHERE id > 100;

重写前：
- 从active_users选择
- 条件：id > 100

重写后：
- 从users选择（视图被替换为底层表）
- 条件：(active = true) AND (id > 100)（条件合并）

重写器还处理：
- 规则系统（PostgreSQL特有功能）
- INSTEAD OF触发器
- 安全屏障视图
```

Key file: `src/backend/rewrite/rewriteHandler.c`

The rewriter handles:
1. **View expansion**: Replace view references with their definitions
2. **Rule application**: Apply user-defined rules (ON SELECT, ON INSERT, etc.)
3. **Security barriers**: Handle security-barrier views

---

## 5.5 Stage 4: Planning (Optimization)

This is where PostgreSQL decides HOW to execute the query.

```
Query Tree
     |
     | (planner)
     v
Plan Tree

Planner Steps:
+------------------------------------------------------------------+
| 1. Generate Paths                                                |
|    For each table, consider:                                     |
|    - Sequential scan                                             |
|    - Index scan (for each relevant index)                        |
|    - Bitmap scan                                                 |
|    - TID scan                                                    |
+------------------------------------------------------------------+
| 2. Generate Join Paths                                           |
|    For each pair of relations, consider:                         |
|    - Nested loop join                                            |
|    - Hash join                                                   |
|    - Merge join                                                  |
+------------------------------------------------------------------+
| 3. Estimate Costs                                                |
|    For each path, estimate:                                      |
|    - Startup cost (time to first tuple)                          |
|    - Total cost (time to all tuples)                             |
|    Based on:                                                     |
|    - Table statistics (row counts, distributions)                |
|    - Index statistics                                            |
|    - Configuration parameters                                    |
+------------------------------------------------------------------+
| 4. Choose Cheapest Path                                          |
|    Select the path with lowest total cost                        |
|    (or lowest startup cost for LIMIT queries)                    |
+------------------------------------------------------------------+
| 5. Create Plan Tree                                              |
|    Convert chosen path into executable plan nodes                |
+------------------------------------------------------------------+

计划阶段（优化）：

1. 生成访问路径
   对每个表考虑：
   - 顺序扫描
   - 索引扫描（每个相关索引）
   - 位图扫描
   - TID扫描

2. 生成连接路径
   对每对表考虑：
   - 嵌套循环连接
   - 哈希连接
   - 归并连接

3. 代价估算
   对每个路径估算：
   - 启动代价（获取第一行的时间）
   - 总代价（获取所有行的时间）
   基于：
   - 表统计信息（行数、分布）
   - 索引统计信息
   - 配置参数

4. 选择最优路径
   选择总代价最低的路径
   （LIMIT查询可能选择启动代价最低的）

5. 创建计划树
   将选中的路径转换为可执行的计划节点
```

### Example Plan Tree

```
Query: SELECT * FROM orders o JOIN customers c
       ON o.customer_id = c.id
       WHERE c.country = 'US'

Plan Tree (after optimization):

                    +-------------------+
                    |    Hash Join      |
                    | cost: 150.00      |
                    +--------+----------+
                             |
              +--------------+--------------+
              |                             |
    +---------+---------+         +---------+---------+
    |     Seq Scan      |         |       Hash        |
    |     orders        |         +---------+---------+
    | cost: 50.00       |                   |
    +-------------------+         +---------+---------+
                                  |     Seq Scan      |
                                  |    customers      |
                                  | Filter: country   |
                                  |         = 'US'    |
                                  | cost: 25.00       |
                                  +-------------------+

执行计划示例：

对于一个JOIN查询，计划器可能生成如上的哈希连接计划：

1. 首先扫描customers表，过滤country='US'的行
2. 将结果建立哈希表
3. 扫描orders表
4. 对orders的每一行，在哈希表中查找匹配

代价(cost)是一个相对数值：
- 单位是"磁盘页面顺序读取"
- 用于比较不同计划，不是实际时间
```

Key files:
- `src/backend/optimizer/plan/planner.c` - Entry point
- `src/backend/optimizer/path/allpaths.c` - Path generation
- `src/backend/optimizer/path/costsize.c` - Cost estimation

---

## 5.6 Stage 5: Execution

```
Plan Tree
     |
     | (executor)
     v
Results

Execution Model: Volcano (Iterator) Model

Each plan node implements:
+------------------------------------------------------------------+
| Init()     - One-time initialization                             |
| GetNext()  - Return next tuple (or NULL if done)                 |
| End()      - Cleanup                                             |
+------------------------------------------------------------------+

Execution Flow:
+------------------------------------------------------------------+
|                                                                  |
|  Portal                                                          |
|    |                                                             |
|    v                                                             |
|  ExecutorStart()  -----> Initialize all nodes top-down           |
|    |                                                             |
|    v                                                             |
|  ExecutorRun()    -----> Pull tuples from root node              |
|    |                       |                                     |
|    |                       v                                     |
|    |                     Root node calls GetNext on children     |
|    |                       |                                     |
|    |                       v                                     |
|    |                     Children call GetNext on their children |
|    |                       |                                     |
|    |                       v                                     |
|    |                     Leaf nodes read from storage            |
|    |                                                             |
|    v                                                             |
|  ExecutorEnd()    -----> Cleanup all nodes bottom-up             |
|                                                                  |
+------------------------------------------------------------------+

执行阶段：

火山模型（迭代器模型）：

每个计划节点实现三个接口：
- Init(): 初始化
- GetNext(): 返回下一个元组
- End(): 清理

执行流程：
1. ExecutorStart() - 自顶向下初始化所有节点
2. ExecutorRun() - 从根节点拉取元组
   - 根节点调用子节点的GetNext
   - 子节点递归调用其子节点
   - 叶子节点从存储读取数据
3. ExecutorEnd() - 自底向上清理所有节点

这种"拉取"模型的优点：
- 流水线执行，不需要物化中间结果
- 支持LIMIT（只拉取需要的行数）
- 内存友好（一次只处理一个元组）
```

### Executor Node Examples

```c
/* Simplified node interface */

/* Sequential Scan Node */
TupleTableSlot *
ExecSeqScan(SeqScanState *node)
{
    /* Get next tuple from heap */
    HeapTuple tuple = heap_getnext(node->ss_currentScanDesc, ForwardScanDirection);
    if (tuple == NULL)
        return NULL;

    /* Store in slot and return */
    ExecStoreHeapTuple(tuple, node->ss_ScanTupleSlot);
    return node->ss_ScanTupleSlot;
}

/* Hash Join Node */
TupleTableSlot *
ExecHashJoin(HashJoinState *node)
{
    for (;;)
    {
        /* Get next outer tuple */
        TupleTableSlot *outerSlot = ExecProcNode(outerPlanState(node));
        if (TupIsNull(outerSlot))
            return NULL;

        /* Probe hash table */
        if (ExecHashGetHashValue(...))
        {
            /* Found match - return joined tuple */
            return ExecProject(node->js.ps.ps_ProjInfo);
        }
    }
}

执行器节点示例（简化）：

顺序扫描节点：
1. 从堆获取下一个元组
2. 如果没有更多元组，返回NULL
3. 将元组存入槽位并返回

哈希连接节点：
1. 从外表获取下一个元组
2. 在哈希表中查找匹配
3. 如果找到匹配，返回连接后的元组
4. 重复直到外表扫描完成
```

Key files:
- `src/backend/executor/execMain.c` - Main executor
- `src/backend/executor/nodeSeqscan.c` - Sequential scan
- `src/backend/executor/nodeHashjoin.c` - Hash join
- `src/backend/executor/nodeIndexscan.c` - Index scan

---

## 5.7 Tracing Query Execution

### Using EXPLAIN

```sql
EXPLAIN SELECT * FROM users WHERE id = 1;

                      QUERY PLAN
-------------------------------------------------------
 Index Scan using users_pkey on users  (cost=0.29..8.30 rows=1 width=100)
   Index Cond: (id = 1)

EXPLAIN输出解读：

Index Scan - 使用索引扫描
using users_pkey - 使用主键索引
on users - 扫描users表

cost=0.29..8.30
  - 0.29: 启动代价（获取第一行前的工作）
  - 8.30: 总代价（获取所有行的代价）

rows=1 - 估计返回1行
width=100 - 估计每行100字节
```

### Using EXPLAIN ANALYZE

```sql
EXPLAIN ANALYZE SELECT * FROM users WHERE id = 1;

                      QUERY PLAN
-------------------------------------------------------
 Index Scan using users_pkey on users
   (cost=0.29..8.30 rows=1 width=100)
   (actual time=0.015..0.016 rows=1 loops=1)
   Index Cond: (id = 1)
 Planning Time: 0.080 ms
 Execution Time: 0.032 ms

EXPLAIN ANALYZE输出解读：

actual time=0.015..0.016
  - 0.015ms: 实际获取第一行的时间
  - 0.016ms: 实际获取所有行的时间

rows=1 - 实际返回1行（对比估计的1行）
loops=1 - 该节点执行1次

Planning Time: 计划生成时间
Execution Time: 执行时间

当估计值(rows)与实际值差距大时，
说明统计信息过时，需要ANALYZE。
```

---

## Summary

```
+------------------------------------------------------------------+
|                SQL Processing Pipeline Summary                    |
+------------------------------------------------------------------+
|                                                                  |
| Stage        | Input          | Output         | Key Files       |
| -------------|----------------|----------------|-----------------|
| Parser       | SQL text       | Parse tree     | gram.y, scan.l  |
| Analyzer     | Parse tree     | Query tree     | analyze.c       |
| Rewriter     | Query tree     | Query tree(s)  | rewriteHandler.c|
| Planner      | Query tree     | Plan tree      | planner.c       |
| Executor     | Plan tree      | Results        | execMain.c      |
|                                                                  |
+------------------------------------------------------------------+
|                                                                  |
| Why Each Stage Exists:                                           |
|                                                                  |
| Parser:   Validates syntax, creates structured representation    |
| Analyzer: Validates semantics, resolves names and types          |
| Rewriter: Applies views and rules (abstraction layer)            |
| Planner:  Chooses HOW to execute (optimization)                  |
| Executor: Actually performs the work                             |
|                                                                  |
+------------------------------------------------------------------+

SQL处理流水线总结：

各阶段作用：
- 解析器：验证语法，创建结构化表示
- 分析器：验证语义，解析名称和类型
- 重写器：应用视图和规则（抽象层）
- 计划器：选择如何执行（优化）
- 执行器：实际执行工作

为什么分这么多阶段？
1. 关注点分离 - 每个阶段专注一个任务
2. 可扩展性 - 可以在任何阶段添加功能
3. 可调试性 - 可以查看每个阶段的输出
4. 优化机会 - 重写和计划阶段可以独立优化

下一节我们将深入存储引擎，了解数据如何在磁盘上存储。
```
