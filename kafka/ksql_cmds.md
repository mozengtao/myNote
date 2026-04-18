# 1. 基础与连接命令
```ksql
SHOW VERSIONS;
LIST PROPERTIES;
EXIT;
QUIT;
CLEAR;
```

# 2. 查看流、表、主题、查询
```ksql
SHOW TABLES;
SHOW STREAMS;
SHOW TOPICS;
SHOW QUERIES;
SHOW FUNCTIONS;
```

# 3. 查看表/流结构
```ksql
DESCRIBE 表名或流名;
DESCRIBE EXTENDED 表名或流名;
```

# 4. 简单查询（避免刷屏）
```ksql
SELECT * FROM 表名 LIMIT 10;
SELECT 字段1,字段2 FROM 表名 LIMIT 10;
```

# 5. 统计总行数
```ksql
SELECT COUNT(*) AS total FROM 表名 EMIT CHANGES LIMIT 1;
```

# 6. 条件查询
```ksql
SELECT * FROM 表名 WHERE 字段='值' LIMIT 5;
```

# 7. 实时推送查询
```ksql
SELECT * FROM 表名 EMIT CHANGES;
SELECT COUNT(*) FROM 表名 EMIT CHANGES;
```

# 8. 创建表示例
```ksql
CREATE TABLE 表名 (
    ID VARCHAR PRIMARY KEY
) WITH (
    KAFKA_TOPIC='topic名',
    VALUE_FORMAT='JSON'
);
```

# 9. 删除表/流
```ksql
DROP TABLE 表名 DELETE TOPIC;
DROP STREAM 流名 DELETE TOPIC;
```

# 10. 运维管理
```ksql
TERMINATE 查询ID;
SET 'auto.offset.reset' = 'earliest';
```

# 11. 输出格式设置
```ksql
SET 'ksql.output.format' = 'tabular';
SET 'ksql.output.format' = 'json';
```