#include <stdio.h>
#include <string.h>

// 定义支付金额类型
typedef unsigned int PayAmount;

/* 步骤 1：定义策略接口（函数指针类型） */
// 【策略接口】支付策略的函数指针类型（统一调用规范）
// 参数：支付金额、支付账号；返回值：0成功，-1失败
typedef int (*PayStrategy)(PayAmount amount, const char *account);

/* 步骤 2：实现具体策略（不同支付方式的算法） */
// ==== 具体策略1：微信支付 ====
int WechatPay(PayAmount amount, const char *account) {
    printf("[微信支付策略] 账号：%s，金额：%u元 → 调用微信支付接口\n", account, amount);
    // 模拟支付逻辑
    return 0; // 支付成功
}

// ==== 具体策略2：支付宝支付 ====
int Alipay(PayAmount amount, const char *account) {
    printf("[支付宝支付策略] 账号：%s，金额：%u元 → 调用支付宝支付接口\n", account, amount);
    // 模拟支付逻辑
    return 0; // 支付成功
}

// ==== 具体策略3：银联支付 ====
int UnionPay(PayAmount amount, const char *account) {
    printf("[银联支付策略] 账号：%s，金额：%u元 → 调用银联支付接口\n", account, amount);
    // 模拟支付逻辑
    return 0; // 支付成功
}

/* 步骤 3：步骤 3：定义上下文（封装策略 + 调用逻辑） */
// 【上下文】支付上下文结构体：管理当前选中的支付策略
typedef struct {
    PayStrategy current_strategy; // 当前使用的支付策略（函数指针）
} PayContext;

// 上下文方法1：设置/切换支付策略
void PayContext_SetStrategy(PayContext *ctx, PayStrategy strategy) {
    if (ctx == NULL || strategy == NULL) {
        printf("错误：上下文或策略为空！\n");
        return;
    }
    ctx->current_strategy = strategy;
    printf("已切换支付策略\n");
}

// 上下文方法2：执行支付（核心逻辑，无需修改）
int PayContext_ExecutePay(PayContext *ctx, PayAmount amount, const char *account) {
    if (ctx == NULL || ctx->current_strategy == NULL) {
        printf("错误：未设置支付策略！\n");
        return -1;
    }
    printf("===== 开始执行支付 =====\n");
    // 调用当前选中的策略（算法），客户端无需知道具体实现
    int ret = ctx->current_strategy(amount, account);
    if (ret == 0) {
        printf("支付完成 ✅\n");
    } else {
        printf("支付失败 ❌\n");
    }
    printf("===== 支付流程结束 =====\n\n");
    return ret;
}

/* 步骤 4：客户端调用（动态切换策略）*/
int main(void) {
    // 1. 创建支付上下文
    PayContext pay_ctx = {.current_strategy = NULL};
    
    // 2. 动态切换为微信支付策略，执行支付
    PayContext_SetStrategy(&pay_ctx, WechatPay);
    PayContext_ExecutePay(&pay_ctx, 100, "wx12345678");
    
    // 3. 动态切换为支付宝策略，执行支付
    PayContext_SetStrategy(&pay_ctx, Alipay);
    PayContext_ExecutePay(&pay_ctx, 200, "ali87654321");
    
    // 4. 动态切换为银联策略，执行支付
    PayContext_SetStrategy(&pay_ctx, UnionPay);
    PayContext_ExecutePay(&pay_ctx, 300, "union11223344");
    
    // 【扩展】新增支付方式（比如数字人民币）：
    // 只需新增一个函数，无需修改上下文和现有逻辑
    // int DigitalRMB(PayAmount amount, const char *account) { ... }
    // PayContext_SetStrategy(&pay_ctx, DigitalRMB);
    // PayContext_ExecutePay(&pay_ctx, 400, "drmb998877");
    
    return 0;
}
