// 1. 【抽象步骤】定义函数指针结构体：封装所有可变的业务步骤
typedef struct {
    void (*step1)(void); // 可变步骤1
    int  (*step2)(int);  // 可变步骤2
    void (*step3)(void); // 可变步骤3
} XXX_Ops;

// 2. 【固定骨架】模板函数：固化执行流程，调用结构体的函数指针，永不修改
void template_workflow(const XXX_Ops *ops) {
    int ret;
    ops->step1();        // 执行步骤1
    ret = ops->step2(10); // 执行步骤2
    ops->step3();        // 执行步骤3
}

// 3. 【具体实现】编写业务细节函数
void my_step1(void) { /* 业务1实现 */ }
int  my_step2(int val) { /* 业务2实现 */ return val; }
void my_step3(void) { /* 业务3实现 */ }

// 4. 【绑定】实例化结构体，填充函数指针
const XXX_Ops my_ops = {.step1=my_step1, .step2=my_step2, .step3=my_step3};

// 5. 【调用】传入具体实现，执行固定流程
template_workflow(&my_ops);