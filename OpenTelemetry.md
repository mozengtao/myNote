[OpenTelemetry](https://opentelemetry.io/)  
[OpenTelemetry Github](https://github.com/open-telemetry)  
[]()  
[]()  
[OpenTelemetry 架构与核心概念](OpenTelemetry_Arch.md)  
[OpenTelemetry入门](https://www.modb.pro/db/1712285347122601984)  
[OpenTelemetry and Dynatrace](https://docs.dynatrace.com/docs/ingest-from/opentelemetry)  
[]()  

## 可观测性三大信号（Telemetry Data）
- Traces: Distributed traces
    - 业务场景
        用户调用 GET /hello/otel → 接口处理 → 调用 “用户信息查询” 子函数 → 查询 MySQL 数据库 → 返回结果
    - Trace 链路结构图
    ```
    TraceID: 0123456789abcdef0123456789abcdef
    └── Span: /hello/:name (HTTP 请求入口) [SpanID: 1111111111111111]
        ├── Span: handle_hello_request (业务逻辑处理) [SpanID: 2222222222222222, ParentSpanID: 1111111111111111]
        │   └── Span: query_user_info (查询用户信息) [SpanID: 3333333333333333, ParentSpanID: 2222222222222222]
        │       └── Span: mysql_query (数据库查询) [SpanID: 4444444444444444, ParentSpanID: 3333333333333333]
    ```
    - 完整 Trace 示例（OTel 标准 JSON 格式）
    ```json
    {
    "resourceSpans": [
        {
        "resource": {
            "attributes": [
            {"key": "service.name", "value": {"stringValue": "go-gin-demo"}},
            {"key": "service.version", "value": {"stringValue": "1.0.0"}},
            {"key": "deployment.environment", "value": {"stringValue": "dev"}}
            ]
        },
        "scopeSpans": [
            {
            "scope": {"name": "gin-demo-app"},
            "spans": [
                // Span 1: HTTP 请求入口（根 Span）
                {
                "traceId": "0123456789abcdef0123456789abcdef",
                "spanId": "1111111111111111",
                "parentSpanId": "", // 根 Span 无父 ID
                "name": "/hello/:name",
                "kind": "SPAN_KIND_SERVER", // 服务端 Span
                "startTimeUnixNano": "1710888000000000000", // 开始时间（纳秒）
                "endTimeUnixNano": "1710888000250000000",   // 结束时间（纳秒），耗时 250ms
                "attributes": [
                    {"key": "http.method", "value": {"stringValue": "GET"}},
                    {"key": "http.path", "value": {"stringValue": "/hello/:name"}},
                    {"key": "http.client_ip", "value": {"stringValue": "127.0.0.1"}},
                    {"key": "http.status_code", "value": {"intValue": "200"}}
                ],
                "status": {"code": "STATUS_CODE_OK"}
                },
                // Span 2: 业务逻辑处理（子 Span）
                {
                "traceId": "0123456789abcdef0123456789abcdef",
                "spanId": "2222222222222222",
                "parentSpanId": "1111111111111111",
                "name": "handle_hello_request",
                "kind": "SPAN_KIND_INTERNAL", // 内部操作 Span
                "startTimeUnixNano": "1710888000010000000",
                "endTimeUnixNano": "1710888000240000000", // 耗时 230ms
                "attributes": [
                    {"key": "user.name", "value": {"stringValue": "otel"}}
                ],
                "status": {"code": "STATUS_CODE_OK"}
                },
                // Span 3: 查询用户信息（子 Span）
                {
                "traceId": "0123456789abcdef0123456789abcdef",
                "spanId": "3333333333333333",
                "parentSpanId": "2222222222222222",
                "name": "query_user_info",
                "kind": "SPAN_KIND_INTERNAL",
                "startTimeUnixNano": "1710888000050000000",
                "endTimeUnixNano": "1710888000200000000", // 耗时 150ms
                "attributes": [
                    {"key": "user.id", "value": {"stringValue": "user_123"}}
                ],
                "status": {"code": "STATUS_CODE_OK"}
                },
                // Span 4: 数据库查询（子 Span）
                {
                "traceId": "0123456789abcdef0123456789abcdef",
                "spanId": "4444444444444444",
                "parentSpanId": "3333333333333333",
                "name": "mysql_query",
                "kind": "SPAN_KIND_CLIENT", // 客户端 Span（调用数据库）
                "startTimeUnixNano": "1710888000100000000",
                "endTimeUnixNano": "1710888000180000000", // 耗时 80ms
                "attributes": [
                    {"key": "db.system", "value": {"stringValue": "mysql"}},
                    {"key": "db.statement", "value": {"stringValue": "SELECT * FROM users WHERE name = 'otel'"}},
                    {"key": "db.connection_string", "value": {"stringValue": "localhost:3306/db?sslmode=disable"}}
                ],
                "status": {"code": "STATUS_CODE_OK"}
                }
            ]
            }
        ]
        }
    ]
    }
    ```
- Metrics: Measurements over time
    - 业务场景
        Go Gin 示例中 /hello/:name 接口的「请求数计数器」和「接口耗时直方图」
    - OTel 标准 Metric 原始数据（JSON 格式）
    ```json
    {
    "resourceMetrics": [
        {
        "resource": {
            "attributes": [
            {"key": "service.name", "value": {"stringValue": "go-gin-demo"}},
            {"key": "deployment.environment", "value": {"stringValue": "dev"}}
            ]
        },
        "scopeMetrics": [
            {
            "scope": {"name": "gin-demo-app"},
            "metrics": [
                // 示例1：Counter（请求数计数器）
                {
                "name": "demo.http.request.count",
                "description": "Number of HTTP requests processed by Gin demo",
                "unit": "{request}",
                "dataType": "SUM",
                "sum": {
                    "dataPoints": [
                    {
                        "attributes": [
                        {"key": "http.method", "value": {"stringValue": "GET"}},
                        {"key": "http.path", "value": {"stringValue": "/hello/:name"}}
                        ],
                        "startTimeUnixNano": "1710888000000000000", // 统计起始时间
                        "timeUnixNano": "1710888060000000000",       // 统计结束时间（60秒窗口）
                        "value": {"intValue": "120"},               // 累计值：60秒内调用120次
                        "flags": "0",
                        "exemplars": [] // 可选：关联TraceID，用于定位异常样本
                    }
                    ],
                    "aggregationTemporality": "CUMULATIVE", // 累计型（单调递增）
                    "isMonotonic": true                      // 计数器：值不递减
                }
                },
                // 示例2：Histogram（接口耗时直方图）
                {
                "name": "demo.http.request.duration",
                "description": "Duration of HTTP requests in milliseconds",
                "unit": "ms",
                "dataType": "HISTOGRAM",
                "histogram": {
                    "dataPoints": [
                    {
                        "attributes": [
                        {"key": "http.method", "value": {"stringValue": "GET"}},
                        {"key": "http.path", "value": {"stringValue": "/hello/:name"}}
                        ],
                        "startTimeUnixNano": "1710888000000000000",
                        "timeUnixNano": "1710888060000000000",
                        "count": "120",                          // 样本总数
                        "sum": {"doubleValue": "15600.0"},       // 总耗时：15600ms → 平均130ms/次
                        "bucketCounts": ["0", "80", "30", "10", "0"], // 桶计数：
                        "explicitBounds": ["100.0", "200.0", "300.0", "400.0"], // 桶边界：
                        // 桶含义：<100ms(80次)、100-200ms(30次)、200-300ms(10次)、>300ms(0次)
                        "exemplars": [
                        { // 示例：关联慢请求的TraceID，便于排查
                            "filteredAttributes": [{"key": "trace_id", "value": {"stringValue": "0123456789abcdef0123456789abcdef"}}],
                            "timeUnixNano": "1710888050000000000",
                            "value": {"doubleValue": "250.0"}
                        }
                        ]
                    }
                    ],
                    "aggregationTemporality": "CUMULATIVE"
                }
                }
            ]
            }
        ]
        }
    ]
    }
    ```
    - Prometheus 格式（可视化 / 存储常用）
    ```
    # HELP demo_http_request_count Number of HTTP requests processed by Gin demo
    # TYPE demo_http_request_count counter
    demo_http_request_count{deployment_environment="dev",http_method="GET",http_path="/hello/:name",service_name="go-gin-demo"} 120 1710888060000

    # HELP demo_http_request_duration Duration of HTTP requests in milliseconds
    # TYPE demo_http_request_duration histogram
    demo_http_request_duration_bucket{deployment_environment="dev",http_method="GET",http_path="/hello/:name",service_name="go-gin-demo",le="100.0"} 80 1710888060000
    demo_http_request_duration_bucket{deployment_environment="dev",http_method="GET",http_path="/hello/:name",service_name="go-gin-demo",le="200.0"} 110 1710888060000
    demo_http_request_duration_bucket{deployment_environment="dev",http_method="GET",http_path="/hello/:name",service_name="go-gin-demo",le="300.0"} 120 1710888060000
    demo_http_request_duration_bucket{deployment_environment="dev",http_method="GET",http_path="/hello/:name",service_name="go-gin-demo",le="400.0"} 120 1710888060000
    demo_http_request_duration_bucket{deployment_environment="dev",http_method="GET",http_path="/hello/:name",service_name="go-gin-demo",le="+Inf"} 120 1710888060000
    demo_http_request_duration_sum{deployment_environment="dev",http_method="GET",http_path="/hello/:name",service_name="go-gin-demo"} 15600.0 1710888060000
    demo_http_request_duration_count{deployment_environment="dev",http_method="GET",http_path="/hello/:name",service_name="go-gin-demo"} 120 1710888060000
    ```
- Logs: Timestamped records
    - 业务场景
        /hello/:name 接口处理时的业务日志，包含正常日志和异常日志，关联 TraceID/SpanID
    - OTel 标准 Log 原始数据（JSON 格式）
    ```json
    {
    "resourceLogs": [
        {
        "resource": {
            "attributes": [
            {"key": "service.name", "value": {"stringValue": "go-gin-demo"}},
            {"key": "deployment.environment", "value": {"stringValue": "dev"}},
            {"key": "host.name", "value": {"stringValue": "localhost"}}
            ]
        },
        "scopeLogs": [
            {
            "scope": {"name": "gin-demo-app"},
            "logRecords": [
                // 示例1：正常业务日志（关联Trace/Span）
                {
                "timeUnixNano": "1710888000100000000", // 日志产生时间
                "severityText": "INFO",                // 日志级别
                "severityNumber": "9",                 // OTel标准级别（INFO=9）
                "body": {"stringValue": "Processed request for user otel successfully"}, // 日志内容
                "attributes": [
                    {"key": "trace_id", "value": {"stringValue": "0123456789abcdef0123456789abcdef"}},
                    {"key": "span_id", "value": {"stringValue": "1111111111111111"}},
                    {"key": "http.method", "value": {"stringValue": "GET"}},
                    {"key": "http.path", "value": {"stringValue": "/hello/:name"}},
                    {"key": "user.id", "value": {"stringValue": "user_123"}}
                ],
                "flags": "0"
                },
                // 示例2：异常日志（含堆栈信息）
                {
                "timeUnixNano": "1710888000200000000",
                "severityText": "ERROR",
                "severityNumber": "17", // OTel标准级别（ERROR=17）
                "body": {"stringValue": "Failed to query user info: database connection timeout"}, // 错误信息
                "attributes": [
                    {"key": "trace_id", "value": {"stringValue": "0123456789abcdef0123456789abcdef"}},
                    {"key": "span_id", "value": {"stringValue": "3333333333333333"}},
                    {"key": "error.type", "value": {"stringValue": "database.TimeoutError"}},
                    {"key": "error.message", "value": {"stringValue": "database connection timeout"}},
                    {"key": "error.stack_trace", "value": {"stringValue": "main.query_user_info:120\nmain.handle_hello_request:80\nmain.traceMiddleware:45"}} // 堆栈
                ],
                "flags": "0"
                }
            ]
            }
        ]
        }
    ]
    }
    ```
    - 常见输出格式（JSON 文本，可直接写入 Loki/ELK）
        - loki
        ```json
        // 正常日志
        {
        "timestamp": "2024-03-20T12:00:00.100Z",
        "level": "INFO",
        "service": "go-gin-demo",
        "environment": "dev",
        "host": "localhost",
        "message": "Processed request for user otel successfully",
        "trace_id": "0123456789abcdef0123456789abcdef",
        "span_id": "1111111111111111",
        "http_method": "GET",
        "http_path": "/hello/:name",
        "user_id": "user_123"
        }

        // 异常日志
        {
        "timestamp": "2024-03-20T12:00:00.200Z",
        "level": "ERROR",
        "service": "go-gin-demo",
        "environment": "dev",
        "host": "localhost",
        "message": "Failed to query user info: database connection timeout",
        "trace_id": "0123456789abcdef0123456789abcdef",
        "span_id": "3333333333333333",
        "error_type": "database.TimeoutError",
        "error_message": "database connection timeout",
        "error_stack_trace": "main.query_user_info:120\nmain.handle_hello_request:80\nmain.traceMiddleware:45"
        }
        ```
        - 纯文本日志（兼容传统日志系统）
        ```
        2024-03-20T12:00:00.100Z INFO [go-gin-demo] [trace_id=0123456789abcdef0123456789abcdef span_id=1111111111111111] Processed request for user otel successfully (http_method=GET, http_path=/hello/:name, user_id=user_123)
        2024-03-20T12:00:00.200Z ERROR [go-gin-demo] [trace_id=0123456789abcdef0123456789abcdef span_id=3333333333333333] Failed to query user info: database connection timeout (error_type=database.TimeoutError, error_message=database connection timeout)
        ```