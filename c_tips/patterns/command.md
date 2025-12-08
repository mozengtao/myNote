# Command Pattern (命令模式)

## 1. Core Concept and Use Cases

### Core Concept
Encapsulate a request as an object, allowing you to parameterize clients with different requests, queue or log requests, and support **undoable operations**.

### Typical Use Cases
- Command-line interface implementation
- Undo/Redo functionality
- Task queue/job scheduler
- Transaction management
- Macro recording and playback

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                      Command Pattern                                              |
+--------------------------------------------------------------------------------------------------+

    +-------------+                                              +-------------+
    |   Client    |                                              |  Receiver   |
    +------+------+                                              +------+------+
           |                                                            ^
           | creates                                                    |
           v                                                            |
    +------------------+                                                |
    | Concrete Command |                                                |
    +------------------+         execute() calls                        |
    | - receiver       |------> receiver->action() ------------------->+
    | - parameters     |
    +------------------+
    | + execute()      |
    | + undo()         |
    +--------+---------+
             |
             | stored in
             v
    +---------------------------+
    |        Invoker            |
    +---------------------------+
    | - command_history[]       |
    | - current_index           |
    +---------------------------+
    | + execute_command(cmd)    |
    | + undo()                  |
    | + redo()                  |
    +---------------------------+


    Command Queue / History:
    +-------+-------+-------+-------+-------+
    | Cmd 1 | Cmd 2 | Cmd 3 | Cmd 4 | ...   |
    +-------+-------+-------+-------+-------+
                              ^
                              |
                        current_index
                        
    Undo: execute cmd[current_index]->undo(), current_index--
    Redo: current_index++, execute cmd[current_index]->execute()
```

**中文说明：**

命令模式的核心流程：

1. **命令（Command）**：
   - 封装了操作请求
   - 包含接收者引用和操作参数
   - 提供 `execute()` 和 `undo()` 方法

2. **调用者（Invoker）**：
   - 存储命令历史
   - 执行命令并记录
   - 支持撤销（undo）和重做（redo）

3. **接收者（Receiver）**：
   - 实际执行操作的对象
   - 命令通过它完成具体工作

---

## 3. Code Skeleton

```c
/* Command interface */
typedef struct Command {
    void (*execute)(struct Command* self);
    void (*undo)(struct Command* self);
    void* receiver;
    void* data;
} Command;

/* Invoker (command manager) */
typedef struct {
    Command* history[MAX_HISTORY];
    int history_count;
    int current_index;
} CommandInvoker;

/* Invoker operations */
void invoker_execute(CommandInvoker* inv, Command* cmd);
void invoker_undo(CommandInvoker* inv);
void invoker_redo(CommandInvoker* inv);
```

**中文说明：**

代码骨架包含：
- `Command`：命令接口，包含 execute 和 undo 函数指针
- `CommandInvoker`：命令管理器，维护命令历史
- 核心操作：`execute`、`undo`、`redo`

---

## 4. Complete Example Code

```c
/*
 * Command Pattern - Text Editor Example
 * 
 * This example demonstrates a simple text editor with
 * undo/redo functionality using the command pattern.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TEXT_SIZE 1024
#define MAX_HISTORY 50

/* ============================================
 * Receiver: Text Buffer
 * ============================================ */
typedef struct {
    char text[MAX_TEXT_SIZE];
    int length;
    int cursor_pos;
} TextBuffer;

/* TextBuffer operations */
void buffer_init(TextBuffer* buf)
{
    memset(buf->text, 0, MAX_TEXT_SIZE);
    buf->length = 0;
    buf->cursor_pos = 0;
}

void buffer_insert(TextBuffer* buf, int pos, const char* str)
{
    int str_len = strlen(str);
    if (buf->length + str_len >= MAX_TEXT_SIZE) return;
    
    /* Shift existing text to make room */
    memmove(&buf->text[pos + str_len], &buf->text[pos], buf->length - pos + 1);
    
    /* Insert new text */
    memcpy(&buf->text[pos], str, str_len);
    buf->length += str_len;
    buf->cursor_pos = pos + str_len;
}

void buffer_delete(TextBuffer* buf, int pos, int len)
{
    if (pos + len > buf->length) len = buf->length - pos;
    
    /* Shift text to remove characters */
    memmove(&buf->text[pos], &buf->text[pos + len], buf->length - pos - len + 1);
    buf->length -= len;
    buf->cursor_pos = pos;
}

void buffer_print(TextBuffer* buf)
{
    printf("Text [%d chars]: \"%s\"\n", buf->length, buf->text);
}

/* ============================================
 * Command Interface
 * ============================================ */
typedef struct Command Command;

struct Command {
    void (*execute)(Command* self);       /* Execute the command */
    void (*undo)(Command* self);          /* Undo the command */
    void (*destroy)(Command* self);       /* Free command resources */
    char description[64];                 /* Command description */
};

/* ============================================
 * Concrete Command: Insert Text
 * ============================================ */
typedef struct {
    Command base;                         /* Base command (inheritance) */
    TextBuffer* buffer;                   /* Receiver */
    int position;                         /* Insert position */
    char* text;                           /* Text to insert */
    int text_len;                         /* Text length */
} InsertCommand;

void insert_execute(Command* self)
{
    InsertCommand* cmd = (InsertCommand*)self;
    printf("  [Execute] Insert \"%s\" at position %d\n", cmd->text, cmd->position);
    buffer_insert(cmd->buffer, cmd->position, cmd->text);
}

void insert_undo(Command* self)
{
    InsertCommand* cmd = (InsertCommand*)self;
    printf("  [Undo] Delete %d chars at position %d\n", cmd->text_len, cmd->position);
    buffer_delete(cmd->buffer, cmd->position, cmd->text_len);
}

void insert_destroy(Command* self)
{
    InsertCommand* cmd = (InsertCommand*)self;
    if (cmd->text) free(cmd->text);
    free(cmd);
}

Command* create_insert_command(TextBuffer* buf, int pos, const char* text)
{
    InsertCommand* cmd = (InsertCommand*)malloc(sizeof(InsertCommand));
    if (cmd == NULL) return NULL;
    
    cmd->base.execute = insert_execute;
    cmd->base.undo = insert_undo;
    cmd->base.destroy = insert_destroy;
    snprintf(cmd->base.description, sizeof(cmd->base.description),
             "Insert \"%s\"", text);
    
    cmd->buffer = buf;
    cmd->position = pos;
    cmd->text = strdup(text);
    cmd->text_len = strlen(text);
    
    return (Command*)cmd;
}

/* ============================================
 * Concrete Command: Delete Text
 * ============================================ */
typedef struct {
    Command base;
    TextBuffer* buffer;
    int position;
    int length;
    char* deleted_text;                   /* Saved for undo */
} DeleteCommand;

void delete_execute(Command* self)
{
    DeleteCommand* cmd = (DeleteCommand*)self;
    
    /* Save deleted text for undo */
    cmd->deleted_text = (char*)malloc(cmd->length + 1);
    strncpy(cmd->deleted_text, &cmd->buffer->text[cmd->position], cmd->length);
    cmd->deleted_text[cmd->length] = '\0';
    
    printf("  [Execute] Delete %d chars at position %d (\"%s\")\n",
           cmd->length, cmd->position, cmd->deleted_text);
    buffer_delete(cmd->buffer, cmd->position, cmd->length);
}

void delete_undo(Command* self)
{
    DeleteCommand* cmd = (DeleteCommand*)self;
    printf("  [Undo] Restore \"%s\" at position %d\n",
           cmd->deleted_text, cmd->position);
    buffer_insert(cmd->buffer, cmd->position, cmd->deleted_text);
}

void delete_destroy(Command* self)
{
    DeleteCommand* cmd = (DeleteCommand*)self;
    if (cmd->deleted_text) free(cmd->deleted_text);
    free(cmd);
}

Command* create_delete_command(TextBuffer* buf, int pos, int len)
{
    DeleteCommand* cmd = (DeleteCommand*)malloc(sizeof(DeleteCommand));
    if (cmd == NULL) return NULL;
    
    cmd->base.execute = delete_execute;
    cmd->base.undo = delete_undo;
    cmd->base.destroy = delete_destroy;
    snprintf(cmd->base.description, sizeof(cmd->base.description),
             "Delete %d chars", len);
    
    cmd->buffer = buf;
    cmd->position = pos;
    cmd->length = len;
    cmd->deleted_text = NULL;
    
    return (Command*)cmd;
}

/* ============================================
 * Concrete Command: Replace Text
 * ============================================ */
typedef struct {
    Command base;
    TextBuffer* buffer;
    int position;
    int old_length;
    char* old_text;
    char* new_text;
} ReplaceCommand;

void replace_execute(Command* self)
{
    ReplaceCommand* cmd = (ReplaceCommand*)self;
    
    /* Save old text for undo */
    cmd->old_text = (char*)malloc(cmd->old_length + 1);
    strncpy(cmd->old_text, &cmd->buffer->text[cmd->position], cmd->old_length);
    cmd->old_text[cmd->old_length] = '\0';
    
    printf("  [Execute] Replace \"%s\" with \"%s\"\n", cmd->old_text, cmd->new_text);
    buffer_delete(cmd->buffer, cmd->position, cmd->old_length);
    buffer_insert(cmd->buffer, cmd->position, cmd->new_text);
}

void replace_undo(Command* self)
{
    ReplaceCommand* cmd = (ReplaceCommand*)self;
    printf("  [Undo] Restore \"%s\"\n", cmd->old_text);
    buffer_delete(cmd->buffer, cmd->position, strlen(cmd->new_text));
    buffer_insert(cmd->buffer, cmd->position, cmd->old_text);
}

void replace_destroy(Command* self)
{
    ReplaceCommand* cmd = (ReplaceCommand*)self;
    if (cmd->old_text) free(cmd->old_text);
    if (cmd->new_text) free(cmd->new_text);
    free(cmd);
}

Command* create_replace_command(TextBuffer* buf, int pos, int len, const char* new_text)
{
    ReplaceCommand* cmd = (ReplaceCommand*)malloc(sizeof(ReplaceCommand));
    if (cmd == NULL) return NULL;
    
    cmd->base.execute = replace_execute;
    cmd->base.undo = replace_undo;
    cmd->base.destroy = replace_destroy;
    snprintf(cmd->base.description, sizeof(cmd->base.description), "Replace text");
    
    cmd->buffer = buf;
    cmd->position = pos;
    cmd->old_length = len;
    cmd->old_text = NULL;
    cmd->new_text = strdup(new_text);
    
    return (Command*)cmd;
}

/* ============================================
 * Invoker: Command Manager
 * ============================================ */
typedef struct {
    Command* history[MAX_HISTORY];        /* Command history */
    int history_count;                    /* Total commands in history */
    int current_index;                    /* Current position (-1 = no commands) */
} CommandManager;

void manager_init(CommandManager* mgr)
{
    memset(mgr->history, 0, sizeof(mgr->history));
    mgr->history_count = 0;
    mgr->current_index = -1;
}

/* Execute a command and add to history */
void manager_execute(CommandManager* mgr, Command* cmd)
{
    printf("\n[Manager] Executing: %s\n", cmd->description);
    
    /* Clear any commands after current position (for redo cleanup) */
    for (int i = mgr->current_index + 1; i < mgr->history_count; i++) {
        if (mgr->history[i]) {
            mgr->history[i]->destroy(mgr->history[i]);
            mgr->history[i] = NULL;
        }
    }
    
    /* Execute the command */
    cmd->execute(cmd);
    
    /* Add to history */
    mgr->current_index++;
    mgr->history[mgr->current_index] = cmd;
    mgr->history_count = mgr->current_index + 1;
    
    printf("[Manager] History: %d commands, current index: %d\n",
           mgr->history_count, mgr->current_index);
}

/* Undo last command */
int manager_undo(CommandManager* mgr)
{
    if (mgr->current_index < 0) {
        printf("\n[Manager] Nothing to undo!\n");
        return -1;
    }
    
    Command* cmd = mgr->history[mgr->current_index];
    printf("\n[Manager] Undo: %s\n", cmd->description);
    cmd->undo(cmd);
    mgr->current_index--;
    
    printf("[Manager] After undo, current index: %d\n", mgr->current_index);
    return 0;
}

/* Redo undone command */
int manager_redo(CommandManager* mgr)
{
    if (mgr->current_index >= mgr->history_count - 1) {
        printf("\n[Manager] Nothing to redo!\n");
        return -1;
    }
    
    mgr->current_index++;
    Command* cmd = mgr->history[mgr->current_index];
    printf("\n[Manager] Redo: %s\n", cmd->description);
    cmd->execute(cmd);
    
    printf("[Manager] After redo, current index: %d\n", mgr->current_index);
    return 0;
}

/* Print command history */
void manager_print_history(CommandManager* mgr)
{
    printf("\n=== Command History ===\n");
    for (int i = 0; i < mgr->history_count; i++) {
        printf("  [%d] %s %s\n", i, mgr->history[i]->description,
               (i == mgr->current_index) ? "<-- current" : "");
    }
    printf("=======================\n");
}

/* Cleanup */
void manager_destroy(CommandManager* mgr)
{
    for (int i = 0; i < mgr->history_count; i++) {
        if (mgr->history[i]) {
            mgr->history[i]->destroy(mgr->history[i]);
        }
    }
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    TextBuffer buffer;
    CommandManager manager;
    
    printf("=== Command Pattern Demo: Text Editor ===\n\n");
    
    /* Initialize */
    buffer_init(&buffer);
    manager_init(&manager);
    
    printf("Initial state:\n");
    buffer_print(&buffer);
    
    /* Execute some commands */
    printf("\n--- Executing Commands ---\n");
    
    manager_execute(&manager, create_insert_command(&buffer, 0, "Hello"));
    buffer_print(&buffer);
    
    manager_execute(&manager, create_insert_command(&buffer, 5, " World"));
    buffer_print(&buffer);
    
    manager_execute(&manager, create_insert_command(&buffer, 11, "!"));
    buffer_print(&buffer);
    
    manager_execute(&manager, create_replace_command(&buffer, 6, 5, "C Programming"));
    buffer_print(&buffer);
    
    manager_print_history(&manager);
    
    /* Undo operations */
    printf("\n--- Undo Operations ---\n");
    
    manager_undo(&manager);
    buffer_print(&buffer);
    
    manager_undo(&manager);
    buffer_print(&buffer);
    
    manager_undo(&manager);
    buffer_print(&buffer);
    
    manager_print_history(&manager);
    
    /* Redo operations */
    printf("\n--- Redo Operations ---\n");
    
    manager_redo(&manager);
    buffer_print(&buffer);
    
    manager_redo(&manager);
    buffer_print(&buffer);
    
    manager_print_history(&manager);
    
    /* Execute new command (clears redo history) */
    printf("\n--- New Command (clears redo) ---\n");
    
    manager_execute(&manager, create_delete_command(&buffer, 5, 6));
    buffer_print(&buffer);
    
    manager_print_history(&manager);
    
    /* Final undo */
    printf("\n--- Final Undo ---\n");
    manager_undo(&manager);
    buffer_print(&buffer);
    
    /* Cleanup */
    manager_destroy(&manager);
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了一个支持撤销/重做的文本编辑器：

1. **接收者（TextBuffer）**：
   - 实际存储文本的缓冲区
   - 提供 `insert`、`delete` 等基本操作

2. **命令实现**：
   - **InsertCommand**：插入文本，撤销时删除
   - **DeleteCommand**：删除文本，撤销时恢复
   - **ReplaceCommand**：替换文本，撤销时还原

3. **命令管理器（Invoker）**：
   - 维护命令历史数组
   - `execute()`：执行命令并记录历史
   - `undo()`：撤销当前命令
   - `redo()`：重做已撤销的命令

4. **撤销/重做机制**：
   - 每个命令保存足够的信息用于撤销
   - `current_index` 跟踪当前位置
   - 新命令会清除重做历史

