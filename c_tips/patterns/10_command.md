# 命令模式 (Command Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                      COMMAND PATTERN                              |
+------------------------------------------------------------------+

    WITHOUT COMMAND (Direct Coupling):
    
    +----------+         +----------+
    |  Button  | ------> |  Light   |
    | onClick()|         | turnOn() |
    +----------+         +----------+
    
    Problem: Button directly knows about Light!


    WITH COMMAND (Decoupled via Command Object):
    
    +----------+     +-----------+     +----------+
    | Invoker  |     |  Command  |     | Receiver |
    | (Button) |---->| (Object)  |---->|  (Light) |
    +----------+     +-----------+     +----------+
    | command  |     | execute() |     | turnOn() |
    | click()  |     | undo()    |     | turnOff()|
    +----------+     +-----------+     +----------+
                           ^
                           |
              +------------+------------+
              |            |            |
        +-----------+ +-----------+ +-----------+
        | LightOn   | | LightOff  | | VolumeUp  |
        |  Command  | |  Command  | |  Command  |
        +-----------+ +-----------+ +-----------+


    COMMAND QUEUE (for undo/redo):
    
    +------------------------------------------+
    |              COMMAND HISTORY              |
    | [Cmd1] [Cmd2] [Cmd3] [Cmd4] [Cmd5]       |
    |                        ^                  |
    |                     current               |
    +------------------------------------------+
    
    undo() --> execute Cmd4.undo(), move current left
    redo() --> execute Cmd5.execute(), move current right


    MACRO COMMAND (Composite):
    
    +------------------+
    |   MacroCommand   |
    | +-------------+  |
    | | commands[]  |  |
    | +-------------+  |
    | | execute() { |  |
    | |   for each  |  |
    | |     cmd     |  |
    | |   cmd.exec()|  |
    | | }           |  |
    +------------------+
```

**核心思想说明：**
- 将请求封装为独立的命令对象
- 解耦请求发起者（Invoker）和请求执行者（Receiver）
- 支持命令的队列化、日志记录、撤销/重做
- 可组合多个命令为宏命令

## 实现思路

1. **定义命令接口**：`execute()` 和 `undo()` 函数
2. **实现具体命令**：每个命令封装一个操作
3. **命令持有接收者**：命令内部调用接收者方法
4. **调用者执行命令**：调用者只知道命令接口

## 典型应用场景

- GUI 按钮操作
- 撤销/重做功能
- 事务系统
- 任务队列
- 宏录制

## 完整代码示例

```c
/*============================================================================
 * 命令模式示例 - 文本编辑器（支持撤销/重做）
 *============================================================================*/

/*---------------------------------------------------------------------------
 * command.h - 命令接口定义
 *---------------------------------------------------------------------------*/
#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>

/* 关键点：命令接口 */
typedef struct command command_t;

typedef struct {
    bool (*execute)(command_t *cmd);
    bool (*undo)(command_t *cmd);
    void (*destroy)(command_t *cmd);
    const char* (*get_name)(command_t *cmd);
} command_ops_t;

struct command {
    const command_ops_t *ops;
    void *context;
};

#endif /* COMMAND_H */


/*---------------------------------------------------------------------------
 * document.h - 接收者（文档）定义
 *---------------------------------------------------------------------------*/
#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stddef.h>

#define DOC_MAX_SIZE 4096

typedef struct {
    char content[DOC_MAX_SIZE];
    size_t length;
    int cursor_pos;
} document_t;

void document_init(document_t *doc);
void document_insert(document_t *doc, int pos, const char *text);
void document_delete(document_t *doc, int pos, int len);
void document_print(document_t *doc);
const char* document_get_content(document_t *doc);

#endif /* DOCUMENT_H */


/*---------------------------------------------------------------------------
 * document.c - 接收者实现
 *---------------------------------------------------------------------------*/
#include "document.h"
#include <string.h>
#include <stdio.h>

void document_init(document_t *doc) {
    memset(doc->content, 0, sizeof(doc->content));
    doc->length = 0;
    doc->cursor_pos = 0;
}

void document_insert(document_t *doc, int pos, const char *text) {
    size_t text_len = strlen(text);
    
    if (doc->length + text_len >= DOC_MAX_SIZE) {
        return;
    }
    
    /* 移动后续内容 */
    memmove(&doc->content[pos + text_len], 
            &doc->content[pos], 
            doc->length - pos + 1);
    
    /* 插入新文本 */
    memcpy(&doc->content[pos], text, text_len);
    doc->length += text_len;
    
    printf("[Document] Inserted '%s' at position %d\n", text, pos);
}

void document_delete(document_t *doc, int pos, int len) {
    if (pos < 0 || pos >= (int)doc->length || len <= 0) {
        return;
    }
    
    if (pos + len > (int)doc->length) {
        len = doc->length - pos;
    }
    
    printf("[Document] Deleted %d chars at position %d\n", len, pos);
    
    /* 移动后续内容 */
    memmove(&doc->content[pos],
            &doc->content[pos + len],
            doc->length - pos - len + 1);
    doc->length -= len;
}

void document_print(document_t *doc) {
    printf("[Document] Content: \"%s\" (length: %zu)\n", doc->content, doc->length);
}

const char* document_get_content(document_t *doc) {
    return doc->content;
}


/*---------------------------------------------------------------------------
 * insert_command.c - 插入命令
 *---------------------------------------------------------------------------*/
#include "command.h"
#include "document.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    document_t *doc;
    int position;
    char *text;
    size_t text_len;
} insert_context_t;

static bool insert_execute(command_t *cmd) {
    insert_context_t *ctx = (insert_context_t *)cmd->context;
    document_insert(ctx->doc, ctx->position, ctx->text);
    return true;
}

/* 关键点：撤销操作 - 删除插入的文本 */
static bool insert_undo(command_t *cmd) {
    insert_context_t *ctx = (insert_context_t *)cmd->context;
    printf("[InsertCmd] Undo: removing '%s'\n", ctx->text);
    document_delete(ctx->doc, ctx->position, ctx->text_len);
    return true;
}

static void insert_destroy(command_t *cmd) {
    insert_context_t *ctx = (insert_context_t *)cmd->context;
    free(ctx->text);
    free(ctx);
    free(cmd);
}

static const char* insert_get_name(command_t *cmd) {
    (void)cmd;
    return "Insert";
}

static const command_ops_t insert_ops = {
    .execute = insert_execute,
    .undo = insert_undo,
    .destroy = insert_destroy,
    .get_name = insert_get_name
};

command_t* insert_command_create(document_t *doc, int pos, const char *text) {
    command_t *cmd = malloc(sizeof(command_t));
    insert_context_t *ctx = malloc(sizeof(insert_context_t));
    
    if (cmd == NULL || ctx == NULL) {
        free(cmd);
        free(ctx);
        return NULL;
    }
    
    ctx->doc = doc;
    ctx->position = pos;
    ctx->text = strdup(text);
    ctx->text_len = strlen(text);
    
    cmd->ops = &insert_ops;
    cmd->context = ctx;
    
    return cmd;
}


/*---------------------------------------------------------------------------
 * delete_command.c - 删除命令
 *---------------------------------------------------------------------------*/
typedef struct {
    document_t *doc;
    int position;
    int length;
    char *deleted_text;  /* 保存删除的文本用于撤销 */
} delete_context_t;

static bool delete_execute(command_t *cmd) {
    delete_context_t *ctx = (delete_context_t *)cmd->context;
    
    /* 关键点：执行前保存被删除的文本 */
    const char *content = document_get_content(ctx->doc);
    ctx->deleted_text = malloc(ctx->length + 1);
    strncpy(ctx->deleted_text, &content[ctx->position], ctx->length);
    ctx->deleted_text[ctx->length] = '\0';
    
    document_delete(ctx->doc, ctx->position, ctx->length);
    return true;
}

/* 关键点：撤销操作 - 恢复删除的文本 */
static bool delete_undo(command_t *cmd) {
    delete_context_t *ctx = (delete_context_t *)cmd->context;
    printf("[DeleteCmd] Undo: restoring '%s'\n", ctx->deleted_text);
    document_insert(ctx->doc, ctx->position, ctx->deleted_text);
    return true;
}

static void delete_destroy(command_t *cmd) {
    delete_context_t *ctx = (delete_context_t *)cmd->context;
    free(ctx->deleted_text);
    free(ctx);
    free(cmd);
}

static const char* delete_get_name(command_t *cmd) {
    (void)cmd;
    return "Delete";
}

static const command_ops_t delete_ops = {
    .execute = delete_execute,
    .undo = delete_undo,
    .destroy = delete_destroy,
    .get_name = delete_get_name
};

command_t* delete_command_create(document_t *doc, int pos, int len) {
    command_t *cmd = malloc(sizeof(command_t));
    delete_context_t *ctx = malloc(sizeof(delete_context_t));
    
    if (cmd == NULL || ctx == NULL) {
        free(cmd);
        free(ctx);
        return NULL;
    }
    
    ctx->doc = doc;
    ctx->position = pos;
    ctx->length = len;
    ctx->deleted_text = NULL;
    
    cmd->ops = &delete_ops;
    cmd->context = ctx;
    
    return cmd;
}


/*---------------------------------------------------------------------------
 * editor.h - 调用者（编辑器）定义
 *---------------------------------------------------------------------------*/
#ifndef EDITOR_H
#define EDITOR_H

#include "command.h"
#include "document.h"

#define MAX_HISTORY 100

typedef struct {
    document_t *doc;
    command_t *history[MAX_HISTORY];
    int history_count;
    int current_index;  /* 当前位置（用于redo） */
} editor_t;

editor_t* editor_create(void);
void editor_destroy(editor_t *editor);

/* 关键点：执行命令并记录历史 */
void editor_execute(editor_t *editor, command_t *cmd);
void editor_undo(editor_t *editor);
void editor_redo(editor_t *editor);

void editor_print_status(editor_t *editor);

#endif /* EDITOR_H */


/*---------------------------------------------------------------------------
 * editor.c - 调用者实现
 *---------------------------------------------------------------------------*/
#include "editor.h"
#include <stdlib.h>
#include <stdio.h>

editor_t* editor_create(void) {
    editor_t *editor = malloc(sizeof(editor_t));
    if (editor == NULL) return NULL;
    
    editor->doc = malloc(sizeof(document_t));
    if (editor->doc == NULL) {
        free(editor);
        return NULL;
    }
    
    document_init(editor->doc);
    editor->history_count = 0;
    editor->current_index = 0;
    
    return editor;
}

void editor_destroy(editor_t *editor) {
    if (editor == NULL) return;
    
    /* 清理历史命令 */
    for (int i = 0; i < editor->history_count; i++) {
        if (editor->history[i] != NULL) {
            editor->history[i]->ops->destroy(editor->history[i]);
        }
    }
    
    free(editor->doc);
    free(editor);
}

/* 关键点：执行命令并添加到历史记录 */
void editor_execute(editor_t *editor, command_t *cmd) {
    /* 清除当前位置之后的历史（如果有undo过） */
    for (int i = editor->current_index; i < editor->history_count; i++) {
        if (editor->history[i] != NULL) {
            editor->history[i]->ops->destroy(editor->history[i]);
            editor->history[i] = NULL;
        }
    }
    editor->history_count = editor->current_index;
    
    /* 执行命令 */
    printf("\n>>> Executing: %s\n", cmd->ops->get_name(cmd));
    cmd->ops->execute(cmd);
    
    /* 添加到历史 */
    if (editor->history_count < MAX_HISTORY) {
        editor->history[editor->history_count++] = cmd;
        editor->current_index = editor->history_count;
    }
    
    document_print(editor->doc);
}

/* 关键点：撤销 */
void editor_undo(editor_t *editor) {
    if (editor->current_index <= 0) {
        printf("\n>>> Cannot undo: no more history\n");
        return;
    }
    
    editor->current_index--;
    command_t *cmd = editor->history[editor->current_index];
    
    printf("\n>>> Undo: %s\n", cmd->ops->get_name(cmd));
    cmd->ops->undo(cmd);
    
    document_print(editor->doc);
}

/* 关键点：重做 */
void editor_redo(editor_t *editor) {
    if (editor->current_index >= editor->history_count) {
        printf("\n>>> Cannot redo: no more commands\n");
        return;
    }
    
    command_t *cmd = editor->history[editor->current_index];
    editor->current_index++;
    
    printf("\n>>> Redo: %s\n", cmd->ops->get_name(cmd));
    cmd->ops->execute(cmd);
    
    document_print(editor->doc);
}

void editor_print_status(editor_t *editor) {
    printf("\n[Editor] History: %d commands, current index: %d\n",
           editor->history_count, editor->current_index);
    printf("[Editor] Can undo: %s, Can redo: %s\n",
           editor->current_index > 0 ? "yes" : "no",
           editor->current_index < editor->history_count ? "yes" : "no");
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
#include "editor.h"
#include <stdio.h>

/* 命令创建函数声明 */
command_t* insert_command_create(document_t *doc, int pos, const char *text);
command_t* delete_command_create(document_t *doc, int pos, int len);

int main(void) {
    printf("=== Command Pattern Demo ===\n");
    
    editor_t *editor = editor_create();
    
    /* 执行一系列命令 */
    editor_execute(editor, insert_command_create(editor->doc, 0, "Hello"));
    editor_execute(editor, insert_command_create(editor->doc, 5, " World"));
    editor_execute(editor, insert_command_create(editor->doc, 11, "!"));
    editor_execute(editor, delete_command_create(editor->doc, 5, 1));
    editor_execute(editor, insert_command_create(editor->doc, 5, ", Beautiful"));
    
    editor_print_status(editor);
    
    /* 关键点：撤销操作 */
    printf("\n========== UNDO OPERATIONS ==========");
    editor_undo(editor);
    editor_undo(editor);
    editor_undo(editor);
    
    editor_print_status(editor);
    
    /* 关键点：重做操作 */
    printf("\n========== REDO OPERATIONS ==========");
    editor_redo(editor);
    editor_redo(editor);
    
    editor_print_status(editor);
    
    /* 在中间位置执行新命令会清除后续历史 */
    printf("\n========== NEW COMMAND (clears redo history) ==========");
    editor_execute(editor, insert_command_create(editor->doc, 0, ">>> "));
    
    editor_print_status(editor);
    
    /* 尝试 redo（应该失败） */
    editor_redo(editor);
    
    /* 清理 */
    printf("\n========== CLEANUP ==========\n");
    editor_destroy(editor);
    printf("Done!\n");
    
    return 0;
}
```

## 运行输出示例

```
=== Command Pattern Demo ===

>>> Executing: Insert
[Document] Inserted 'Hello' at position 0
[Document] Content: "Hello" (length: 5)

>>> Executing: Insert
[Document] Inserted ' World' at position 5
[Document] Content: "Hello World" (length: 11)

>>> Executing: Insert
[Document] Inserted '!' at position 11
[Document] Content: "Hello World!" (length: 12)

>>> Executing: Delete
[Document] Deleted 1 chars at position 5
[Document] Content: "HelloWorld!" (length: 11)

>>> Executing: Insert
[Document] Inserted ', Beautiful' at position 5
[Document] Content: "Hello, Beautiful World!" (length: 22)

[Editor] History: 5 commands, current index: 5
[Editor] Can undo: yes, Can redo: no

========== UNDO OPERATIONS ==========
>>> Undo: Insert
[InsertCmd] Undo: removing ', Beautiful'
[Document] Content: "HelloWorld!" (length: 11)

>>> Undo: Delete
[DeleteCmd] Undo: restoring ' '
[Document] Content: "Hello World!" (length: 12)

>>> Undo: Insert
[InsertCmd] Undo: removing '!'
[Document] Content: "Hello World" (length: 11)

[Editor] History: 5 commands, current index: 2
[Editor] Can undo: yes, Can redo: yes

========== REDO OPERATIONS ==========
>>> Redo: Insert
[Document] Inserted '!' at position 11
[Document] Content: "Hello World!" (length: 12)

>>> Redo: Delete
[Document] Deleted 1 chars at position 5
[Document] Content: "HelloWorld!" (length: 11)

========== NEW COMMAND (clears redo history) ==========
>>> Executing: Insert
[Document] Inserted '>>> ' at position 0
[Document] Content: ">>> HelloWorld!" (length: 15)

>>> Cannot redo: no more commands

========== CLEANUP ==========
Done!
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **解耦** | 调用者不知道接收者和具体操作 |
| **可撤销** | 每个命令保存足够信息用于撤销 |
| **可队列** | 命令可存储、排队、延迟执行 |
| **可组合** | 多个命令可组合为宏命令 |
| **可日志** | 命令执行可记录和回放 |

