/**
 * simple_consumer.c - librdkafka Consumer 示例
 * 
 * 编译命令：
 *   gcc -o simple_consumer simple_consumer.c -lrdkafka
 * 
 * 运行命令：
 *   ./simple_consumer <broker> <group_id> <topic1> [topic2 ...]
 *   例如: ./simple_consumer localhost:9092 my-group test-topic
 */

/*
Consumer 流程图
┌─────────────────────────────────────────────────────────────┐
│                    Consumer 工作流程                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. rd_kafka_conf_new()          创建配置对象                 │
│              ↓                                              │
│  2. rd_kafka_conf_set()          设置 broker、group.id 等     │
│              ↓                                              │
│  3. rd_kafka_new(CONSUMER)       创建 Consumer 实例          │
│              ↓                                              │
│  4. rd_kafka_poll_set_consumer() 启用 High-level Consumer    │
│              ↓                                              │
│  5. rd_kafka_subscribe()         订阅 Topic（加入消费者组）    │
│              ↓                                              │
│  ┌──────────────────────────────────────┐                   │
│  │  6. rd_kafka_consumer_poll()  消费循环 │ ←─────┐          │
│  │            ↓                          │       │          │
│  │     处理消息 / 处理错误                 │       │          │
│  │            ↓                          │       │          │
│  │  rd_kafka_message_destroy()   释放消息 ├───────┘          │
│  └──────────────────────────────────────┘                   │
│              ↓                                              │
│  7. rd_kafka_consumer_close()    关闭 Consumer               │
│              ↓                                              │
│  8. rd_kafka_destroy()           销毁实例，释放资源            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
*/

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <signal.h>
 #include <librdkafka/rdkafka.h>
 
 // 全局变量，用于控制主循环
 static volatile sig_atomic_t run = 1;
 
 /**
  * 信号处理函数
  * 捕获 Ctrl+C (SIGINT) 以优雅地关闭 Consumer
  */
 static void stop_handler(int sig) {
     run = 0;
 }
 
 /**
  * 分区再均衡回调函数
  * 当 Consumer Group 发生再均衡时被调用
  * 
  * @param rk        Kafka handle
  * @param err       再均衡事件类型
  * @param partitions 分配/撤销的分区列表
  * @param opaque    用户不透明指针
  */
 static void rebalance_cb(rd_kafka_t *rk,
                          rd_kafka_resp_err_t err,
                          rd_kafka_topic_partition_list_t *partitions,
                          void *opaque) {
     switch (err) {
         case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
             // 新分区被分配给此 Consumer
             fprintf(stderr, "分区分配事件: 获得 %d 个分区\n", partitions->cnt);
             
             // 打印分配的分区信息
             for (int i = 0; i < partitions->cnt; i++) {
                 fprintf(stderr, "  - %s [%d]\n",
                         partitions->elems[i].topic,
                         partitions->elems[i].partition);
             }
             
             // 必须调用 assign 来接受分区分配
             rd_kafka_assign(rk, partitions);
             break;
             
         case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
             // 分区被撤销（可能是因为再均衡或 Consumer 离开组）
             fprintf(stderr, "分区撤销事件: 失去 %d 个分区\n", partitions->cnt);
             
             // 调用 assign(NULL) 来放弃分区
             rd_kafka_assign(rk, NULL);
             break;
             
         default:
             // 再均衡失败
             fprintf(stderr, "再均衡错误: %s\n", rd_kafka_err2str(err));
             rd_kafka_assign(rk, NULL);
             break;
     }
 }
 
 int main(int argc, char **argv) {
     rd_kafka_t *rk;                      // Kafka consumer 实例句柄
     rd_kafka_conf_t *conf;               // 配置对象
     rd_kafka_topic_partition_list_t *subscription;  // 订阅的 topic 列表
     char errstr[512];                    // 错误信息缓冲区
     const char *brokers;                 // Kafka broker 地址
     const char *group_id;                // Consumer Group ID
     int topic_cnt;                       // topic 数量
     
     // 检查命令行参数
     if (argc < 4) {
         fprintf(stderr, "用法: %s <broker> <group_id> <topic1> [topic2 ...]\n", argv[0]);
         return 1;
     }
     
     brokers = argv[1];
     group_id = argv[2];
     topic_cnt = argc - 3;
     
     // 设置信号处理器，用于捕获 Ctrl+C
     signal(SIGINT, stop_handler);
     signal(SIGTERM, stop_handler);
     
     /*
      * ========================================
      * 步骤 1: 创建配置对象
      * ========================================
      */
     conf = rd_kafka_conf_new();
     
     /*
      * ========================================
      * 步骤 2: 设置配置参数
      * ========================================
      */
     
     // 设置 broker 地址列表
     if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers,
                           errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
         fprintf(stderr, "配置错误: %s\n", errstr);
         rd_kafka_conf_destroy(conf);
         return 1;
     }
     
     // 设置 Consumer Group ID
     // 这是必须的配置项，用于标识 Consumer 所属的组
     if (rd_kafka_conf_set(conf, "group.id", group_id,
                           errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
         fprintf(stderr, "配置错误: %s\n", errstr);
         rd_kafka_conf_destroy(conf);
         return 1;
     }
     
     // 设置自动提交 offset
     // 生产环境中可能需要手动提交以确保消息处理完成
     if (rd_kafka_conf_set(conf, "enable.auto.commit", "true",
                           errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
         fprintf(stderr, "配置错误: %s\n", errstr);
         rd_kafka_conf_destroy(conf);
         return 1;
     }
     
     // 设置 auto.offset.reset
     // earliest: 从最早的消息开始消费
     // latest: 从最新的消息开始消费（默认）
     if (rd_kafka_conf_set(conf, "auto.offset.reset", "earliest",
                           errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
         fprintf(stderr, "配置错误: %s\n", errstr);
         rd_kafka_conf_destroy(conf);
         return 1;
     }
     
     // 设置再均衡回调函数
     rd_kafka_conf_set_rebalance_cb(conf, rebalance_cb);
     
     /*
      * ========================================
      * 步骤 3: 创建 Consumer 实例
      * ========================================
      */
     rk = rd_kafka_new(RD_KAFKA_CONSUMER,  // 类型: Consumer
                       conf,                // 配置对象
                       errstr,              // 错误信息缓冲区
                       sizeof(errstr));     // 缓冲区大小
     
     if (!rk) {
         fprintf(stderr, "创建 Consumer 失败: %s\n", errstr);
         return 1;
     }
     
     /*
      * ========================================
      * 步骤 4: 将 Consumer 添加到消费者队列
      * ========================================
      * 
      * 这一步是使用 High-level Consumer API 所必需的
      * 它将 Consumer 的事件重定向到 Consumer 队列
      */
     rd_kafka_poll_set_consumer(rk);
     
     /*
      * ========================================
      * 步骤 5: 创建订阅列表并订阅 Topic
      * ========================================
      */
     
     // 创建 topic 分区列表
     subscription = rd_kafka_topic_partition_list_new(topic_cnt);
     
     // 添加要订阅的 topic
     for (int i = 0; i < topic_cnt; i++) {
         // 使用 RD_KAFKA_PARTITION_UA 表示订阅该 topic 的所有分区
         rd_kafka_topic_partition_list_add(subscription,
                                           argv[3 + i],          // topic 名称
                                           RD_KAFKA_PARTITION_UA);  // 所有分区
     }
     
     // 订阅 topic
     // rd_kafka_subscribe() 会触发 Consumer Group 的分区分配
     rd_kafka_resp_err_t err = rd_kafka_subscribe(rk, subscription);
     if (err) {
         fprintf(stderr, "订阅失败: %s\n", rd_kafka_err2str(err));
         rd_kafka_topic_partition_list_destroy(subscription);
         rd_kafka_destroy(rk);
         return 1;
     }
     
     fprintf(stderr, "Consumer 创建成功\n");
     fprintf(stderr, "  Broker: %s\n", brokers);
     fprintf(stderr, "  Group ID: %s\n", group_id);
     fprintf(stderr, "  订阅的 Topics:\n");
     for (int i = 0; i < topic_cnt; i++) {
         fprintf(stderr, "    - %s\n", argv[3 + i]);
     }
     fprintf(stderr, "\n等待消息... (按 Ctrl+C 退出)\n\n");
     
     // 订阅列表已被复制，可以销毁原始列表
     rd_kafka_topic_partition_list_destroy(subscription);
     
     /*
      * ========================================
      * 步骤 6: 消费消息循环
      * ========================================
      */
     while (run) {
         rd_kafka_message_t *rkmsg;
         
         /*
          * rd_kafka_consumer_poll() - 从消费队列获取消息
          * 
          * 参数是超时时间（毫秒）
          * - 如果有消息可用，立即返回
          * - 如果没有消息，最多等待指定时间
          * - 返回 NULL 表示超时或发生错误
          * 
          * 返回的消息需要调用 rd_kafka_message_destroy() 释放
          */
         rkmsg = rd_kafka_consumer_poll(rk, 1000);  // 等待最多 1 秒
         
         if (rkmsg == NULL) {
             // 超时，没有新消息
             continue;
         }
         
         /*
          * 处理消息
          * 
          * rkmsg->err 字段指示消息是否有效:
          * - 0 (RD_KAFKA_RESP_ERR_NO_ERROR): 正常消息
          * - 其他值: 错误或特殊事件
          */
         if (rkmsg->err) {
             if (rkmsg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
                 // 到达分区末尾（不是错误，只是没有更多消息了）
                 fprintf(stderr, "到达 %s [%d] 末尾 (offset %ld)\n",
                         rd_kafka_topic_name(rkmsg->rkt),
                         rkmsg->partition,
                         rkmsg->offset);
             } else {
                 // 真正的错误
                 fprintf(stderr, "消费错误: %s [%d] %s\n",
                         rkmsg->rkt ? rd_kafka_topic_name(rkmsg->rkt) : "unknown",
                         rkmsg->partition,
                         rd_kafka_message_errstr(rkmsg));
             }
         } else {
             // 正常消息，处理它
             printf("收到消息:\n");
             printf("  Topic: %s\n", rd_kafka_topic_name(rkmsg->rkt));
             printf("  Partition: %d\n", rkmsg->partition);
             printf("  Offset: %ld\n", rkmsg->offset);
             
             // 打印 key（如果有）
             if (rkmsg->key) {
                 printf("  Key: %.*s\n", (int)rkmsg->key_len, (char *)rkmsg->key);
             }
             
             // 打印消息内容
             printf("  Value: %.*s\n", (int)rkmsg->len, (char *)rkmsg->payload);
             printf("\n");
         }
         
         // 释放消息资源
         // 重要：每条从 rd_kafka_consumer_poll() 获取的消息都必须释放
         rd_kafka_message_destroy(rkmsg);
     }
     
     /*
      * ========================================
      * 步骤 7: 关闭 Consumer
      * ========================================
      */
     fprintf(stderr, "\n正在关闭 Consumer...\n");
     
     // 关闭 Consumer，会触发最后的再均衡并提交 offset
     err = rd_kafka_consumer_close(rk);
     if (err) {
         fprintf(stderr, "关闭 Consumer 失败: %s\n", rd_kafka_err2str(err));
     }
     
     // 销毁 Consumer 实例，释放所有资源
     rd_kafka_destroy(rk);
     
     fprintf(stderr, "Consumer 已关闭\n");
     
     return 0;
 }