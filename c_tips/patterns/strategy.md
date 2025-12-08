# Strategy Pattern (策略模式)

## 1. Core Concept and Use Cases

### Core Concept
Define a family of algorithms, encapsulate each one, and make them **interchangeable**. Strategy lets the algorithm vary independently from clients that use it. The client can select which algorithm to use at runtime.

### Typical Use Cases
- Sorting algorithms (quicksort, mergesort, bubblesort)
- Payment methods (credit card, PayPal, crypto)
- Compression algorithms (gzip, bzip2, lz4)
- Authentication methods (password, OAuth, biometric)
- Routing strategies (shortest path, least congestion)

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                      Strategy Pattern                                             |
+--------------------------------------------------------------------------------------------------+

                              +---------------------------+
                              |         Context           |
                              +---------------------------+
                              | - strategy: Strategy*     |
                              +---------------------------+
                              | + set_strategy(strategy)  |
                              | + execute_strategy()      |
                              +-------------+-------------+
                                            |
                                            | uses
                                            v
                              +---------------------------+
                              |    Strategy Interface     |
                              +---------------------------+
                              | + execute(data)           |
                              +-------------+-------------+
                                            |
                      +---------------------+---------------------+
                      |                     |                     |
                      v                     v                     v
               +-------------+       +-------------+       +-------------+
               | Strategy A  |       | Strategy B  |       | Strategy C  |
               | (Algorithm1)|       | (Algorithm2)|       | (Algorithm3)|
               +-------------+       +-------------+       +-------------+
               | + execute() |       | + execute() |       | + execute() |
               +-------------+       +-------------+       +-------------+


    Runtime Strategy Selection:
    
    +--------+     set_strategy()     +---------+
    | Client |----------------------->| Context |
    +--------+                        +---------+
         |                                 |
         |                                 | execute_strategy()
         |                                 v
         |                           +-------------+
         |                           | Strategy A  |  <-- Selected at runtime
         |                           +-------------+
         |
         | Later: set_strategy(B)
         |
         v
    +---------+
    | Context |
    +---------+
         |
         | execute_strategy()
         v
    +-------------+
    | Strategy B  |  <-- Now using different strategy
    +-------------+
```

**中文说明：**

策略模式的核心流程：

1. **上下文（Context）**：
   - 持有策略接口的引用
   - 可以在运行时切换策略
   - 委托给策略执行算法

2. **策略接口（Strategy Interface）**：
   - 定义算法的通用接口
   - 所有具体策略都实现此接口

3. **具体策略**：
   - 实现不同的算法
   - 可以互换使用

---

## 3. Code Skeleton

```c
/* Strategy interface */
typedef struct Strategy {
    char name[32];
    int (*execute)(struct Strategy* self, void* data);
    void* context;
} Strategy;

/* Context that uses strategy */
typedef struct {
    Strategy* current_strategy;
    void* data;
} Context;

/* Context operations */
void context_set_strategy(Context* ctx, Strategy* strategy);
int context_execute(Context* ctx);

/* Strategy factory */
Strategy* create_strategy_a(void);
Strategy* create_strategy_b(void);
```

**中文说明：**

代码骨架包含：
- `Strategy`：策略接口
- `Context`：持有当前策略的上下文
- `context_set_strategy()`：运行时切换策略
- `context_execute()`：执行当前策略

---

## 4. Complete Example Code

```c
/*
 * Strategy Pattern - Payment Processing Example
 * 
 * This example demonstrates different payment strategies
 * that can be selected and switched at runtime.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================
 * Payment Data Structure
 * ============================================ */
typedef struct {
    double amount;
    char currency[4];
    char description[128];
    char customer_id[32];
} PaymentRequest;

typedef struct {
    int success;
    char transaction_id[64];
    char message[128];
} PaymentResult;

/* ============================================
 * Strategy Interface
 * ============================================ */
typedef struct PaymentStrategy PaymentStrategy;

typedef PaymentResult (*process_payment_fn)(PaymentStrategy* self, PaymentRequest* request);
typedef int (*validate_fn)(PaymentStrategy* self, PaymentRequest* request);
typedef void (*cleanup_fn)(PaymentStrategy* self);

struct PaymentStrategy {
    char name[32];                        /* Strategy name */
    process_payment_fn process;           /* Main processing function */
    validate_fn validate;                 /* Validation function */
    cleanup_fn cleanup;                   /* Cleanup function */
    void* config;                         /* Strategy-specific configuration */
};

/* ============================================
 * Context: Payment Processor
 * ============================================ */
typedef struct {
    PaymentStrategy* strategy;            /* Current payment strategy */
    int total_transactions;
    double total_amount;
} PaymentProcessor;

/* Initialize processor */
PaymentProcessor* processor_create(void)
{
    PaymentProcessor* proc = (PaymentProcessor*)malloc(sizeof(PaymentProcessor));
    if (proc) {
        proc->strategy = NULL;
        proc->total_transactions = 0;
        proc->total_amount = 0.0;
    }
    return proc;
}

/* Set payment strategy - can be changed at runtime */
void processor_set_strategy(PaymentProcessor* proc, PaymentStrategy* strategy)
{
    if (proc->strategy != NULL) {
        printf("[Processor] Switching strategy from '%s' to '%s'\n",
               proc->strategy->name, strategy->name);
    } else {
        printf("[Processor] Setting strategy to '%s'\n", strategy->name);
    }
    proc->strategy = strategy;
}

/* Process payment using current strategy */
PaymentResult processor_pay(PaymentProcessor* proc, PaymentRequest* request)
{
    PaymentResult result = {0, "", "No strategy set"};
    
    if (proc->strategy == NULL) {
        printf("[Processor] ERROR: No payment strategy set!\n");
        return result;
    }
    
    printf("\n[Processor] Processing payment of %.2f %s via %s\n",
           request->amount, request->currency, proc->strategy->name);
    
    /* Validate using strategy's validation */
    if (proc->strategy->validate != NULL) {
        if (!proc->strategy->validate(proc->strategy, request)) {
            result.success = 0;
            strcpy(result.message, "Validation failed");
            return result;
        }
    }
    
    /* Process using strategy's algorithm */
    result = proc->strategy->process(proc->strategy, request);
    
    if (result.success) {
        proc->total_transactions++;
        proc->total_amount += request->amount;
    }
    
    return result;
}

void processor_print_stats(PaymentProcessor* proc)
{
    printf("\n=== Processor Statistics ===\n");
    printf("Total Transactions: %d\n", proc->total_transactions);
    printf("Total Amount: $%.2f\n", proc->total_amount);
    printf("Current Strategy: %s\n", 
           proc->strategy ? proc->strategy->name : "None");
    printf("============================\n\n");
}

void processor_destroy(PaymentProcessor* proc)
{
    free(proc);
}

/* ============================================
 * Strategy 1: Credit Card Payment
 * ============================================ */
typedef struct {
    char merchant_id[32];
    double transaction_fee_percent;
} CreditCardConfig;

int creditcard_validate(PaymentStrategy* self, PaymentRequest* request)
{
    printf("  [CreditCard] Validating payment...\n");
    
    if (request->amount <= 0) {
        printf("  [CreditCard] Invalid amount: %.2f\n", request->amount);
        return 0;
    }
    if (request->amount > 10000) {
        printf("  [CreditCard] Amount exceeds limit: %.2f\n", request->amount);
        return 0;
    }
    
    printf("  [CreditCard] Validation passed\n");
    return 1;
}

PaymentResult creditcard_process(PaymentStrategy* self, PaymentRequest* request)
{
    PaymentResult result;
    CreditCardConfig* config = (CreditCardConfig*)self->config;
    
    double fee = request->amount * config->transaction_fee_percent / 100.0;
    double total = request->amount + fee;
    
    printf("  [CreditCard] Processing...\n");
    printf("  [CreditCard] Merchant: %s\n", config->merchant_id);
    printf("  [CreditCard] Amount: %.2f + Fee: %.2f = Total: %.2f\n",
           request->amount, fee, total);
    printf("  [CreditCard] Contacting card network...\n");
    printf("  [CreditCard] Authorization received!\n");
    
    result.success = 1;
    snprintf(result.transaction_id, sizeof(result.transaction_id),
             "CC-%s-%d", config->merchant_id, rand() % 100000);
    strcpy(result.message, "Credit card payment successful");
    
    return result;
}

void creditcard_cleanup(PaymentStrategy* self)
{
    free(self->config);
    free(self);
}

PaymentStrategy* create_creditcard_strategy(const char* merchant_id, double fee_percent)
{
    PaymentStrategy* strategy = (PaymentStrategy*)malloc(sizeof(PaymentStrategy));
    CreditCardConfig* config = (CreditCardConfig*)malloc(sizeof(CreditCardConfig));
    
    strncpy(strategy->name, "CreditCard", sizeof(strategy->name) - 1);
    strategy->process = creditcard_process;
    strategy->validate = creditcard_validate;
    strategy->cleanup = creditcard_cleanup;
    
    strncpy(config->merchant_id, merchant_id, sizeof(config->merchant_id) - 1);
    config->transaction_fee_percent = fee_percent;
    strategy->config = config;
    
    printf("[Factory] Created CreditCard strategy (fee=%.1f%%)\n", fee_percent);
    return strategy;
}

/* ============================================
 * Strategy 2: PayPal Payment
 * ============================================ */
typedef struct {
    char api_key[64];
    char environment[16];  /* sandbox or production */
} PayPalConfig;

int paypal_validate(PaymentStrategy* self, PaymentRequest* request)
{
    printf("  [PayPal] Validating payment...\n");
    
    if (strlen(request->customer_id) == 0) {
        printf("  [PayPal] Customer ID required\n");
        return 0;
    }
    
    printf("  [PayPal] Validation passed\n");
    return 1;
}

PaymentResult paypal_process(PaymentStrategy* self, PaymentRequest* request)
{
    PaymentResult result;
    PayPalConfig* config = (PayPalConfig*)self->config;
    
    printf("  [PayPal] Processing in %s environment...\n", config->environment);
    printf("  [PayPal] Authenticating with API...\n");
    printf("  [PayPal] Creating payment for customer: %s\n", request->customer_id);
    printf("  [PayPal] Amount: %.2f %s\n", request->amount, request->currency);
    printf("  [PayPal] Payment authorized!\n");
    
    result.success = 1;
    snprintf(result.transaction_id, sizeof(result.transaction_id),
             "PP-%d-%s", rand() % 1000000, config->environment);
    strcpy(result.message, "PayPal payment successful");
    
    return result;
}

void paypal_cleanup(PaymentStrategy* self)
{
    free(self->config);
    free(self);
}

PaymentStrategy* create_paypal_strategy(const char* api_key, int is_sandbox)
{
    PaymentStrategy* strategy = (PaymentStrategy*)malloc(sizeof(PaymentStrategy));
    PayPalConfig* config = (PayPalConfig*)malloc(sizeof(PayPalConfig));
    
    strncpy(strategy->name, "PayPal", sizeof(strategy->name) - 1);
    strategy->process = paypal_process;
    strategy->validate = paypal_validate;
    strategy->cleanup = paypal_cleanup;
    
    strncpy(config->api_key, api_key, sizeof(config->api_key) - 1);
    strcpy(config->environment, is_sandbox ? "sandbox" : "production");
    strategy->config = config;
    
    printf("[Factory] Created PayPal strategy (%s)\n", config->environment);
    return strategy;
}

/* ============================================
 * Strategy 3: Cryptocurrency Payment
 * ============================================ */
typedef struct {
    char wallet_address[64];
    char coin_type[8];
    double exchange_rate;
} CryptoConfig;

int crypto_validate(PaymentStrategy* self, PaymentRequest* request)
{
    CryptoConfig* config = (CryptoConfig*)self->config;
    
    printf("  [Crypto] Validating %s payment...\n", config->coin_type);
    
    /* Minimum amount for crypto */
    if (request->amount < 10.0) {
        printf("  [Crypto] Minimum amount is $10.00\n");
        return 0;
    }
    
    printf("  [Crypto] Validation passed\n");
    return 1;
}

PaymentResult crypto_process(PaymentStrategy* self, PaymentRequest* request)
{
    PaymentResult result;
    CryptoConfig* config = (CryptoConfig*)self->config;
    
    double crypto_amount = request->amount / config->exchange_rate;
    
    printf("  [Crypto] Processing %s payment...\n", config->coin_type);
    printf("  [Crypto] Exchange rate: 1 %s = $%.2f\n", 
           config->coin_type, config->exchange_rate);
    printf("  [Crypto] Amount: %.2f USD = %.8f %s\n",
           request->amount, crypto_amount, config->coin_type);
    printf("  [Crypto] Sending to wallet: %s\n", config->wallet_address);
    printf("  [Crypto] Waiting for blockchain confirmation...\n");
    printf("  [Crypto] Transaction confirmed!\n");
    
    result.success = 1;
    snprintf(result.transaction_id, sizeof(result.transaction_id),
             "0x%08X%08X", rand(), rand());
    snprintf(result.message, sizeof(result.message),
             "%s payment of %.8f confirmed", config->coin_type, crypto_amount);
    
    return result;
}

void crypto_cleanup(PaymentStrategy* self)
{
    free(self->config);
    free(self);
}

PaymentStrategy* create_crypto_strategy(const char* coin, const char* wallet, double rate)
{
    PaymentStrategy* strategy = (PaymentStrategy*)malloc(sizeof(PaymentStrategy));
    CryptoConfig* config = (CryptoConfig*)malloc(sizeof(CryptoConfig));
    
    strncpy(strategy->name, "Cryptocurrency", sizeof(strategy->name) - 1);
    strategy->process = crypto_process;
    strategy->validate = crypto_validate;
    strategy->cleanup = crypto_cleanup;
    
    strncpy(config->coin_type, coin, sizeof(config->coin_type) - 1);
    strncpy(config->wallet_address, wallet, sizeof(config->wallet_address) - 1);
    config->exchange_rate = rate;
    strategy->config = config;
    
    printf("[Factory] Created Crypto strategy (%s @ $%.2f)\n", coin, rate);
    return strategy;
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    printf("=== Strategy Pattern Demo ===\n\n");
    
    /* Create payment processor (context) */
    PaymentProcessor* processor = processor_create();
    
    /* Create different payment strategies */
    printf("--- Creating Strategies ---\n");
    PaymentStrategy* credit_card = create_creditcard_strategy("MERCH001", 2.5);
    PaymentStrategy* paypal = create_paypal_strategy("sk_test_123", 1);
    PaymentStrategy* bitcoin = create_crypto_strategy("BTC", "1A2b3C4d5E...", 45000.0);
    
    /* Create test payment request */
    PaymentRequest request = {
        .amount = 99.99,
        .currency = "USD",
        .description = "Premium subscription",
        .customer_id = "CUST001"
    };
    
    /* Test 1: Credit Card payment */
    printf("\n--- Test 1: Credit Card Payment ---\n");
    processor_set_strategy(processor, credit_card);
    PaymentResult result1 = processor_pay(processor, &request);
    printf("Result: %s (ID: %s)\n", result1.message, result1.transaction_id);
    
    /* Test 2: Switch to PayPal at runtime */
    printf("\n--- Test 2: Switch to PayPal ---\n");
    processor_set_strategy(processor, paypal);
    request.amount = 149.99;
    PaymentResult result2 = processor_pay(processor, &request);
    printf("Result: %s (ID: %s)\n", result2.message, result2.transaction_id);
    
    /* Test 3: Switch to Cryptocurrency */
    printf("\n--- Test 3: Switch to Cryptocurrency ---\n");
    processor_set_strategy(processor, bitcoin);
    request.amount = 500.00;
    PaymentResult result3 = processor_pay(processor, &request);
    printf("Result: %s (ID: %s)\n", result3.message, result3.transaction_id);
    
    /* Test 4: Validation failure */
    printf("\n--- Test 4: Crypto Validation Failure ---\n");
    request.amount = 5.00;  /* Below minimum */
    PaymentResult result4 = processor_pay(processor, &request);
    printf("Result: %s\n", result4.message);
    
    /* Print statistics */
    processor_print_stats(processor);
    
    /* Cleanup */
    printf("--- Cleanup ---\n");
    credit_card->cleanup(credit_card);
    paypal->cleanup(paypal);
    bitcoin->cleanup(bitcoin);
    processor_destroy(processor);
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了支付处理策略：

1. **支付处理器（Context）**：
   - 持有当前支付策略
   - `set_strategy()`：运行时切换策略
   - `pay()`：使用当前策略处理支付

2. **支付策略**：
   - **CreditCard**：信用卡支付，有手续费
   - **PayPal**：PayPal 支付，需要客户 ID
   - **Cryptocurrency**：加密货币支付，有最低金额

3. **运行时切换**：
   - 示例中多次切换支付策略
   - 同一处理器，不同策略，不同行为

4. **策略独立性**：
   - 每个策略有自己的配置
   - 有自己的验证和处理逻辑
   - 策略之间完全独立

