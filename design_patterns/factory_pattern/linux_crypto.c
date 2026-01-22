/*
场景说明
内核 Crypto 子系统提供了统一的加密 / 哈希接口，用户只需指定算法名（如 “AES”“SHA1”），工厂函数会返回对应算法的实例，无需关心算法的具体实现。

核心说明
内核 Crypto 子系统中，crypto_alloc_alg是统一工厂入口，根据算法名调用对应工厂方法创建实例；
工厂方法模式的优势：新增算法时，只需新增算法实现和对应的工厂方法，无需修改原有工厂逻辑，符合 “开闭原则”。
*/

#include <stdio.h>
#include <string.h>

// 1. 抽象产品：加密算法接口
typedef struct {
    const char *name;          // 算法名
    void (*encrypt)(char *data); // 加密/哈希函数
} crypto_alg;

// 2. 具体产品1：AES加密算法
static crypto_alg aes_alg = {
    .name = "aes",
    .encrypt = (void)(char *data) { printf("[AES算法] 加密数据：%s\n", data); }
};

// 3. 具体产品2：SHA1哈希算法
static crypto_alg sha1_alg = {
    .name = "sha1",
    .encrypt = (void)(char *data) { printf("[SHA1算法] 哈希数据：%s\n", data); }
};

// 4. 工厂方法：每个算法对应一个工厂函数（工厂方法模式）
crypto_alg* crypto_create_aes(void) {
    printf("Crypto工厂：创建AES算法实例\n");
    return &aes_alg;
}
crypto_alg* crypto_create_sha1(void) {
    printf("Crypto工厂：创建SHA1算法实例\n");
    return &sha1_alg;
}

// 5. 统一工厂入口：根据算法名调用对应工厂方法
crypto_alg* crypto_factory(const char *alg_name) {
    if (strcmp(alg_name, "aes") == 0) {
        return crypto_create_aes();
    } else if (strcmp(alg_name, "sha1") == 0) {
        return crypto_create_sha1();
    }
    return NULL;
}

// 测试
int main() {
    char data[] = "hello linux kernel";
    // 创建AES算法实例并使用
    crypto_alg *aes = crypto_factory("aes");
    if (aes) aes->encrypt(data);
    
    // 创建SHA1算法实例并使用
    crypto_alg *sha1 = crypto_factory("sha1");
    if (sha1) sha1->encrypt(data);
    return 0;
}