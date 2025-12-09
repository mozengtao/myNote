# 桥接模式 (Bridge Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                       BRIDGE PATTERN                              |
+------------------------------------------------------------------+

    PROBLEM: Inheritance Explosion
    
    Without Bridge (Every combination needs a class):
    
                    Shape
                      |
        +-------------+-------------+
        |             |             |
      Circle      Rectangle      Triangle
        |             |             |
    +---+---+     +---+---+     +---+---+
    |       |     |       |     |       |
   Red    Blue   Red    Blue   Red    Blue
  Circle Circle Rect   Rect   Tri    Tri
    
    3 shapes x 2 colors = 6 classes! (N x M explosion)


    SOLUTION: Bridge separates Abstraction from Implementation
    
    +------------------+                    +------------------+
    |   ABSTRACTION    |       bridge       |  IMPLEMENTATION  |
    |     (Shape)      | -----------------> |    (Renderer)    |
    +------------------+                    +------------------+
           |                                        |
     +-----+-----+                            +-----+-----+
     |     |     |                            |           |
   Circle Rect Triangle                   VectorRender RasterRender
    
    3 shapes + 2 renderers = 5 classes! (N + M)


    STRUCTURE:
    
    +------------------+         +------------------+
    |   Shape          |         |   Renderer       |
    |  (Abstraction)   |    *    | (Implementation) |
    |  +------------+  | ------> |  +------------+  |
    |  | renderer   |--|         |  | render()   |  |
    |  +------------+  |         |  +------------+  |
    |  | draw()     |  |         +--------+--------+
    |  +------------+  |                  ^
    +--------+---------+                  |
             ^                    +-------+-------+
             |                    |               |
    +--------+--------+    +------+-----+  +------+-----+
    |    Circle       |    |   Vector   |  |   Raster   |
    |  draw() {       |    |  Renderer  |  |  Renderer  |
    |    renderer->   |    +------------+  +------------+
    |      render();  |
    |  }              |
    +-----------------+
```

**核心思想说明：**
- 将抽象部分与实现部分分离，使它们可以独立变化
- 通过组合代替继承，避免类爆炸
- 抽象层持有实现层的引用（桥接）
- 两个维度可以独立扩展

## 实现思路

1. **定义实现接口**：渲染器/驱动等底层实现接口
2. **定义抽象层**：持有实现接口的指针（桥）
3. **实现具体实现**：各种实现方式
4. **实现具体抽象**：各种抽象类型
5. **组合使用**：运行时将抽象和实现组合

## 典型应用场景

- 跨平台图形渲染
- 数据库驱动抽象
- 消息发送（多渠道）
- 设备驱动与业务逻辑分离

## 完整代码示例

```c
/*============================================================================
 * 桥接模式示例 - 消息发送系统（消息类型 x 发送渠道）
 *============================================================================*/

/*---------------------------------------------------------------------------
 * message_sender.h - 实现接口定义（发送渠道）
 *---------------------------------------------------------------------------*/
#ifndef MESSAGE_SENDER_H
#define MESSAGE_SENDER_H

#include <stdint.h>
#include <stdbool.h>

/* 关键点：实现接口 - 各种发送渠道 */
typedef struct message_sender message_sender_t;

typedef struct {
    bool (*send)(message_sender_t *sender, const char *recipient, 
                 const char *subject, const char *body);
    void (*destroy)(message_sender_t *sender);
} sender_ops_t;

struct message_sender {
    const sender_ops_t *ops;
    void *config;
};

/* 具体发送器创建函数 */
message_sender_t* email_sender_create(const char *smtp_server, int port);
message_sender_t* sms_sender_create(const char *api_key);
message_sender_t* push_sender_create(const char *app_id);

#endif /* MESSAGE_SENDER_H */


/*---------------------------------------------------------------------------
 * message.h - 抽象层定义（消息类型）
 *---------------------------------------------------------------------------*/
#ifndef MESSAGE_H
#define MESSAGE_H

#include "message_sender.h"

/* 关键点：抽象层 - 各种消息类型 */
typedef struct message message_t;

typedef struct {
    bool (*send)(message_t *msg, const char *recipient);
    void (*set_content)(message_t *msg, const char *content);
    void (*destroy)(message_t *msg);
} message_ops_t;

struct message {
    const message_ops_t *ops;
    message_sender_t *sender;  /* 关键点：桥接 - 持有实现层引用 */
    void *data;
};

/* 具体消息类型创建函数 */
message_t* alert_message_create(message_sender_t *sender);
message_t* report_message_create(message_sender_t *sender);
message_t* newsletter_message_create(message_sender_t *sender);

#endif /* MESSAGE_H */


/*---------------------------------------------------------------------------
 * email_sender.c - Email 发送实现
 *---------------------------------------------------------------------------*/
#include "message_sender.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char smtp_server[128];
    int port;
} email_config_t;

static bool email_send(message_sender_t *sender, const char *recipient,
                       const char *subject, const char *body) {
    email_config_t *cfg = (email_config_t *)sender->config;
    
    printf("[EMAIL] Connecting to %s:%d\n", cfg->smtp_server, cfg->port);
    printf("[EMAIL] To: %s\n", recipient);
    printf("[EMAIL] Subject: %s\n", subject);
    printf("[EMAIL] Body: %.50s%s\n", body, strlen(body) > 50 ? "..." : "");
    printf("[EMAIL] Sent successfully!\n");
    
    return true;
}

static void email_destroy(message_sender_t *sender) {
    if (sender != NULL) {
        free(sender->config);
        free(sender);
    }
}

static const sender_ops_t email_ops = {
    .send = email_send,
    .destroy = email_destroy
};

message_sender_t* email_sender_create(const char *smtp_server, int port) {
    message_sender_t *sender = malloc(sizeof(message_sender_t));
    email_config_t *cfg = malloc(sizeof(email_config_t));
    
    if (sender == NULL || cfg == NULL) {
        free(sender);
        free(cfg);
        return NULL;
    }
    
    strncpy(cfg->smtp_server, smtp_server, sizeof(cfg->smtp_server) - 1);
    cfg->port = port;
    
    sender->ops = &email_ops;
    sender->config = cfg;
    
    printf("[EMAIL] Sender created for %s:%d\n", smtp_server, port);
    return sender;
}


/*---------------------------------------------------------------------------
 * sms_sender.c - SMS 发送实现
 *---------------------------------------------------------------------------*/
typedef struct {
    char api_key[64];
} sms_config_t;

static bool sms_send(message_sender_t *sender, const char *recipient,
                     const char *subject, const char *body) {
    sms_config_t *cfg = (sms_config_t *)sender->config;
    (void)subject;  /* SMS 不用 subject */
    
    printf("[SMS] Using API key: %s...\n", cfg->api_key);
    printf("[SMS] To: %s\n", recipient);
    printf("[SMS] Message: %.100s%s\n", body, strlen(body) > 100 ? "..." : "");
    printf("[SMS] Sent successfully!\n");
    
    return true;
}

static void sms_destroy(message_sender_t *sender) {
    if (sender != NULL) {
        free(sender->config);
        free(sender);
    }
}

static const sender_ops_t sms_ops = {
    .send = sms_send,
    .destroy = sms_destroy
};

message_sender_t* sms_sender_create(const char *api_key) {
    message_sender_t *sender = malloc(sizeof(message_sender_t));
    sms_config_t *cfg = malloc(sizeof(sms_config_t));
    
    if (sender == NULL || cfg == NULL) {
        free(sender);
        free(cfg);
        return NULL;
    }
    
    strncpy(cfg->api_key, api_key, sizeof(cfg->api_key) - 1);
    
    sender->ops = &sms_ops;
    sender->config = cfg;
    
    printf("[SMS] Sender created with API key\n");
    return sender;
}


/*---------------------------------------------------------------------------
 * push_sender.c - Push 通知发送实现
 *---------------------------------------------------------------------------*/
typedef struct {
    char app_id[64];
} push_config_t;

static bool push_send(message_sender_t *sender, const char *recipient,
                      const char *subject, const char *body) {
    push_config_t *cfg = (push_config_t *)sender->config;
    
    printf("[PUSH] App ID: %s\n", cfg->app_id);
    printf("[PUSH] Device Token: %s\n", recipient);
    printf("[PUSH] Title: %s\n", subject);
    printf("[PUSH] Body: %.50s%s\n", body, strlen(body) > 50 ? "..." : "");
    printf("[PUSH] Sent successfully!\n");
    
    return true;
}

static void push_destroy(message_sender_t *sender) {
    if (sender != NULL) {
        free(sender->config);
        free(sender);
    }
}

static const sender_ops_t push_ops = {
    .send = push_send,
    .destroy = push_destroy
};

message_sender_t* push_sender_create(const char *app_id) {
    message_sender_t *sender = malloc(sizeof(message_sender_t));
    push_config_t *cfg = malloc(sizeof(push_config_t));
    
    if (sender == NULL || cfg == NULL) {
        free(sender);
        free(cfg);
        return NULL;
    }
    
    strncpy(cfg->app_id, app_id, sizeof(cfg->app_id) - 1);
    
    sender->ops = &push_ops;
    sender->config = cfg;
    
    printf("[PUSH] Sender created for app %s\n", app_id);
    return sender;
}


/*---------------------------------------------------------------------------
 * alert_message.c - 警报消息（抽象层实现）
 *---------------------------------------------------------------------------*/
#include "message.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    char content[256];
    int severity;
} alert_data_t;

static bool alert_send(message_t *msg, const char *recipient) {
    alert_data_t *data = (alert_data_t *)msg->data;
    
    /* 构建警报消息 */
    char subject[64];
    char body[512];
    
    snprintf(subject, sizeof(subject), "⚠️ ALERT [Severity: %d]", data->severity);
    
    time_t now = time(NULL);
    snprintf(body, sizeof(body), 
             "Alert Time: %s"
             "Severity Level: %d\n"
             "Details: %s",
             ctime(&now), data->severity, data->content);
    
    /* 关键点：通过桥接调用具体发送实现 */
    return msg->sender->ops->send(msg->sender, recipient, subject, body);
}

static void alert_set_content(message_t *msg, const char *content) {
    alert_data_t *data = (alert_data_t *)msg->data;
    strncpy(data->content, content, sizeof(data->content) - 1);
}

static void alert_destroy(message_t *msg) {
    if (msg != NULL) {
        free(msg->data);
        free(msg);
    }
}

static const message_ops_t alert_ops = {
    .send = alert_send,
    .set_content = alert_set_content,
    .destroy = alert_destroy
};

message_t* alert_message_create(message_sender_t *sender) {
    message_t *msg = malloc(sizeof(message_t));
    alert_data_t *data = malloc(sizeof(alert_data_t));
    
    if (msg == NULL || data == NULL) {
        free(msg);
        free(data);
        return NULL;
    }
    
    memset(data, 0, sizeof(alert_data_t));
    data->severity = 1;
    
    msg->ops = &alert_ops;
    msg->sender = sender;  /* 关键点：建立桥接 */
    msg->data = data;
    
    return msg;
}


/*---------------------------------------------------------------------------
 * report_message.c - 报表消息
 *---------------------------------------------------------------------------*/
typedef struct {
    char title[128];
    char content[1024];
    char footer[64];
} report_data_t;

static bool report_send(message_t *msg, const char *recipient) {
    report_data_t *data = (report_data_t *)msg->data;
    
    char subject[256];
    char body[2048];
    
    snprintf(subject, sizeof(subject), "📊 Report: %s", data->title);
    snprintf(body, sizeof(body),
             "========== REPORT ==========\n"
             "%s\n"
             "============================\n"
             "%s",
             data->content, data->footer);
    
    /* 关键点：通过桥接调用具体发送实现 */
    return msg->sender->ops->send(msg->sender, recipient, subject, body);
}

static void report_set_content(message_t *msg, const char *content) {
    report_data_t *data = (report_data_t *)msg->data;
    strncpy(data->content, content, sizeof(data->content) - 1);
}

static void report_destroy(message_t *msg) {
    if (msg != NULL) {
        free(msg->data);
        free(msg);
    }
}

static const message_ops_t report_ops = {
    .send = report_send,
    .set_content = report_set_content,
    .destroy = report_destroy
};

message_t* report_message_create(message_sender_t *sender) {
    message_t *msg = malloc(sizeof(message_t));
    report_data_t *data = malloc(sizeof(report_data_t));
    
    if (msg == NULL || data == NULL) {
        free(msg);
        free(data);
        return NULL;
    }
    
    memset(data, 0, sizeof(report_data_t));
    strcpy(data->title, "Daily Report");
    strcpy(data->footer, "Generated automatically");
    
    msg->ops = &report_ops;
    msg->sender = sender;
    msg->data = data;
    
    return msg;
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
#include "message.h"
#include <stdio.h>

int main(void) {
    printf("=== Bridge Pattern Demo ===\n\n");
    
    /* 关键点：创建不同的发送渠道（实现层） */
    printf("--- Creating Senders (Implementations) ---\n");
    message_sender_t *email = email_sender_create("smtp.example.com", 587);
    message_sender_t *sms = sms_sender_create("sk-xxxx-yyyy-zzzz");
    message_sender_t *push = push_sender_create("com.myapp.notification");
    
    printf("\n--- Creating Messages (Abstractions) ---\n\n");
    
    /* 关键点：创建消息并桥接到不同发送渠道 */
    
    /* 警报 + Email */
    printf("=== Alert via Email ===\n");
    message_t *alert_email = alert_message_create(email);
    alert_email->ops->set_content(alert_email, "Server CPU usage exceeded 90%!");
    alert_email->ops->send(alert_email, "admin@example.com");
    
    printf("\n=== Alert via SMS ===\n");
    /* 同样的警报类型，不同的发送渠道 */
    message_t *alert_sms = alert_message_create(sms);
    alert_sms->ops->set_content(alert_sms, "Server CPU usage exceeded 90%!");
    alert_sms->ops->send(alert_sms, "+1234567890");
    
    printf("\n=== Report via Email ===\n");
    /* 不同的消息类型 */
    message_t *report = report_message_create(email);
    report->ops->set_content(report, 
        "Sales: $10,000\n"
        "Orders: 150\n"
        "Returns: 5");
    report->ops->send(report, "manager@example.com");
    
    printf("\n=== Alert via Push Notification ===\n");
    message_t *alert_push = alert_message_create(push);
    alert_push->ops->set_content(alert_push, "New login from unknown device!");
    alert_push->ops->send(alert_push, "device_token_abc123");
    
    /* 清理 */
    printf("\n--- Cleanup ---\n");
    alert_email->ops->destroy(alert_email);
    alert_sms->ops->destroy(alert_sms);
    alert_push->ops->destroy(alert_push);
    report->ops->destroy(report);
    
    email->ops->destroy(email);
    sms->ops->destroy(sms);
    push->ops->destroy(push);
    
    printf("Done!\n");
    return 0;
}
```

## 运行输出示例

```
=== Bridge Pattern Demo ===

--- Creating Senders (Implementations) ---
[EMAIL] Sender created for smtp.example.com:587
[SMS] Sender created with API key
[PUSH] Sender created for app com.myapp.notification

--- Creating Messages (Abstractions) ---

=== Alert via Email ===
[EMAIL] Connecting to smtp.example.com:587
[EMAIL] To: admin@example.com
[EMAIL] Subject: ⚠️ ALERT [Severity: 1]
[EMAIL] Body: Alert Time: ...
[EMAIL] Sent successfully!

=== Alert via SMS ===
[SMS] Using API key: sk-xxxx-yyyy-zzzz...
[SMS] To: +1234567890
[SMS] Message: Alert Time: ...
[SMS] Sent successfully!

=== Report via Email ===
[EMAIL] Connecting to smtp.example.com:587
[EMAIL] To: manager@example.com
[EMAIL] Subject: 📊 Report: Daily Report
[EMAIL] Body: ========== REPORT ==========...
[EMAIL] Sent successfully!

=== Alert via Push Notification ===
[PUSH] App ID: com.myapp.notification
[PUSH] Device Token: device_token_abc123
[PUSH] Title: ⚠️ ALERT [Severity: 1]
[PUSH] Body: Alert Time: ...
[PUSH] Sent successfully!

--- Cleanup ---
Done!
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **避免类爆炸** | N+M 替代 N×M |
| **独立扩展** | 消息类型和发送渠道独立扩展 |
| **运行时绑定** | 可动态切换实现 |
| **单一职责** | 抽象和实现各负其责 |
| **开闭原则** | 新增不修改现有代码 |

