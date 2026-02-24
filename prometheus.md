[Prometheus](https://prometheus.io/)  
[]()  


[What is Prometheus?](https://prometheus.io/docs/introduction/overview/)  
[What is Prometheus?](https://pagertree.com/learn/prometheus/overview)  
[]()  
[**prometheus-book**](https://yunlzheng.gitbook.io/prometheus-book)  

[PromQL (Prometheus Query Language)](https://prometheus.io/docs/prometheus/latest/querying/basics/)  
[An Intro to PromQL: Basic Concepts & Examples](https://logz.io/blog/promql-examples-introduction/)  
[PromQL: A Developer's Guide to Prometheus Query Language](https://last9.io/blog/guide-to-prometheus-query-language/)  
[PromQL Cheat Sheet](https://promlabs.com/promql-cheat-sheet/)  
[]()  
[]()  
> The power tool for querying Prometheus, Build, understand, and fix your queries much more effectively with the ultimate query builder for PromQL
[PromLens](https://promlens.com/)  

- Example: a time series for HTTP requests 
```
http_requests_total{status="200", method="GET"}  1234  1623456789000
|__________________| \________________________/ |____| |___________|
         |                       |                |          |
    Metric Name               Labels            Value    Timestamp
         |             (Key-Value Pairs)          |     (Unix ms)
         |                       |                |          |
  "What is being       "Dimensions to filter      |     "When it 
    measured"           or group results"         |      happened"
                                                  |
                                         "The actual count
                                          at this moment"
```

[**Learn Prometheus**](https://training.promlabs.com/)  
[]()  
[Prometheus](https://prometheus.io/docs/concepts/data_model/)  
[EXPOSITION FORMATS](https://prometheus.io/docs/instrumenting/exposition_formats/)  

[Prometheus system architecture](./assets/Prometheus_system_architecture.svg)  

## 测试
### PromLens
[PromLens](https://promlabs.com/promql-cheat-sheet/)  
### docker + mock exporter (导入数据 + 可视化执行)
```bash
# 1. 写一个简单的 mock exporter（纯文本文件即可）
mkdir -p /tmp/prom-test

cat > /tmp/prom-test/metrics.txt << 'EOF'
# HELP collectd_cm_registration_state_reg_status_code CM registration state
# TYPE collectd_cm_registration_state_reg_status_code gauge
collectd_cm_registration_state_reg_status_code{service="vmc-1",cm="aa:bb:cc:dd:ee:ff"} 6
collectd_cm_registration_state_reg_status_code{service="vmc-1",cm="11:22:33:44:55:66"} 8
collectd_cm_registration_state_reg_status_code{service="vmc-1",cm="22:33:44:55:66:77"} 8
collectd_cm_registration_state_reg_status_code{service="vmc-1",cm="33:44:55:66:77:88"} 5

# HELP ossi_DocsIf31CmtsCmRegStatusPartialSvcState Partial service state
# TYPE ossi_DocsIf31CmtsCmRegStatusPartialSvcState gauge
ossi_DocsIf31CmtsCmRegStatusPartialSvcState{service="vmc-1",mac="22:33:44:55:66:77"} 3
ossi_DocsIf31CmtsCmRegStatusPartialSvcState{service="vmc-1",mac="33:44:55:66:77:88"} 4
EOF

# 2. 用 Python 起一个简易 HTTP 服务来 serve 这些 metrics
cd /tmp/prom-test
python3 -c '
import http.server
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type","text/plain")
        self.end_headers()
        with open("metrics.txt","rb") as f:
            self.wfile.write(f.read())
http.server.HTTPServer(("",9999),H).serve_forever()
' &

# 3. 配置 Prometheus 抓取这个 exporter
cat > /tmp/prom-test/prometheus.yml << 'EOF'
global:
  scrape_interval: 5s

scrape_configs:
  - job_name: mock
    static_configs:
      - targets: ["host.docker.internal:9999"]
EOF

# 4. 启动 Prometheus
docker run -d --name prom-test \
  -p 9090:9090 \
  --add-host=host.docker.internal:host-gateway \
  -v /tmp/prom-test/prometheus.yml:/etc/prometheus/prometheus.yml \
  prom/prometheus

# 5. 等待几秒后，打开浏览器
# http://localhost:9090/graph
```

###  promtool + unit test (离线验证，无需运行服务)
```bash
# 安装 promtool（随 Prometheus 一起发布）
# 方式1: 从 Prometheus release 包中获取
# 下载最新 Prometheus（包含 promtool）
cd /tmp
curl -LO https://github.com/prometheus/prometheus/releases/download/v3.3.0/prometheus-3.3.0.linux-amd64.tar.gz

# 解压
tar xzf prometheus-3.3.0.linux-amd64.tar.gz

# 把 promtool 放到 PATH 中
sudo cp prometheus-3.3.0.linux-amd64/promtool /usr/local/bin/

# 验证
promtool --version

# 方式2: go install
go install github.com/prometheus/prometheus/cmd/promtool@latest

# 运行测试
promtool test rules test_expr.yml

# 如果预期值写错了，它会告诉你实际结果是什么 — 这就变成了一个交互式探索工具
```

```yaml
# test_expr.yml
evaluation_interval: 1m

tests:
  - interval: 1m
    input_series:
      # 4 台 CM 的注册状态
      - series: 'collectd_cm_registration_state_reg_status_code{service="vmc-1", cm="aa:bb:cc:dd:ee:ff"}'
        values: "6"
      - series: 'collectd_cm_registration_state_reg_status_code{service="vmc-1", cm="11:22:33:44:55:66"}'
        values: "8"
      - series: 'collectd_cm_registration_state_reg_status_code{service="vmc-1", cm="22:33:44:55:66:77"}'
        values: "8"
      - series: 'collectd_cm_registration_state_reg_status_code{service="vmc-1", cm="33:44:55:66:77:88"}'
        values: "5"

      # 其中 2 台处于部分服务 (PartialSvcState > 2)
      - series: 'ossi_DocsIf31CmtsCmRegStatusPartialSvcState{service="vmc-1", mac="22:33:44:55:66:77"}'
        values: "3"
      - series: 'ossi_DocsIf31CmtsCmRegStatusPartialSvcState{service="vmc-1", mac="33:44:55:66:77:88"}'
        values: "4"

    # 测试你的表达式
    promql_expr_test:
      - expr: |
          count_values("State",
            (collectd_cm_registration_state_reg_status_code{service="vmc-1"}
              unless on(service, cm)
              label_replace(ossi_DocsIf31CmtsCmRegStatusPartialSvcState{service="vmc-1"} > 2, "cm", "$1", "mac", "(.*)"))
            or
            ((((collectd_cm_registration_state_reg_status_code{service="vmc-1"} * 0) + 8))
              and on(service, cm)
              label_replace(ossi_DocsIf31CmtsCmRegStatusPartialSvcState{service="vmc-1"} > 2, "cm", "$1", "mac", "(.*)"))
          )
        eval_time: 1m
        exp_samples:
          - labels: '{State="6"}'
            value: 1
          - labels: '{State="8"}'
            value: 3
```

```yaml
# label_replace_demo.yml
evaluation_interval: 1m

tests:
  - interval: 1m
    input_series:
      - series: 'ossi_DocsIf31CmtsCmRegStatusPartialSvcState{service="vmc-1", mac="aa:bb:cc:dd:ee:ff"}'
        values: "3"
      - series: 'ossi_DocsIf31CmtsCmRegStatusPartialSvcState{service="vmc-1", mac="11:22:33:44:55:66"}'
        values: "1"

    promql_expr_test:
      # 测试1: label_replace 之前 — 原始 series
      - expr: 'ossi_DocsIf31CmtsCmRegStatusPartialSvcState{service="vmc-1"}'
        eval_time: 1m
        exp_samples:
          - labels: '{__name__="SHOW_ME"}'
            value: 0

      # 测试2: label_replace 之后 — 观察新增的 cm 标签
      - expr: |
          label_replace(
            ossi_DocsIf31CmtsCmRegStatusPartialSvcState{service="vmc-1"},
            "cm", "$1", "mac", "(.*)"
          )
        eval_time: 1m
        exp_samples:
          - labels: '{__name__="SHOW_ME"}'
            value: 0

      # 测试3: label_replace + 过滤 > 2 — 只保留部分服务的 CM
      - expr: |
          label_replace(
            ossi_DocsIf31CmtsCmRegStatusPartialSvcState{service="vmc-1"} > 2,
            "cm", "$1", "mac", "(.*)"
          )
        eval_time: 1m
        exp_samples:
          - labels: '{__name__="SHOW_ME"}'
            value: 0
```