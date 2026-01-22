/*
策略思想映射
    策略接口：加密 / 哈希函数指针；
    具体策略：AES（加密）、SHA1/MD5（哈希）；
    上下文：内核 Crypto 子系统，上层模块通过算法名动态选择策略。
*/

#include <stdio.h>
#include <string.h>

// 模拟加密数据结构体
typedef struct { char *data; char *key; } crypto_data;

// 1. 策略接口：加密/哈希函数指针
typedef int (*CryptoStrategy)(crypto_data *data, char *out);

// 2. 具体策略实现：AES/SHA1/MD5
int aes_encrypt(crypto_data *d, char *out) {
    printf("[AES策略] 加密%s（密钥：%s）\n", d->data, d->key);
    strcpy(out, "aes_encrypted");
    return 0;
}
int sha1_hash(crypto_data *d, char *out) {
    printf("[SHA1策略] 哈希%s\n", d->data);
    strcpy(out, "sha1_hash");
    return 0;
}

// 3. 上下文：Crypto子系统上下文
typedef struct {
    CryptoStrategy alg; // 当前加密/哈希算法
    char alg_name[16];  // 算法名
} crypto_context;

// 上下文方法：切换加密算法
void crypto_set_alg(crypto_context *ctx, const char *name, CryptoStrategy a) {
    ctx->alg = a;
    strcpy(ctx->alg_name, name);
    printf("已切换算法：%s\n", name);
}

// 上下文方法：执行加密/哈希（内核核心逻辑）
int crypto_execute(crypto_context *ctx, crypto_data *d, char *out) {
    return ctx->alg ? ctx->alg(d, out) : -1;
}

// 调用测试
int main() {
    crypto_context ctx = {NULL, ""};
    crypto_data data = {"hello", "123456"};
    char out[32] = {0};

    // 切换为AES加密
    crypto_set_alg(&ctx, "aes", aes_encrypt);
    crypto_execute(&ctx, &data, out);
    printf("结果：%s\n\n", out);

    // 切换为SHA1哈希
    crypto_set_alg(&ctx, "sha1", sha1_hash);
    crypto_execute(&ctx, &data, out);
    printf("结果：%s\n", out);

    return 0;
}