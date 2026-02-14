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