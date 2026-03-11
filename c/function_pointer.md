## 函数指针的应用场景
## 1 回调机制
```c
#include <stdio.h>

typedef struct Button {
    void (*onClick)();  // 回调函数指针
} Button;

void buttonClicked() {
    printf("Button was clicked!\n");
}

int main() {
    Button btn;
    btn.onClick = buttonClicked;  // 注册回调函数
    if (btn.onClick) {
        btn.onClick();  // 调用回调函数
    }
    return 0;
}

// callback function
int compare(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

qsort(array, size, sizeof(int), compare);
```

## 2 策略模式
```c
#include <stdio.h>

typedef struct Sorter {
    int (*compare)(int, int);  // 函数指针定义了比较策略
} Sorter;

int ascending(int a, int b) {	// 比较策略 1
    return a - b;
}

int descending(int a, int b) {	// 比较策略 2
    return b - a;
}

void sort(int arr[], int n, Sorter sorter) {	// 动态指定比较策略
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (sorter.compare(arr[j], arr[j + 1]) > 0) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

int main() {
    int arr[] = {5, 3, 7, 2, 8};
    Sorter ascSorter = {ascending};
    Sorter descSorter = {descending};
    
    sort(arr, 5, ascSorter);	// 动态指定比较策略 1
    printf("Ascending: ");
    for (int i = 0; i < 5; i++) printf("%d ", arr[i]);
    printf("\n");
    
    sort(arr, 5, descSorter);	// 动态指定比较策略 2
    printf("Descending: ");
    for (int i = 0; i < 5; i++) printf("%d ", arr[i]);
    printf("\n");
    
    return 0;
}
```

## 3 插件系统
```c
#include <stdio.h>

typedef struct Filter {
    void (*apply)(int*, int);  // 函数指针指向具体的处理函数
} Filter;

void grayscale(int* pixels, int size) {
    printf("Applying grayscale filter\n");
}

void sepia(int* pixels, int size) {
    printf("Applying sepia filter\n");
}

int main() {
    int pixels[100];
    Filter filters[] = {	// 数组存储多个Filter对象，每个对象关联一个Filter函数
        {grayscale},
        {sepia}
    };
    
    for (int i = 0; i < 2; i++) {		// 遍历数组并调用apply，实现插件式的Filter应用
        filters[i].apply(pixels, 100);
    }
    return 0;
}
```

## 4 状态机
```c
#include <stdio.h>

typedef struct State {
    void (*handleInput)(char);  // 函数指针处理特定状态下的输入
} State;

void handleListen(char input) {
    if (input == 'S') printf("Received SYN, moving to SYN_RECEIVED\n");
}

void handleSynReceived(char input) {
    if (input == 'A') printf("Received ACK, moving to ESTABLISHED\n");
}

int main() {
    State states[] = {		  // 每个State（如LISTEN和SYN_RECEIVED）有独立的处理逻辑
        {handleListen},
        {handleSynReceived}
    };
    
    states[0].handleInput('S');  // 状态机在不同状态间的转换
    states[1].handleInput('A');  // 状态机在不同状态间的转换
    return 0;
}

//
#include <stdio.h>

typedef enum {
    STATE_RED,
    STATE_GREEN,
    STATE_YELLOW,
    STATE_COUNT
} TrafficLightState;

typedef TrafficLightState (*StateFunction)();

TrafficLightState red_state() {
    printf("State: RED\n");
    return STATE_GREEN;
}

TrafficLightState green_state() {
    printf("State: GREEN\n");
    return STATE_YELLOW;
}

TrafficLightState yellow_state() {
    printf("State: YELLOW\n");
    return STATE_RED;
}

int main() {
    StateFunction state_table[STATE_COUNT] = {
        red_state,
        green_state,
        yellow_state
    };

    TrafficLightState current_state = STATE_RED;

    for (int i = 0; i < 10; ++i) {
        current_state = state_table[current_state]();
    }

    return 0;
}
```

## 5 对象模拟
```c
#include <stdio.h>

typedef struct Animal {
    void (*speak)();  // 函数指针模拟类的“方法”
} Animal;

void dogSpeak() {		// 不同的函数指针实现各自的行为
    printf("Woof!\n");
}

void catSpeak() {		// 不同的函数指针实现各自的行为
    printf("Meow!\n");
}

int main() {
    Animal dog = {dogSpeak};
    Animal cat = {catSpeak};
    
    dog.speak();
    cat.speak();
    return 0;
}
```

## 6 事件分发
```c
#include <stdio.h>

typedef struct EventHandler {
    void (*handle)(int eventType);  // 函数指针用于事件处理
} EventHandler;

void handleEvent(int eventType) {	// 根据eventType执行不同逻辑
    if (eventType == 1) {
        printf("Handling event type 1\n");
    } else if (eventType == 2) {
        printf("Handling event type 2\n");
    }
}

int main() {
    EventHandler handler = {handleEvent};
    
    handler.handle(1);	// 分发事件，实现集中式事件处理
    handler.handle(2);	// 分发事件，实现集中式事件处理
    return 0;
}
```

## 7 线程池
```c
#include <stdio.h>

typedef struct Task {			// 结构体包含execute函数指针和参数arg
    void (*execute)(void* arg);
    void* arg;
} Task;

void printNumber(void* arg) {	// 任务函数，接收参数并执行
    int num = *(int*)arg;
    printf("Number: %d\n", num);
}

int main() {
    int num = 42;
    Task task = {printNumber, &num};
    
    task.execute(task.arg);  // 执行线程池中的任务
    return 0;
}
```

## 8 虚拟函数表
```c
#include <stdio.h>

typedef struct VTable {		// 虚函数表定义(包含函数指针)
    void (*speak)();
} VTable;

typedef struct Animal {		// 结构体通过函数表指针关联具体的实现
    VTable* vtable;
} Animal;

void dogSpeak() {
    printf("Woof!\n");
}

void catSpeak() {
    printf("Meow!\n");
}

VTable dogVTable = {dogSpeak};
VTable catVTable = {catSpeak};

int main() {
    Animal dog = {&dogVTable};	// 通过不同的虚函数表实现多态调用
    Animal cat = {&catVTable};	// 通过不同的虚函数表实现多态调用
    
    dog.vtable->speak();
    cat.vtable->speak();
    return 0;
}
```

## 9 钩子函数
```c
#include <stdio.h>

typedef struct Monitor {				// 结构体中的函数指针注册钩子函数
    void (*onChange)(const char* file);
} Monitor;

void logChange(const char* file) {		// 钩子函数实现
    printf("File changed: %s\n", file);
}

int main() {
    Monitor monitor = {logChange};
    
    if (monitor.onChange) {
        monitor.onChange("example.txt");  // 触发钩子函数逻辑
    }
    return 0;
}
```

## 10 从动态库加载函数
```c
#include <stdio.h>
#include <dlfcn.h>

typedef struct Plugin {
    void (*init)();
    void (*cleanup)();
} Plugin;

int main() {
    void* handle = dlopen("./libplugin.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    
    Plugin plugin;
    plugin.init = dlsym(handle, "plugin_init");			// dlsym获取函数地址
    plugin.cleanup = dlsym(handle, "plugin_cleanup");	// dlsym获取函数地址
    
    if (plugin.init)
		plugin.init();

    if (plugin.cleanup)
		plugin.cleanup();
    
    dlclose(handle);
    return 0;
}

// jump tables
void add() { printf("Add\n"); }
void subtract() { printf("Subtract\n"); }

void (*operation_table[2])() = { add, subtract };
operation_table[op_index]();  // Dynamically invoke the correct operation

// Plugin-like Architecture / Dynamic Behavior
typedef struct {
    void (*init)();
    void (*process)();
} Module;

void mod1_init() { printf("Mod1 Init\n"); }
void mod1_process() { printf("Mod1 Process\n"); }

Module mod1 = { mod1_init, mod1_process };
mod1.init();
mod1.process();

// Encapsulating Behavior in Data Structures
typedef struct {
    void (*draw)(void*);
} Shape;

void draw_circle(void* s) { printf("Drawing Circle\n"); }

Shape circle = { draw_circle };
circle.draw(&circle);

```