/**
 * simple_producer.c - librdkafka Producer 示例
 * 
 * 编译命令：
 *   gcc -o simple_producer simple_producer.c -lrdkafka
 * 
 * 运行命令：
 *   ./simple_producer <broker> <topic>
 *   例如: ./simple_producer localhost:9092 test-topic
 */

/*
Producer 流程图
┌─────────────────────────────────────────────────────────────┐
│                    Producer 工作流程                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. rd_kafka_conf_new()          创建配置对象                 │
│              ↓                                              │
│  2. rd_kafka_conf_set()          设置 broker、回调等配置       │
│              ↓                                              │
│  3. rd_kafka_new(PRODUCER)       创建 Producer 实例          │
│              ↓                                              │
│  4. rd_kafka_producev()          发送消息到本地队列            │
│              ↓                                              │
│  5. rd_kafka_poll()              处理投递回调（异步确认）       │
│              ↓                                              │
│  6. rd_kafka_flush()             等待所有消息投递完成          │
│              ↓                                              │
│  7. rd_kafka_destroy()           销毁实例，释放资源            │
│                                                             │
└─────────────────────────────────────────────────────────────┘

关键 API 说明
API 函数	            用途
rd_kafka_conf_new()	    创建配置对象
rd_kafka_conf_set()	    设置配置参数
rd_kafka_new()	        创建 Kafka 实例（Producer/Consumer）
rd_kafka_producev()	    发送消息（可变参数版本）
rd_kafka_poll()	        处理事件和回调
rd_kafka_flush()	    等待所有消息投递完成
rd_kafka_subscribe()	订阅 Topic
rd_kafka_consumer_poll()	消费消息
rd_kafka_message_destroy()	释放消息内存
rd_kafka_consumer_close()	优雅关闭 Consumer
rd_kafka_destroy()	        销毁 Kafka 实例
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <librdkafka/rdkafka.h>

/**
 * 消息投递报告回调函数
 * 当消息被成功投递或投递失败时，librdkafka 会调用此回调
 * 
 * @param rk     Kafka handle
 * @param rkmsg  投递的消息
 * @param opaque 用户自定义的不透明指针（在 rd_kafka_conf_set_opaque 中设置）
 */
static void dr_msg_cb(rd_kafka_t *rk,
                      const rd_kafka_message_t *rkmsg,
                      void *opaque) {
    if (rkmsg->err) {
        // 消息投递失败
        fprintf(stderr, "消息投递失败: %s\n", rd_kafka_err2str(rkmsg->err));
    } else {
        // 消息投递成功
        fprintf(stderr, "消息投递成功 (partition=%d, offset=%ld, %zd bytes)\n",
                rkmsg->partition,
                rkmsg->offset,
                rkmsg->len);
    }
    
    // 注意: rkmsg 指向的内存由 librdkafka 管理，回调返回后会被释放
    // 如果消息有 payload 数据需要在回调中处理，需要在这里处理完毕
}

int main(int argc, char **argv) {
    rd_kafka_t *rk;            // Kafka producer 实例句柄
    rd_kafka_conf_t *conf;     // 配置对象
    char errstr[512];          // 错误信息缓冲区
    const char *brokers;       // Kafka broker 地址
    const char *topic;         // 目标 topic 名称
    
    // 检查命令行参数
    if (argc != 3) {
        fprintf(stderr, "用法: %s <broker> <topic>\n", argv[0]);
        return 1;
    }
    
    brokers = argv[1];
    topic = argv[2];
    
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
    // 这是必须的配置项，指定 Kafka 集群的地址
    if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers,
                          errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        fprintf(stderr, "配置错误: %s\n", errstr);
        rd_kafka_conf_destroy(conf);
        return 1;
    }
    
    // 设置消息投递报告回调函数
    // 这允许我们追踪每条消息的投递状态
    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);
    
    /*
     * ========================================
     * 步骤 3: 创建 Producer 实例
     * ========================================
     * 
     * rd_kafka_new() 会接管 conf 对象的所有权，
     * 之后不应该再使用 conf 指针
     */
    rk = rd_kafka_new(RD_KAFKA_PRODUCER,  // 类型: Producer
                      conf,                // 配置对象
                      errstr,              // 错误信息缓冲区
                      sizeof(errstr));     // 缓冲区大小
    
    if (!rk) {
        fprintf(stderr, "创建 Producer 失败: %s\n", errstr);
        return 1;
    }
    
    fprintf(stderr, "Producer 创建成功，连接到 %s\n", brokers);
    fprintf(stderr, "输入消息内容（每行一条消息，Ctrl+D 结束）:\n");
    
    /*
     * ========================================
     * 步骤 4: 发送消息
     * ========================================
     */
    char buf[512];
    while (fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        
        // 去掉换行符
        if (len > 0 && buf[len - 1] == '\n') {
            buf[--len] = '\0';
        }
        
        // 跳过空行
        if (len == 0) {
            // 即使没有新消息，也要调用 poll 来触发回调处理
            rd_kafka_poll(rk, 0);
            continue;
        }
        
        /*
         * rd_kafka_produce() - 发送消息到 Kafka
         * 
         * 参数说明:
         * - topic: 目标 topic 名称 (通过 rd_kafka_topic_new 创建，这里用简化版本)
         * - partition: 目标分区，RD_KAFKA_PARTITION_UA 表示由 partitioner 自动选择
         * - msgflags: 消息标志，RD_KAFKA_MSG_F_COPY 表示复制 payload 数据
         * - payload: 消息内容
         * - len: 消息长度
         * - key: 消息 key（用于分区选择）
         * - key_len: key 长度
         * - opaque: 用户不透明指针，会传递给投递回调
         * 
         * 返回值:
         * - 0: 消息已成功加入本地队列（不代表已投递成功）
         * - -1: 失败，通过 rd_kafka_last_error() 获取错误码
         */
    retry:
        if (rd_kafka_producev(
                rk,
                RD_KAFKA_V_TOPIC(topic),           // 目标 topic
                RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),  // 自动选择分区
                RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),     // 复制消息数据
                RD_KAFKA_V_VALUE(buf, len),        // 消息内容和长度
                RD_KAFKA_V_OPAQUE(NULL),           // 不透明指针
                RD_KAFKA_V_END                     // 参数列表结束标记
            ) == -1) {
            
            rd_kafka_resp_err_t err = rd_kafka_last_error();
            fprintf(stderr, "发送消息失败: %s\n", rd_kafka_err2str(err));
            
            // 如果是队列满的错误，等待后重试
            if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
                // 等待消息投递完成，腾出队列空间
                rd_kafka_poll(rk, 1000);  // 等待最多 1000ms
                goto retry;
            }
        } else {
            fprintf(stderr, "消息已加入发送队列: %s\n", buf);
        }
        
        /*
         * rd_kafka_poll() - 处理事件和回调
         * 
         * Producer 需要定期调用 poll 来:
         * 1. 处理投递报告回调
         * 2. 处理错误回调
         * 3. 维护与 broker 的连接
         * 
         * 参数 0 表示非阻塞，立即返回
         */
        rd_kafka_poll(rk, 0);
    }
    
    /*
     * ========================================
     * 步骤 5: 等待所有消息投递完成
     * ========================================
     * 
     * rd_kafka_flush() 会阻塞等待所有在队列中的消息被投递
     * 参数是超时时间（毫秒），-1 表示无限等待
     */
    fprintf(stderr, "\n正在等待所有消息投递完成...\n");
    rd_kafka_flush(rk, 10 * 1000);  // 等待最多 10 秒
    
    // 检查是否还有未投递的消息
    if (rd_kafka_outq_len(rk) > 0) {
        fprintf(stderr, "警告: %d 条消息未能投递\n", rd_kafka_outq_len(rk));
    }
    
    /*
     * ========================================
     * 步骤 6: 销毁 Producer 实例，释放资源
     * ========================================
     */
    rd_kafka_destroy(rk);
    
    fprintf(stderr, "Producer 已关闭\n");
    
    return 0;
}