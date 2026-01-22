/*
场景说明
内核的 dm-crypt（磁盘加密）是装饰器模式的典型应用：核心功能是底层块设备的file_operations（读 / 写），dm-crypt 作为装饰器，不修改底层块设备的任何代码，仅在读写时添加 “加密 / 解密” 附加功能，上层文件系统完全无感知。

核心说明
dm-crypt 作为装饰器，持有底层块设备的file_operations接口；
写操作时先加密数据，再调用底层写；读操作时先调用底层读，再解密数据；
上层文件系统（如 ext4）仅调用装饰后的file_operations，完全无感知加密逻辑。
*/

#include <stdio.h>
#include <string.h>

// 模拟内核file_operations核心接口（Component）
typedef struct {
    int (*read)(char *buf, int len);  // 核心读接口
    int (*write)(const char *buf, int len); // 核心写接口
} file_operations;

// ---------------------- 具体组件：底层块设备的基础读写（无加密） ----------------------
static int disk_base_read(char *buf, int len) {
    strncpy(buf, "encrypted_data", len-1); // 模拟从磁盘读取加密数据
    printf("[核心功能] 底层磁盘读数据：%s\n", buf);
    return strlen(buf);
}

static int disk_base_write(const char *buf, int len) {
    printf("[核心功能] 底层磁盘写数据：%.*s\n", len, buf);
    return len;
}

static file_operations disk_base_fops = {
    .read = disk_base_read,
    .write = disk_base_write
};

// ---------------------- 具体装饰器：dm-crypt加密装饰器 ----------------------
typedef struct {
    file_operations core_fops; // 持有核心接口（底层块设备）
} crypt_decorator;

// 模拟AES加密（附加功能）
static void aes_encrypt(char *data, int len) {
    for (int i=0; i<len; i++) data[i] ^= 0x55; // 简单异或加密
}

// 模拟AES解密（附加功能）
static void aes_decrypt(char *data, int len) {
    for (int i=0; i<len; i++) data[i] ^= 0x55; // 简单异或解密
}

// 加密装饰器-写：先加密，再调用底层写
static int crypt_write(crypt_decorator *decorator, const char *buf, int len) {
    char encrypt_buf[256] = {0};
    memcpy(encrypt_buf, buf, len);
    // 附加功能：加密
    aes_encrypt(encrypt_buf, len);
    printf("[加密装饰] 数据加密后：%.*s\n", len, encrypt_buf);
    // 调用核心功能（底层磁盘写）
    return decorator->core_fops.write(encrypt_buf, len);
}

// 加密装饰器-读：先调用底层读，再解密
static int crypt_read(crypt_decorator *decorator, char *buf, int len) {
    // 调用核心功能（底层磁盘读）
    int ret = decorator->core_fops.read(buf, len);
    // 附加功能：解密
    aes_decrypt(buf, ret);
    printf("[加密装饰] 数据解密后：%s\n", buf);
    return ret;
}

// 初始化加密装饰器
static void crypt_decorator_init(crypt_decorator *decorator, file_operations core) {
    decorator->core_fops = core;
    decorator->core_fops.write = (int (*)(const char*, int))crypt_write;
    decorator->core_fops.read = (int (*)(char*, int))crypt_read;
}

// ---------------------- 上层文件系统：无感知使用加密装饰后的接口 ----------------------
int main() {
    // 初始化加密装饰器，包裹底层磁盘接口
    crypt_decorator crypt_dec;
    crypt_decorator_init(&crypt_dec, disk_base_fops);
    file_operations fops = crypt_dec.core_fops;
    
    // 上层写操作：透明加密
    printf("\n=== 执行加密写操作 ===\n");
    fops.write("plain text", 10);
    
    // 上层读操作：透明解密
    printf("\n=== 执行加密读操作 ===\n");
    char buf[32] = {0};
    fops.read(buf, sizeof(buf));
    
    return 0;
}