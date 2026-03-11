## 简化版插件架构示例
- 文件结构
```
plugin
├── main.c            # 主程序
├── plugin.h           # 插件接口定义
├── plugin_a.c        # 插件A实现
├── plugin_b.c        # 插件B实现
└── Makefile          # 编译脚本
```
- plugin.h, plugin_a.c, plugin_b.c
```c
// plugin.h
#ifndef PLUGIN_H
#define PLUGIN_H

// 插件接口结构体
typedef struct
{
	const char *name;
	void (*init)(void);
	void (*execute)(void);
	void (*cleanup)(void);
} Plugin;

// 插件注册函数类型
typedef Plugin *(*RegisterPluginFunc)(void);

#endif // PLUGIN_H

// plugin_a.c
#include <stdio.h>
#include "plugin.h"

// 插件A的初始化函数
static void plugin_a_init(void)
{
	printf("Plugin A initialized\n");
}

// 插件A的执行函数
static void plugin_a_execute(void)
{
	printf("Plugin A executing...\n");
}

// 插件A的清理函数
static void plugin_a_cleanup(void)
{
	printf("Plugin A cleaned up\n");
}

// 插件A的接口实例
static Plugin plugin_a = {
	.name = "PluginA",
	.init = plugin_a_init,
	.execute = plugin_a_execute,
	.cleanup = plugin_a_cleanup};

// 插件注册函数
__attribute__((visibility("default")))
Plugin *
register_plugin(void)
{
	return &plugin_a;
}

// plugin_b.c
#include <stdio.h>
#include "plugin.h"

// 插件B的初始化函数
static void plugin_b_init(void)
{
	printf("Plugin B initialized\n");
}

// 插件B的执行函数
static void plugin_b_execute(void)
{
	printf("Plugin B executing...\n");
}

// 插件B的清理函数
static void plugin_b_cleanup(void)
{
	printf("Plugin B cleaned up\n");
}

// 插件B的接口实例
static Plugin plugin_b = {
	.name = "PluginB",
	.init = plugin_b_init,
	.execute = plugin_b_execute,
	.cleanup = plugin_b_cleanup};

// 插件注册函数
__attribute__((visibility("default")))
Plugin *
register_plugin(void)
{
	return &plugin_b;
}

// main.c
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include "plugin.h"

#define PLUGIN_DIR "./plugins"

int main()
{
	DIR *dir;
	struct dirent *entry;

	// 打开插件目录
	if ((dir = opendir(PLUGIN_DIR)) == NULL)
	{
		perror("opendir");
		return EXIT_FAILURE;
	}

	// 遍历插件目录
	while ((entry = readdir(dir)) != NULL)
	{
		// 只处理.so文件
		if (entry->d_type != DT_REG)
			continue;
		char *ext = strrchr(entry->d_name, '.');
		if (!ext || strcmp(ext, ".so") != 0)
			continue;

		// 构建完整路径
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", PLUGIN_DIR, entry->d_name);

		// 加载插件
		void *handle = dlopen(path, RTLD_LAZY);
		if (!handle)
		{
			fprintf(stderr, "Error loading %s: %s\n", path, dlerror());
			continue;
		}

		// 获取注册函数
		RegisterPluginFunc register_plugin = dlsym(handle, "register_plugin");
		if (!register_plugin)
		{
			fprintf(stderr, "Error finding register_plugin in %s: %s\n", path, dlerror());
			dlclose(handle);
			continue;
		}

		// 注册并获取插件接口
		Plugin *plugin = register_plugin();
		if (!plugin)
		{
			fprintf(stderr, "Plugin registration failed for %s\n", path);
			dlclose(handle);
			continue;
		}

		// 使用插件
		printf("\n=== Using plugin: %s ===\n", plugin->name);
		plugin->init();
		plugin->execute();
		plugin->cleanup();

		// 关闭插件
		dlclose(handle);
	}

	closedir(dir);
	return EXIT_SUCCESS;
}
```

- Makefile
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -fPIC
LDFLAGS = -ldl

# 主程序目标
TARGET = plugin_demo

# 插件目录
PLUGIN_DIR = plugins

# 插件目标
PLUGINS = $(PLUGIN_DIR)/plugin_a.so $(PLUGIN_DIR)/plugin_b.so

all: $(TARGET) $(PLUGINS)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(PLUGIN_DIR)/plugin_a.so: plugin_a.c plugin.h
	@mkdir -p $(PLUGIN_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $<

$(PLUGIN_DIR)/plugin_b.so: plugin_b.c plugin.h
	@mkdir -p $(PLUGIN_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $<

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(PLUGINS)
	rmdir $(PLUGIN_DIR) 2>/dev/null || true

.PHONY: all run clean

```