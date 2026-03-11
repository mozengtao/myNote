## macro + struct + function pointer
- 基本思想
  -  macros reduce repetition
  -  structs bundle data
  -  function pointers enable runtime polymorphism

- 典型应用
  - 面向对象模拟
  - 状态机实现
  - 回调机制与事件系统
  - 泛型容器实现
  - 策略模式
  - 插件系统
  - plug-in registration
  - state machine
  - state machine

- 面向对象模拟
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 基类定义
typedef struct Animal {
	char name[50];
	int age;
	void (*speak)(struct Animal* self);
	void (*destroy)(struct Animal* self);
} Animal;

// 宏定义简化对象创建
#define ANIMAL_INIT(n, a) {.name = n, .age = a}

// 方法实现
void animal_speak(Animal* self) {
	printf("%s (age %d) makes a sound\n", self->name, self->age);
}

void animal_destroy(Animal* self) {
	printf("Destroying animal: %s\n", self->name);
	free(self);
}

// 派生类 - Dog
typedef struct {
	Animal base;
	char breed[50];
} Dog;

void dog_speak(Animal* self) {
	Dog* dog = (Dog*)self;
	printf("%s (a %s, age %d) says: Woof!\n", 
		   dog->base.name, dog->breed, dog->base.age);
}

Dog* dog_create(const char* name, int age, const char* breed) {
	Dog* dog = malloc(sizeof(Dog));
	strcpy(dog->base.name, name);
	dog->base.age = age;
	strcpy(dog->breed, breed);
	dog->base.speak = dog_speak;
	dog->base.destroy = (void (*)(Animal*))free;
	return dog;
}

// 派生类 - Cat
typedef struct {
	Animal base;
	int lives;
} Cat;

void cat_speak(Animal* self) {
	Cat* cat = (Cat*)self;
	printf("%s (lives: %d, age %d) says: Meow!\n", 
		   cat->base.name, cat->lives, cat->base.age);
}

Cat* cat_create(const char* name, int age, int lives) {
	Cat* cat = malloc(sizeof(Cat));
	strcpy(cat->base.name, name);
	cat->base.age = age;
	cat->lives = lives;
	cat->base.speak = cat_speak;
	cat->base.destroy = (void (*)(Animal*))free;
	return cat;
}

int main() {
	// 多态演示
	Animal* animals[3];
		
	animals[0] = (Animal*)dog_create("Buddy", 3, "Golden Retriever");
	animals[1] = (Animal*)cat_create("Whiskers", 2, 9);
	animals[2] = (Animal*)dog_create("Max", 5, "Labrador");
		
	printf("=== Animal Sounds ===\n");
	for (int i = 0; i < 3; i++) {
		animals[i]->speak(animals[i]);
	}
		
	// 清理
	for (int i = 0; i < 3; i++) {
		animals[i]->destroy(animals[i]);
	}
		
	return 0;
}
```
- 状态机实现
```c
#include <stdio.h>
#include <stdlib.h>

// 状态和事件定义
typedef enum {
	STATE_IDLE,
	STATE_RUNNING,
	STATE_PAUSED,
	STATE_STOPPED
} State;

typedef enum {
	EVENT_START,
	EVENT_PAUSE,
	EVENT_RESUME,
	EVENT_STOP
} Event;

// 状态机结构
typedef struct StateMachine {
	State current_state;
	void (*transition)(struct StateMachine*, Event);
	void (*on_enter[4])(void);  // 进入状态回调
	void (*on_exit[4])(void);   // 离开状态回调
} StateMachine;

// 状态进入/退出回调
void on_idle_enter() { printf("Entering Idle state\n"); }
void on_running_enter() { printf("Entering Running state\n"); }
void on_paused_enter() { printf("Entering Paused state\n"); }
void on_stopped_enter() { printf("Entering Stopped state\n"); }

void on_idle_exit() { printf("Leaving Idle state\n"); }
void on_running_exit() { printf("Leaving Running state\n"); }
void on_paused_exit() { printf("Leaving Paused state\n"); }
void on_stopped_exit() { printf("Leaving Stopped state\n"); }

// 状态转换函数
void state_transition(StateMachine* sm, Event event) {
	State old_state = sm->current_state;
	State new_state = old_state;
		
	// 状态转换逻辑
	switch (old_state) {
		case STATE_IDLE:
			if (event == EVENT_START) new_state = STATE_RUNNING;
			break;
		case STATE_RUNNING:
			if (event == EVENT_PAUSE) new_state = STATE_PAUSED;
			else if (event == EVENT_STOP) new_state = STATE_STOPPED;
			break;
		case STATE_PAUSED:
			if (event == EVENT_RESUME) new_state = STATE_RUNNING;
			else if (event == EVENT_STOP) new_state = STATE_STOPPED;
			break;
		case STATE_STOPPED:
			if (event == EVENT_START) new_state = STATE_RUNNING;
			break;
	}
		
	// 执行状态转换
	if (old_state != new_state) {
		if (sm->on_exit[old_state]) sm->on_exit[old_state]();
		sm->current_state = new_state;
		if (sm->on_enter[new_state]) sm->on_enter[new_state]();
	}
}

// 创建状态机的宏
#define CREATE_STATE_MACHINE() &(StateMachine){ \
	.current_state = STATE_IDLE, \
	.transition = state_transition, \
	.on_enter = {on_idle_enter, on_running_enter, on_paused_enter, on_stopped_enter}, \
	.on_exit = {on_idle_exit, on_running_exit, on_paused_exit, on_stopped_exit} \
}

int main() {
	StateMachine* sm = CREATE_STATE_MACHINE();
		
	printf("=== State Machine Demo ===\n");
		
	// 测试状态转换
	sm->transition(sm, EVENT_START);   // IDLE -> RUNNING
	sm->transition(sm, EVENT_PAUSE);	// RUNNING -> PAUSED
	sm->transition(sm, EVENT_RESUME);   // PAUSED -> RUNNING
	sm->transition(sm, EVENT_STOP);	 // RUNNING -> STOPPED
	sm->transition(sm, EVENT_START);	// STOPPED -> RUNNING
		
	//free(sm);
	return 0;
}
```
- 回调机制与事件系统
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 事件类型定义
typedef enum {
	EVENT_BUTTON_PRESS,
	EVENT_TIMER_EXPIRED,
	EVENT_DATA_RECEIVED
} EventType;

// 事件结构
typedef struct {
	EventType type;
	void* data;
	size_t data_size;
} Event;

// 事件回调函数类型
typedef void (*EventHandler)(Event* event);

// 事件管理器
typedef struct {
	EventHandler* handlers;
	int capacity;
	int count;
} EventManager;

// 创建事件管理器
EventManager* event_manager_create(int capacity) {
	EventManager* em = malloc(sizeof(EventManager));
	em->handlers = malloc(sizeof(EventHandler) * capacity);
	em->capacity = capacity;
	em->count = 0;
	return em;
}

// 注册事件处理器
#define EVENT_REGISTER(em, handler) \
	do { \
		if ((em)->count < (em)->capacity) { \
			(em)->handlers[(em)->count++] = handler; \
		} \
	} while(0)

// 触发事件
void event_trigger(EventManager* em, Event* event) {
	printf("Triggering event %d\n", event->type);
	for (int i = 0; i < em->count; i++) {
		em->handlers[i](event);
	}
}

// 具体的事件处理器
void button_press_handler(Event* event) {
	if (event->type == EVENT_BUTTON_PRESS) {
		char* button = (char*)event->data;
		printf("Button pressed: %s\n", button);
	}
}

void timer_handler(Event* event) {
	if (event->type == EVENT_TIMER_EXPIRED) {
		int* timer_id = (int*)event->data;
		printf("Timer %d expired\n", *timer_id);
	}
}

void data_handler(Event* event) {
	if (event->type == EVENT_DATA_RECEIVED) {
		char* data = (char*)event->data;
		printf("Data received: %s\n", data);
	}
}

// 日志中间件
void logging_middleware(Event* event) {
	printf("[LOG] Event %d processed\n", event->type);
}

int main() {
	EventManager* em = event_manager_create(10);

	// 注册事件处理器
	EVENT_REGISTER(em, button_press_handler);
	EVENT_REGISTER(em, timer_handler);
	EVENT_REGISTER(em, data_handler);
	EVENT_REGISTER(em, logging_middleware);  // 日志中间件

	printf("=== Event System Demo ===\n");

	// 创建并触发事件
	Event e1 = {EVENT_BUTTON_PRESS, "OK", 0};
	event_trigger(em, &e1);

	int timer_id = 123;
	Event e2 = {EVENT_TIMER_EXPIRED, &timer_id, sizeof(int)};
	event_trigger(em, &e2);

	Event e3 = {EVENT_DATA_RECEIVED, "Hello World", 0};
	event_trigger(em, &e3);

	free(em->handlers);
	free(em);
	return 0;
}
```
- 泛型容器实现
```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 通用链表节点
typedef struct ListNode {
	void* data;
	struct ListNode* next;
	void (*print_func)(void* data);
	int (*compare_func)(void* a, void* b);
	void (*free_func)(void* data);
} ListNode;

// 链表结构
typedef struct {
	ListNode* head;
	int size;
} LinkedList;

// 类型特定的函数
void int_print(void* data) {
	printf("%d", *(int*)data);
}

void string_print(void* data) {
	printf("%s", (char*)data);
}

int int_compare(void* a, void* b) {
	return *(int*)a - *(int*)b;
}

int string_compare(void* a, void* b) {
	return strcmp((char*)a, (char*)b);
}

void basic_free(void* data) {
	free(data);
}

// 创建节点的辅助函数（在堆上分配，避免宏参数与成员名冲突）
static ListNode* create_node(
	void* payload,
	void (*print_fn)(void*),
	int (*compare_fn)(void*, void*),
	void (*free_fn)(void*)
) {
	ListNode* node = (ListNode*)malloc(sizeof(ListNode));
	if (!node) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	node->data = payload;
	node->print_func = print_fn;
	node->compare_func = compare_fn;
	node->free_func = free_fn;
	node->next = NULL;
	return node;
}

// 宏包装，保持调用简洁
#define CREATE_NODE(payload, print_fn, compare_fn, free_fn) \
	create_node((payload), (print_fn), (compare_fn), (free_fn))

// 链表操作
void list_append(LinkedList* list, ListNode* node) {
	if (!list->head) {
		list->head = node;
	} else {
		ListNode* current = list->head;
		while (current->next) {
			current = current->next;
		}
		current->next = node;
	}
	list->size++;
}

void list_print(LinkedList* list) {
	ListNode* current = list->head;
	printf("List[%d]: ", list->size);
	while (current) {
		current->print_func(current->data);
		printf(" -> ");
		current = current->next;
	}
	printf("NULL\n");
}

void list_free(LinkedList* list) {
	ListNode* current = list->head;
	while (current) {
		ListNode* next = current->next;
		if (current->free_func) {
			current->free_func(current->data);
		}
		free(current);
		current = next;
	}
}

int main() {
	LinkedList int_list = {0};
	LinkedList string_list = {0};

	printf("=== Generic Container Demo ===\n");

	// 整数链表
	int* num1 = malloc(sizeof(int)); *num1 = 10;
	int* num2 = malloc(sizeof(int)); *num2 = 20;
	int* num3 = malloc(sizeof(int)); *num3 = 30;

	list_append(&int_list, CREATE_NODE(num1, int_print, int_compare, basic_free));
	list_append(&int_list, CREATE_NODE(num2, int_print, int_compare, basic_free));
	list_append(&int_list, CREATE_NODE(num3, int_print, int_compare, basic_free));

	list_print(&int_list);

	// 字符串链表
	char* str1 = strdup("Hello");
	char* str2 = strdup("World");
	char* str3 = strdup("!");

	list_append(&string_list, CREATE_NODE(str1, string_print, string_compare, basic_free));
	list_append(&string_list, CREATE_NODE(str2, string_print, string_compare, basic_free));
	list_append(&string_list, CREATE_NODE(str3, string_print, string_compare, basic_free));

	list_print(&string_list);

	// 清理
	list_free(&int_list);
	list_free(&string_list);

	return 0;
}
```
- 策略模式
```c
#include <stdio.h>
#include <stdlib.h>

// 排序策略接口
typedef struct {
	char name[50];
	void (*sort)(int* array, int size);
	int (*compare)(int a, int b);  // 比较策略
} SortStrategy;

// 具体的排序算法
void bubble_sort(int* array, int size) {
	for (int i = 0; i < size - 1; i++) {
		for (int j = 0; j < size - i - 1; j++) {
			if (array[j] > array[j + 1]) {
				int temp = array[j];
				array[j] = array[j + 1];
				array[j + 1] = temp;
			}
		}
	}
}

void selection_sort(int* array, int size) {
	for (int i = 0; i < size - 1; i++) {
		int min_idx = i;
		for (int j = i + 1; j < size; j++) {
			if (array[j] < array[min_idx]) {
				min_idx = j;
			}
		}
		int temp = array[i];
		array[i] = array[min_idx];
		array[min_idx] = temp;
	}
}

// 比较策略
int ascending(int a, int b) { return a - b; }
int descending(int a, int b) { return b - a; }

// 策略实例
SortStrategy bubble_ascending = {"Bubble Sort (Ascending)", bubble_sort, ascending};
SortStrategy bubble_descending = {"Bubble Sort (Descending)", bubble_sort, descending};
SortStrategy selection_ascending = {"Selection Sort (Ascending)", selection_sort, ascending};

// 排序上下文
typedef struct {
	SortStrategy* strategy;
	int* data;
	int size;
} SortContext;

// 设置策略的宏
#define SET_STRATEGY(ctx, s) do { (ctx)->strategy = s; } while(0)

void sort_context_execute(SortContext* ctx) {
	printf("Using strategy: %s\n", ctx->strategy->name);

	// 使用策略的比较函数进行排序
	for (int i = 0; i < ctx->size - 1; i++) {
		for (int j = 0; j < ctx->size - i - 1; j++) {
			if (ctx->strategy->compare(ctx->data[j], ctx->data[j + 1]) > 0) {
				int temp = ctx->data[j];
				ctx->data[j] = ctx->data[j + 1];
				ctx->data[j + 1] = temp;
			}
		}
	}
}

void print_array(int* array, int size) {
	for (int i = 0; i < size; i++) {
		printf("%d ", array[i]);
	}
	printf("\n");
}

int main() {
	int data[] = {64, 34, 25, 12, 22, 11, 90};
	int size = sizeof(data) / sizeof(data[0]);

	SortContext ctx = {0};
	ctx.data = data;
	ctx.size = size;

	printf("=== Strategy Pattern Demo ===\n");
	printf("Original array: ");
	print_array(data, size);

	// 测试不同策略
	SET_STRATEGY(&ctx, &bubble_ascending);
	sort_context_execute(&ctx);
	printf("Sorted array: ");
	print_array(data, size);

	// 重置数据
	int data2[] = {64, 34, 25, 12, 22, 11, 90};
	ctx.data = data2;

	SET_STRATEGY(&ctx, &bubble_descending);
	sort_context_execute(&ctx);
	printf("Sorted array (descending): ");
	print_array(data2, size);

	return 0;
}
```
- 插件系统
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 插件接口
typedef struct {
	char name[50];
	int version;
	int (*initialize)(void);
	int (*process)(const char* input, char* output, int output_size);
	void (*shutdown)(void);
} Plugin;

// 插件管理器
typedef struct {
	Plugin* plugins[10];
	int count;
} PluginManager;

// 注册插件的宏
#define PLUGIN_REGISTER(pm, p) \
	do { \
		if ((pm)->count < 10) { \
			(pm)->plugins[(pm)->count++] = p; \
		} \
	} while(0)

// 具体插件实现
int logger_init(void) {
	printf("Logger plugin initialized\n");
	return 0;
}

int logger_process(const char* input, char* output, int output_size) {
	snprintf(output, output_size, "[LOG] %s", input);
	return 0;
}

void logger_shutdown(void) {
	printf("Logger plugin shutdown\n");
}

Plugin logger_plugin = {
	.name = "Logger",
	.version = 1,
	.initialize = logger_init,
	.process = logger_process,
	.shutdown = logger_shutdown
};

// 加密插件
int encryptor_init(void) {
	printf("Encryptor plugin initialized\n");
	return 0;
}

int encryptor_process(const char* input, char* output, int output_size) {
	// 简单的"加密" - 字符偏移
	int len = strlen(input);
	for (int i = 0; i < len && i < output_size - 1; i++) {
		output[i] = input[i] + 1;  // 每个字符+1
	}
	output[len < output_size - 1 ? len : output_size - 1] = '\0';
	return 0;
}

void encryptor_shutdown(void) {
	printf("Encryptor plugin shutdown\n");
}

Plugin encryptor_plugin = {
	.name = "Encryptor",
	.version = 1,
	.initialize = encryptor_init,
	.process = encryptor_process,
	.shutdown = encryptor_shutdown
};

void plugin_manager_init(PluginManager* pm) {
	pm->count = 0;
}

void plugin_manager_start(PluginManager* pm) {
	printf("=== Starting %d plugins ===\n", pm->count);
	for (int i = 0; i < pm->count; i++) {
		pm->plugins[i]->initialize();
	}
}

void plugin_manager_process(PluginManager* pm, const char* input) {
	char output[256];

	for (int i = 0; i < pm->count; i++) {
		pm->plugins[i]->process(input, output, sizeof(output));
		printf("Plugin '%s' output: %s\n", pm->plugins[i]->name, output);
	}
}

void plugin_manager_shutdown(PluginManager* pm) {
	printf("=== Shutting down %d plugins ===\n", pm->count);
	for (int i = 0; i < pm->count; i++) {
		pm->plugins[i]->shutdown();
	}
}

int main() {
	PluginManager pm;
	plugin_manager_init(&pm);

	// 注册插件
	PLUGIN_REGISTER(&pm, &logger_plugin);
	PLUGIN_REGISTER(&pm, &encryptor_plugin);

	// 使用插件系统
	plugin_manager_start(&pm);

	printf("\n=== Processing Data ===\n");
	plugin_manager_process(&pm, "Hello, Plugin System!");
	plugin_manager_process(&pm, "Test Message");

	plugin_manager_shutdown(&pm);

	return 0;
}
```

- plug-in registration
```c
#include <stdio.h>

typedef struct plugin
{
	const char *name;
	void (*init)(void);
	void (*run)(void);
} plugin;

#define PLUGIN_DECL(NAME, INIT_FN, RUN_FN) \
	plugin NAME = {#NAME, INIT_FN, RUN_FN}

static void alpha_init(void) { puts("alpha init"); }
static void alpha_run(void) { puts("alpha run"); }
PLUGIN_DECL(alpha_plugin, alpha_init, alpha_run);

static void beta_init(void) { puts("beta init"); }
static void beta_run(void) { puts("beta run"); }
PLUGIN_DECL(beta_plugin, beta_init, beta_run);

static plugin *registry[] = {&alpha_plugin, &beta_plugin};

int main(void)
{
	for (size_t i = 0; i < sizeof registry / sizeof registry[0]; ++i)
	{
		registry[i]->init();
		registry[i]->run();
	}
	return 0;
}
```
- state machine
```c
#include <stdio.h>

typedef struct state state;
struct state
{
	const char *name;
	void (*on_enter)(void);
	const state *(*next)(void);
};

#define STATE(NAME)                                               \
	static void NAME##_enter(void);                               \
	static const state *NAME##_next(void);                        \
	static const state NAME = {#NAME, NAME##_enter, NAME##_next}; \
	static void NAME##_enter(void)
#define STATE_NEXT(NAME) static const state *NAME##_next(void)

STATE(idle) { puts("enter idle"); }
STATE(running) { puts("enter running"); }

STATE_NEXT(idle) { return &running; }
STATE_NEXT(running) { return &idle; }

int main(void)
{
	const state *current = &idle;
	for (int i = 0; i < 4; ++i)
	{
		current->on_enter();
		current = current->next();
	}
	return 0;
}
```
- command dispather
```c
#include <stdio.h>
#include <string.h>

typedef void (*command_fn)(const char *);

typedef struct command
{
	const char *opcode;
	command_fn handler;
} command;

#define CMD(OPCODE, FN) {OPCODE, FN}

static void handle_status(const char *payload)
{
	printf("status: %s\n", payload);
}

static void handle_reset(const char *payload)
{
	printf("reset issued by %s\n", payload);
}

static command table[] = {
	CMD("STATUS", handle_status),
	CMD("RESET", handle_reset)};

static command_fn lookup(const char *opcode)
{
	for (size_t i = 0; i < sizeof table / sizeof table[0]; ++i)
	{
		if (strcmp(table[i].opcode, opcode) == 0)
		{
			return table[i].handler;
		}
	}
	return NULL;
}

int main(void)
{
	struct
	{
		const char *opcode;
		const char *payload;
	} script[] = {
		{"STATUS", "worker-42"},
		{"RESET", "admin"}
	};

	for (size_t i = 0; i < sizeof script / sizeof script[0]; ++i)
	{
		command_fn fn = lookup(script[i].opcode);
		if (fn)
			fn(script[i].payload);
		else
			printf("unknown opcode: %s\n", script[i].opcode);
	}
	return 0;
}
```

- Example: stats handler for several subsystems
- file list
```
.
├── stats_handler1.c
├── stats_handler1.h
├── stats_handler2.c
├── stats_handler2.h
├── stats_handler.c
└── stats_level.h
```
- stats_level.h
```c
#ifndef STATS_LEVEL_H
#define STATS_LEVEL_H

enum stats_level {
	STATS_LEVEL_INFO = 0,
	STATS_LEVEL_DEBUG = 1
};

#endif
```
- stats_handler1.h
```c
#ifndef STATS_HANDLER1_H
#define STATS_HANDLER1_H

#include "stats_level.h"

void stats_handler1_init(const char *app);
void stats_handler1_free(void);
void stats_handler1_update(void);
void stats_handler1_print(enum stats_level level);
const char *stats_handler1_data_provider_get(void);

#endif
```
- stats_handler2.h
```c
#ifndef STATS_HANDLER2_H
#define STATS_HANDLER2_H

#include "stats_level.h"

void stats_handler2_init(const char *app);
void stats_handler2_free(void);
void stats_handler2_update(void);
void stats_handler2_print(enum stats_level level);
const char *stats_handler2_data_provider_get(void);

#endif
```
- stats_handler.c
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "stats_level.h"
#include "stats_handler1.h"
#include "stats_handler2.h"


typedef void (*stats_handler_ops_init)(const char *app);
typedef void (*stats_handler_ops_free)(void);
typedef void (*stats_handler_ops_update)(void);
typedef void (*stats_handler_ops_print)(enum stats_level);
typedef const char* (*stats_handler_ops_data_provider_get)(void);

struct stats_handler_ops {
	stats_handler_ops_init init;
	stats_handler_ops_free free;
	stats_handler_ops_update update;
	stats_handler_ops_print print;
	stats_handler_ops_data_provider_get data_provider_get;
};

static int app_common_enabled_check(const char *name)
{
	if (strcmp(name, "stats_handler1") == 0 ||
		strcmp(name, "stats_handler2") == 0) {
		return 1;
	}

	return 0;
}

/* Dummy "app" just needs to be non-NULL to pass the macro's check */
static int dummy_app_instance = 1;
static void *app = &dummy_app_instance;

/* Macro modeled after your code */
#define IF_PROVIDER_REGISTERED(component, operation)     \
do {                                                     \
	if (app) {                                           \
		const char *o = component.data_provider_get();   \
		if (o) {                                         \
			char *owner = strdup(o);                     \
			uint8_t enabled = 0;                         \
			char *pch, *sp;                              \
			pch = strtok_r(owner, "|", &sp);             \
			while (pch != NULL) {                        \
				if (app_common_enabled_check(pch)) {     \
					enabled = 1;                         \
					break;                               \
				}                                        \
				pch = strtok_r(NULL, "|", &sp);          \
			}                                            \
			if (enabled) {                               \
				component.operation;                     \
			}                                            \
			free(owner);                                 \
		}                                                \
	}                                                    \
}                                                        \
while (0)

static struct stats_handler_ops stats_handler1_ops = {
		.init = stats_handler1_init,
		.free = stats_handler1_free,
		.update = stats_handler1_update,
		.print = stats_handler1_print,
		.data_provider_get = stats_handler1_data_provider_get
};

static struct stats_handler_ops stats_handler2_ops = {
		.init = stats_handler2_init,
		.free = stats_handler2_free,
		.update = stats_handler2_update,
		.print = stats_handler2_print,
		.data_provider_get = stats_handler2_data_provider_get
};

static int stats_enabled = 1;

static void stats_handler_all_init(const char *app)
{
	if (stats_enabled) {
		IF_PROVIDER_REGISTERED(stats_handler1_ops, init(app));
		IF_PROVIDER_REGISTERED(stats_handler2_ops, init(app));
	}
}

static void stats_handler_all_free(void)
{
	if (stats_enabled) {
		IF_PROVIDER_REGISTERED(stats_handler1_ops, free());
		IF_PROVIDER_REGISTERED(stats_handler2_ops, free());
	}
}

static void stats_handler_all_update(void)
{
	if (stats_enabled) {
		IF_PROVIDER_REGISTERED(stats_handler1_ops, update());
		IF_PROVIDER_REGISTERED(stats_handler2_ops, update());
	}
}

static void stats_handler_all_print(enum stats_level level)
{
	if (stats_enabled) {
		IF_PROVIDER_REGISTERED(stats_handler1_ops, print(level));
		IF_PROVIDER_REGISTERED(stats_handler2_ops, print(level));
	}
}

int main(void)
{
	stats_handler_all_init("app1");
	stats_handler_all_update();
	stats_handler_all_print(STATS_LEVEL_INFO);
	stats_handler_all_free();

	return 0;
}
```
- stats_handler1.c
```c
#include <stdio.h>
#include "stats_level.h"

void stats_handler1_init(const char *app)
{
	printf("stats_handler1_init called (app=%s)\n", app);
}

void stats_handler1_free(void)
{
	printf("stats_handler1_free called\n");
}

void stats_handler1_update(void)
{
	printf("stats_handler1_update called\n");
}

void stats_handler1_print(enum stats_level level)
{
	printf("stats_handler1_print called (level=%d)\n", level);
}

const char *stats_handler1_data_provider_get(void)
{
	return "stats_handler1";
}
```
- stats_handler2.c
```c
#include <stdio.h>
#include "stats_level.h"

void stats_handler2_init(const char *app)
{
	printf("stats_handler2_init called (app=%s)\n", app);
}

void stats_handler2_free(void)
{
	printf("stats_handler2_free called\n");
}

void stats_handler2_update(void)
{
	printf("stats_handler2_update called\n");
}

void stats_handler2_print(enum stats_level level)
{
	printf("stats_handler2_print called (level=%d)\n", level);
}

const char *stats_handler2_data_provider_get(void)
{
	return "stats_handler2";
}
```