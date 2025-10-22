> collectd is a small daemon which collects metrics periodically and provides mechanisms to store and monitor the values in a variety of ways.

[collectd](https://collectd.org/)  
[]()  
[]()  
[]()  
[]()  

## install collectd
sudo apt install collectd
 
## install collectd development package
sudo apt install collectd-dev

## 关键数据结构
```c
// value_list_t
struct value_list_s {
  cdtime_t time;          					// 高精度时间戳（纳秒级）
  char host[DATA_MAX_NAME_LEN];       		// 主机名
  char plugin[DATA_MAX_NAME_LEN];     		// 插件名
  char plugin_instance[DATA_MAX_NAME_LEN]; 	// 插件实例
  char type[DATA_MAX_NAME_LEN];       		// 数据类型
  char type_instance[DATA_MAX_NAME_LEN]; 	// 类型实例
  meta_data_t *meta;      					// 元数据（键值对）
  value_t *values;        					// 值数组指针
  int values_len;         					// 值数组长度
  int interval;           					// 采集间隔
  char severity;          					// 严重级别（用于告警）
};
typedef struct value_list_s value_list_t;

// value_t
typedef union {
  gauge_t     gauge;     // 浮点数值（double）
  derive_t    derive;    // 64位有符号整数（可增可减）
  counter_t   counter;   // 64位无符号整数（单调递增）
  absolute_t  absolute; // 64位无符号整数（周期重置）
} value_t;

// 具体类型定义
typedef double gauge_t;
typedef int64_t derive_t;
typedef uint64_t counter_t;
typedef uint64_t absolute_t;

// collectd 使用文本数据库定义metric类型
# /usr/share/collectd/types.db
cpu               value:GAUGE:0:100
memory            used:GAUGE:0:1099511627776, free:GAUGE:0:1099511627776
if_octets         rx:DERIVE:0:U, tx:DERIVE:0:U

// 格式说明
<type_name> <ds_name>:<ds_type>:<min>:<max> [,...]

// 多值 metric
disk_io           read:DERIVE:0:U, write:DERIVE:0:U

// 多值 metric 插件示例
static int disk_read(void) {
	value_t values[2] = {
		{ .derive = get_read_bytes() },
		{ .derive = get_write_bytes() }
	};

	value_list_t vl = {
		.values = values,
		.values_len = 2,
		.plugin = "disk",
		.type = "disk_io",
		.type_instance = "sda"
	};

	plugin_dispatch_values(&vl);
}
```

## 联合体 + 类型数据库：高效灵活的数据类型扩展实现
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>

// ======================
// 1. 数据类型定义
// ======================

// 数据类型枚举
typedef enum
{
	DS_TYPE_UNKNOWN = 0,
	DS_TYPE_GAUGE,	 // 浮点数值
	DS_TYPE_COUNTER, // 单调递增计数器
	DS_TYPE_DERIVE,	 // 可增可减的计数器
	DS_TYPE_ABSOLUTE // 周期重置的计数器
} ds_type_t;

// 值联合体 - 支持多种数据类型
typedef union
{
	double gauge;	   // 浮点数值
	uint64_t counter;  // 64位无符号整数
	int64_t derive;	   // 64位有符号整数
	uint64_t absolute; // 64位无符号整数
} value_t;

// 数据源定义
typedef struct
{
	char name[32];	// 数据源名称
	ds_type_t type; // 数据类型
	double min;		// 最小值
	double max;		// 最大值
} data_source_t;

// 数据类型定义
typedef struct
{
	char name[32];			// 类型名称
	data_source_t *sources; // 数据源数组
	int num_sources;		// 数据源数量
} data_type_t;

// 值列表结构
typedef struct
{
	time_t timestamp;		  // 时间戳
	char host[64];			  // 主机名
	char plugin[32];		  // 插件名
	char plugin_instance[32]; // 插件实例
	char type[32];			  // 数据类型
	char type_instance[32];	  // 类型实例
	value_t *values;		  // 值数组
	int num_values;			  // 值数量
} value_list_t;

// ======================
// 2. 类型数据库实现
// ======================

// 全局类型数据库
data_type_t *type_db = NULL;
int num_types = 0;

// 添加新类型到数据库
void add_type_to_db(const char *name, data_source_t *sources, int num_sources)
{
	// 扩展类型数据库
	type_db = realloc(type_db, (num_types + 1) * sizeof(data_type_t));

	// 填充新类型信息
	data_type_t *new_type = &type_db[num_types];
	strncpy(new_type->name, name, sizeof(new_type->name));
	new_type->num_sources = num_sources;

	// 复制数据源
	new_type->sources = malloc(num_sources * sizeof(data_source_t));
	memcpy(new_type->sources, sources, num_sources * sizeof(data_source_t));

	num_types++;
}

// 从数据库查找类型
const data_type_t *find_type(const char *name)
{
	for (int i = 0; i < num_types; i++)
	{
		if (strcmp(type_db[i].name, name) == 0)
		{
			return &type_db[i];
		}
	}
	return NULL;
}

// 初始化类型数据库
void init_type_db()
{
	// 定义CPU类型
	data_source_t cpu_sources[] = {
		{"user", DS_TYPE_DERIVE, 0, 100},
		{"system", DS_TYPE_DERIVE, 0, 100},
		{"idle", DS_TYPE_DERIVE, 0, 100}};
	add_type_to_db("cpu", cpu_sources, 3);

	// 定义内存类型
	data_source_t memory_sources[] = {
		{"used", DS_TYPE_GAUGE, 0, 1e12}, // 最大1TB
		{"free", DS_TYPE_GAUGE, 0, 1e12}};
	add_type_to_db("memory", memory_sources, 2);

	// 定义网络类型
	data_source_t network_sources[] = {
		{"rx_bytes", DS_TYPE_DERIVE, 0, 1e9}, // 最大1Gbps
		{"tx_bytes", DS_TYPE_DERIVE, 0, 1e9}};
	add_type_to_db("network", network_sources, 2);
}

// ======================
// 3. 数据处理函数
// ======================

// 创建值列表
value_list_t *create_value_list(const char *plugin, const char *type)
{
	value_list_t *vl = malloc(sizeof(value_list_t));
	memset(vl, 0, sizeof(value_list_t));

	vl->timestamp = time(NULL);
	gethostname(vl->host, sizeof(vl->host));
	strncpy(vl->plugin, plugin, sizeof(vl->plugin));
	strncpy(vl->type, type, sizeof(vl->type));

	// 根据类型确定值数量
	const data_type_t *type_def = find_type(type);
	if (type_def)
	{
		vl->num_values = type_def->num_sources;
		vl->values = calloc(vl->num_values, sizeof(value_t));
	}

	return vl;
}

// 设置值
void set_value(value_list_t *vl, int index, double value)
{
	if (index < 0 || index >= vl->num_values)
		return;

	const data_type_t *type_def = find_type(vl->type);
	if (!type_def)
		return;

	// 根据数据类型设置值
	switch (type_def->sources[index].type)
	{
	case DS_TYPE_GAUGE:
		vl->values[index].gauge = value;
		break;
	case DS_TYPE_COUNTER:
		vl->values[index].counter = (uint64_t)value;
		break;
	case DS_TYPE_DERIVE:
		vl->values[index].derive = (int64_t)value;
		break;
	case DS_TYPE_ABSOLUTE:
		vl->values[index].absolute = (uint64_t)value;
		break;
	default:
		break;
	}
}

// 验证值
int validate_value(const value_list_t *vl)
{
	const data_type_t *type_def = find_type(vl->type);
	if (!type_def || type_def->num_sources != vl->num_values)
	{
		return 0; // 无效类型或值数量不匹配
	}

	for (int i = 0; i < vl->num_values; i++)
	{
		const data_source_t *ds = &type_def->sources[i];
		double value;

		// 根据数据类型获取值
		switch (ds->type)
		{
		case DS_TYPE_GAUGE:
			value = vl->values[i].gauge;
			break;
		case DS_TYPE_COUNTER:
			value = vl->values[i].counter;
			break;
		case DS_TYPE_DERIVE:
			value = vl->values[i].derive;
			break;
		case DS_TYPE_ABSOLUTE:
			value = vl->values[i].absolute;
			break;
		default:
			return 0; // 未知类型
		}

		// 检查值范围
		if (value < ds->min || value > ds->max)
		{
			return 0; // 值超出范围
		}
	}

	return 1; // 所有值有效
}

// 数据类型转字符串
const char *ds_type_to_str(ds_type_t type)
{
	switch (type)
	{
	case DS_TYPE_GAUGE:
		return "GAUGE";
	case DS_TYPE_COUNTER:
		return "COUNTER";
	case DS_TYPE_DERIVE:
		return "DERIVE";
	case DS_TYPE_ABSOLUTE:
		return "ABSOLUTE";
	default:
		return "UNKNOWN";
	}
}

// 打印值列表
void print_value_list(const value_list_t *vl)
{
	printf("Value List:\n");
	printf("  Timestamp: %ld\n", vl->timestamp);
	printf("  Host: %s\n", vl->host);
	printf("  Plugin: %s\n", vl->plugin);
	printf("  Type: %s\n", vl->type);

	const data_type_t *type_def = find_type(vl->type);
	if (!type_def)
	{
		printf("  [ERROR] Unknown type!\n");
		return;
	}

	printf("  Values:\n");
	for (int i = 0; i < vl->num_values; i++)
	{
		const data_source_t *ds = &type_def->sources[i];
		double value;

		// 根据数据类型获取值
		switch (ds->type)
		{
		case DS_TYPE_GAUGE:
			value = vl->values[i].gauge;
			break;
		case DS_TYPE_COUNTER:
			value = vl->values[i].counter;
			break;
		case DS_TYPE_DERIVE:
			value = vl->values[i].derive;
			break;
		case DS_TYPE_ABSOLUTE:
			value = vl->values[i].absolute;
			break;
		default:
			value = 0;
		}

		printf("    %s (%s): %.2f\n", ds->name,
			   ds_type_to_str(ds->type), value);
	}
}

// ======================
// 4. 主程序演示
// ======================

int main()
{
	// 初始化类型数据库
	init_type_db();

	// 示例1: 创建CPU使用率数据
	value_list_t *cpu_usage = create_value_list("cpu", "cpu");
	if (cpu_usage)
	{
		strcpy(cpu_usage->plugin_instance, "0"); // CPU核心0
		set_value(cpu_usage, 0, 25.5);			 // user
		set_value(cpu_usage, 1, 10.2);			 // system
		set_value(cpu_usage, 2, 64.3);			 // idle

		printf("CPU Usage Data:\n");
		print_value_list(cpu_usage);
		printf("Validation: %s\n\n", validate_value(cpu_usage) ? "PASS" : "FAIL");

		free(cpu_usage->values);
		free(cpu_usage);
	}

	// 示例2: 创建内存使用数据
	value_list_t *mem_usage = create_value_list("memory", "memory");
	if (mem_usage)
	{
		set_value(mem_usage, 0, 8.5e9);	 // used (8.5GB)
		set_value(mem_usage, 1, 15.5e9); // free (15.5GB)

		printf("Memory Usage Data:\n");
		print_value_list(mem_usage);
		printf("Validation: %s\n\n", validate_value(mem_usage) ? "PASS" : "FAIL");

		free(mem_usage->values);
		free(mem_usage);
	}

	// 示例3: 创建网络流量数据
	value_list_t *net_traffic = create_value_list("net", "network");
	if (net_traffic)
	{
		strcpy(net_traffic->type_instance, "eth0");
		set_value(net_traffic, 0, 125000); // rx_bytes
		set_value(net_traffic, 1, 75000);  // tx_bytes

		printf("Network Traffic Data:\n");
		print_value_list(net_traffic);
		printf("Validation: %s\n\n", validate_value(net_traffic) ? "PASS" : "FAIL");

		free(net_traffic->values);
		free(net_traffic);
	}

	// 示例4: 无效数据测试
	value_list_t *invalid_data = create_value_list("test", "invalid_type");
	if (invalid_data)
	{
		printf("Invalid Type Test:\n");
		print_value_list(invalid_data);
		printf("Validation: %s\n\n", validate_value(invalid_data) ? "PASS" : "FAIL");

		free(invalid_data->values);
		free(invalid_data);
	}

	// 清理类型数据库
	for (int i = 0; i < num_types; i++)
	{
		free(type_db[i].sources);
	}
	free(type_db);

	return 0;
}
```

## plugin example
simple plugin (.so file) that registers:
| Callback   | Purpose                                                      |
| ---------- | ------------------------------------------------------------ |
| `config`   | Parse `<Plugin simple>` config options                       |
| `init`     | Run once during plugin initialization                        |
| `read`     | Periodically produce metrics                                 |
| `write`    | Receive dispatched metrics from other plugins (and log them) |
| `shutdown` | Clean up before exit (optional)                              |
```c
/*
* simple_plugin.c
*
* Example collectd plugin demonstrating:
*   - config callback
*   - init callback
*   - read callback
*   - write callback
*   - shutdown callback
*/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
 
#include "collectd.h"
#include "plugin.h"
#include "common.h"
#include "plugin_dispatch.h"
 
/* --------------------------------------------------------------------------
* 1) Configuration structure
* -------------------------------------------------------------------------- */
typedef struct {
    cdtime_t interval;
    char message[256];
    int enable_writer;
} simple_config_t;
 
static simple_config_t g_cfg = {
    .interval = 0,  /* use global Interval */
    .message = "Default message",
    .enable_writer = 0
};
 
/* --------------------------------------------------------------------------
* 2) Configuration callback
* -------------------------------------------------------------------------- */
static int simple_config(const oconfig_item_t *ci)
{
    for (int i = 0; i < ci->children_num; i++) {
        oconfig_item_t *child = ci->children + i;
 
        if (strcasecmp("Interval", child->key) == 0) {
            double tmp;
            if (cf_util_get_double(child, &tmp) == 0)
                g_cfg.interval = cdtime_from_double(tmp);
        }
        else if (strcasecmp("Message", child->key) == 0) {
            char *tmp = NULL;
            if (cf_util_get_string(child, &tmp) == 0) {
                sstrncpy(g_cfg.message, tmp, sizeof(g_cfg.message));
                free(tmp);
            }
        }
        else if (strcasecmp("EnableWriter", child->key) == 0) {
            int tmp;
            if (cf_util_get_boolean(child, &tmp) == 0)
                g_cfg.enable_writer = tmp;
        }
        else {
            WARNING("simple plugin: unknown config key '%s'", child->key);
        }
    }
 
    INFO("simple plugin: Config parsed (Interval=%.2fs, Message=\"%s\", EnableWriter=%d)",
         (g_cfg.interval > 0 ? CDTIME_T_TO_DOUBLE(g_cfg.interval) : 0),
         g_cfg.message, g_cfg.enable_writer);
 
    return 0;
}
 
/* --------------------------------------------------------------------------
* 3) Init callback
* -------------------------------------------------------------------------- */
static int simple_init(void)
{
    INFO("simple plugin: init called — message=\"%s\"", g_cfg.message);
    return 0;  /* success */
}
 
/* --------------------------------------------------------------------------
* 4) Read callback
* -------------------------------------------------------------------------- */
static int simple_read(user_data_t *ud)
{
    value_t value;
    value_list_t vl = VALUE_LIST_INIT;
 
    /* Example value: seconds mod 60 */
    time_t now = time(NULL);
    value.gauge = (gauge_t)(now % 60);
 
    vl.values = &value;
    vl.values_len = 1;
 
    sstrncpy(vl.plugin, "simple", sizeof(vl.plugin));
    sstrncpy(vl.type, "gauge", sizeof(vl.type));
    sstrncpy(vl.type_instance, "example_value", sizeof(vl.type_instance));
 
    vl.time = cdtime();
    vl.interval = (g_cfg.interval > 0) ? g_cfg.interval : 0;
 
    plugin_dispatch_values(&vl);
 
    INFO("simple plugin (read): dispatched value=%.0f, message=\"%s\"",
         value.gauge, g_cfg.message);
 
    return 0;
}
 
/* --------------------------------------------------------------------------
* 5) Write callback
* -------------------------------------------------------------------------- */
static int simple_write(const data_set_t *ds, const value_list_t *vl,
                        user_data_t *ud)
{
    char ident[6 * DATA_MAX_NAME_LEN];
    if (format_name(ident, sizeof(ident), vl) != 0)
        sstrncpy(ident, "unknown", sizeof(ident));
 
    INFO("simple plugin (write): received metric [%s]", ident);
 
    for (int i = 0; i < vl->values_len; i++) {
        switch (ds->ds[i].type) {
        case DS_TYPE_GAUGE:
            INFO("  gauge[%d] = %f", i, vl->values[i].gauge);
            break;
        case DS_TYPE_COUNTER:
            INFO("  counter[%d] = %" PRIu64, i, vl->values[i].counter);
            break;
        case DS_TYPE_DERIVE:
            INFO("  derive[%d] = %" PRIi64, i, vl->values[i].derive);
            break;
        case DS_TYPE_ABSOLUTE:
            INFO("  absolute[%d] = %" PRIu64, i, vl->values[i].absolute);
            break;
        }
    }
 
    return 0;
}
 
/* --------------------------------------------------------------------------
* 6) Shutdown callback
* -------------------------------------------------------------------------- */
static int simple_shutdown(void)
{
    INFO("simple plugin: shutdown — cleaning up");
    return 0;
}
 
/* --------------------------------------------------------------------------
* 7) Module registration
* -------------------------------------------------------------------------- */
void module_register(void)
{
    plugin_register_complex_config("simple", simple_config);
    plugin_register_init("simple", simple_init);
    plugin_register_complex_read("simple", simple_read, NULL, NULL);
    plugin_register_shutdown("simple", simple_shutdown);
 
    /* Optionally register the write callback if enabled */
    if (g_cfg.enable_writer) {
        plugin_register_write("simple", simple_write, /* user_data */ NULL);
        INFO("simple plugin: write callback registered");
    }
}
```
 
## compile
```bash
gcc -fPIC -shared -o simple_plugin.so simple_plugin.c -I/usr/include/collectd
```
 
## /etc/collectd/collectd.conf
```
LoadPlugin simple
 
<Plugin simple>
  Interval 5
  Message "Hello from plugin with write and init!"
  EnableWriter true
</Plugin>
 
# Optional: enable write_log to observe output
LoadPlugin write_log
<Plugin write_log>
  LogLevel "info"
</Plugin>
```
 
## run collectd interactively
```bash
sudo collectd -C /etc/collectd/collectd.conf -f
```
 
## how it works
| Stage                 | Callback            | Description                                                                                              |
| --------------------- | ------------------- | -------------------------------------------------------------------------------------------------------- |
| **1. Config Parsing** | `simple_config()`   | Called during startup to read `<Plugin simple>` options.                                                 |
| **2. Initialization** | `simple_init()`     | Called once after config parsing, before the main loop.                                                  |
| **3. Reading Data**   | `simple_read()`     | Called periodically (default or configured interval). Dispatches values with `plugin_dispatch_values()`. |
| **4. Writing Data**   | `simple_write()`    | Called for each dispatched metric (including its own). You can store, print, or forward metrics here.    |
| **5. Shutdown**       | `simple_shutdown()` | Called when collectd stops, to release resources.                                                        |
 
## summary
| Callback          | Purpose                    | Register Function                  |
| ----------------- | -------------------------- | ---------------------------------- |
| `simple_config`   | Parse config block         | `plugin_register_complex_config()` |
| `simple_init`     | Init once                  | `plugin_register_init()`           |
| `simple_read`     | Periodic metric production | `plugin_register_complex_read()`   |
| `simple_write`    | Metric consumption         | `plugin_register_write()`          |
| `simple_shutdown` | Cleanup                    | `plugin_register_shutdown()`       |

## collectd 的核心运行机制简化版
| 模块     | 示例函数                         | Collectd 对应逻辑                 |
| ------  | -------------------------------- | -------------------------------  |
| 插件管理 | `register_*()`                   | `plugin_register_*()`            |
| 调度线程 | `scheduler_thread()`             | Collectd 的 `plugin_read_thread` |
| 采集间隔 | `interval`                       | 每个插件的 `Interval` 设置         |
| 生命周期 | `init_all()` / `shutdown_all()`  | 插件初始化与退出钩子               |
| 动态加载 | `dlopen()` + `module_register()` | `plugin_load_all()`              |

- dir structure
```
plugin/
├── main
├── main.c
└── plugins
    ├── plugin_a.c
    ├── plugin_b.c
```
- c code
```c
// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>

typedef void (*init_cb_t)(void);
typedef void (*read_cb_t)(void);
typedef void (*write_cb_t)(const char *);
typedef void (*shutdown_cb_t)(void);

typedef struct plugin_s
{
	char name[64];
	init_cb_t init;
	read_cb_t read;
	write_cb_t write;
	shutdown_cb_t shutdown;
	int interval; // 采集周期 (秒)
	time_t last_run;
	struct plugin_s *next;
} plugin_t;

static plugin_t *plugins = NULL;
static int running = 1;

// ---------------- 插件注册接口 ----------------
void register_read(const char *name, read_cb_t cb)
{
	plugin_t *p = malloc(sizeof(plugin_t));
	memset(p, 0, sizeof(plugin_t));
	strncpy(p->name, name, sizeof(p->name) - 1);
	p->read = cb;
	p->interval = 5; // 默认 5 秒
	p->next = plugins;
	plugins = p;
	printf("[core] Registered read() plugin: %s\n", name);
}

void register_write(const char *name, write_cb_t cb)
{
	for (plugin_t *p = plugins; p; p = p->next)
	{
		if (strcmp(p->name, name) == 0)
		{
			p->write = cb;
			printf("[core] Registered write() for %s\n", name);
			return;
		}
	}
}

void register_init(const char *name, init_cb_t cb)
{
	for (plugin_t *p = plugins; p; p = p->next)
	{
		if (strcmp(p->name, name) == 0)
		{
			p->init = cb;
			printf("[core] Registered init() for %s\n", name);
			return;
		}
	}
}

void register_shutdown(const char *name, shutdown_cb_t cb)
{
	for (plugin_t *p = plugins; p; p = p->next)
	{
		if (strcmp(p->name, name) == 0)
		{
			p->shutdown = cb;
			printf("[core] Registered shutdown() for %s\n", name);
			return;
		}
	}
}

void register_interval(const char *name, int interval)
{
	for (plugin_t *p = plugins; p; p = p->next)
	{
		if (strcmp(p->name, name) == 0)
		{
			p->interval = interval;
			printf("[core] Set interval=%d for %s\n", interval, name);
			return;
		}
	}
}

// ---------------- 生命周期阶段 ----------------
void init_all(void)
{
	printf("\n[core] === init_all() ===\n");
	for (plugin_t *p = plugins; p; p = p->next)
	{
		if (p->init)
		{
			printf("[core] Init plugin: %s\n", p->name);
			p->init();
		}
	}
}

void shutdown_all(void)
{
	printf("\n[core] === shutdown_all() ===\n");
	for (plugin_t *p = plugins; p; p = p->next)
	{
		if (p->shutdown)
		{
			printf("[core] Shutdown plugin: %s\n", p->name);
			p->shutdown();
		}
	}
}

// ---------------- 调度线程 ----------------
void *scheduler_thread(void *arg)
{
	printf("[core] Scheduler thread started.\n");
	while (running)
	{
		time_t now = time(NULL);
		for (plugin_t *p = plugins; p; p = p->next)
		{
			if (!p->read)
				continue;
			if (difftime(now, p->last_run) >= p->interval)
			{
				printf("[core] Running %s.read()\n", p->name);
				p->read();
				p->last_run = now;
			}
		}
		sleep(1);
	}
	return NULL;
}

// ---------------- 动态加载 ----------------
void load_all_plugins(const char *dirpath)
{
	DIR *dir = opendir(dirpath);
	if (!dir)
	{
		perror("opendir");
		return;
	}

	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL)
	{
		if (!strstr(ent->d_name, ".so"))
			continue;

		char path[512];
		snprintf(path, sizeof(path), "%s/%s", dirpath, ent->d_name);

		struct stat st;
		if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
			continue;

		printf("[core] Loading plugin: %s\n", path);
		void *handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
		if (!handle)
		{
			fprintf(stderr, "dlopen failed: %s\n", dlerror());
			continue;
		}

		void (*module_register)(void) = dlsym(handle, "module_register");
		if (!module_register)
		{
			fprintf(stderr, "No module_register() in %s\n", path);
			dlclose(handle);
			continue;
		}

		module_register();
	}
	closedir(dir);
}

// ---------------- 主程序入口 ----------------
int main(void)
{
	printf("[core] Scanning and loading all plugins...\n");
	load_all_plugins("./plugins");

	init_all();

	pthread_t tid;
	pthread_create(&tid, NULL, scheduler_thread, NULL);

	// 模拟主线程的 write() 操作
	for (int i = 0; i < 3; i++)
	{
		printf("\n[core] === main loop iteration %d ===\n", i + 1);
		for (plugin_t *p = plugins; p; p = p->next)
		{
			if (p->write)
				p->write("data from core");
		}
		sleep(5);
	}

	running = 0;
	pthread_join(tid, NULL);

	shutdown_all();
	return 0;
}

// plugins/plugin_a.c
#include <stdio.h>

void register_init(const char *name, void (*cb)(void));
void register_read(const char *name, void (*cb)(void));
void register_write(const char *name, void (*cb)(const char *));
void register_shutdown(const char *name, void (*cb)(void));
void register_interval(const char *name, int interval);

static void init_cb(void)
{
	printf("[plugin_a] init_cb() setup complete\n");
}

static void read_cb(void)
{
	printf("[plugin_a] read_cb() collecting metrics...\n");
}

static void write_cb(const char *data)
{
	printf("[plugin_a] write_cb() sending: %s\n", data);
}

static void shutdown_cb(void)
{
	printf("[plugin_a] shutdown_cb() cleanup done\n");
}

void module_register(void)
{
	printf("[plugin_a] module_register()\n");
	register_read("plugin_a", read_cb);
	register_write("plugin_a", write_cb);
	register_init("plugin_a", init_cb);
	register_shutdown("plugin_a", shutdown_cb);
	register_interval("plugin_a", 3); // 每 3 秒执行一次 read
}

// plugins/plugin_b.c
#include <stdio.h>

void register_read(const char *name, void (*cb)(void));
void register_init(const char *name, void (*cb)(void));
void register_shutdown(const char *name, void (*cb)(void));
void register_interval(const char *name, int interval);

static void init_cb(void)
{
	printf("[plugin_b] init_cb() ready\n");
}

static void read_cb(void)
{
	printf("[plugin_b] read_cb() periodic task running\n");
}

static void shutdown_cb(void)
{
	printf("[plugin_b] shutdown_cb() finished\n");
}

void module_register(void)
{
	printf("[plugin_b] module_register()\n");
	register_read("plugin_b", read_cb);
	register_init("plugin_b", init_cb);
	register_shutdown("plugin_b", shutdown_cb);
	register_interval("plugin_b", 5); // 每 5 秒执行一次 read
}
```

- compile and run
```bash
mkdir -p plugins

gcc -fPIC -shared -o plugins/plugin_a.so plugins/plugin_a.c
gcc -fPIC -shared -o plugins/plugin_b.so plugins/plugin_b.c
gcc -rdynamic -o main main.c -ldl -pthread

./main

```

## 增强版本: 插件自定义配置文件驱动（JSON DB）+ 动态注册指标（metrics）机制
| 模块                               | 对应 Collectd 概念              | 功能说明                 |
| -------------------------------- | -------------------------------- | ----------------------   |
| `metrics.db` / JSON              | `types.db` + `collectd.conf`     | 用户配置 metric 类型与周期 |
| `register_metric()`              | `plugin_register_complex_read()` | 插件动态注册要采集的 metric |
| `scheduler_thread()`             | Collectd read 调度线程            | 按 interval 执行采集       |
| JSON 配置文件                     | 各插件独立配置                    | 支持插件自定义配置行为       |
| `dlopen()` + `module_register()` | 动态插件系统                      | 按需加载模块                |

- dir structure
```
plugin/
├── main.c
└── plugins
    ├── plugin_cpu.c
    ├── plugin_cpu.json
    ├── plugin_temp.c
    └── plugin_temp.json
```

- main.c, plugin_cpu.c, plugin_temp.c
```c
// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

typedef void (*read_cb_t)(const char *metric);
typedef void (*register_metric_cb_t)(const char *plugin, const char *metric, const char *type, int interval);

typedef struct metric_s {
    char name[64];
    char plugin[64];
    char type[32];
    int interval;
    time_t last_run;
    struct metric_s *next;
} metric_t;

typedef struct plugin_s {
    char name[64];
    read_cb_t read;
    struct plugin_s *next;
} plugin_t;

static plugin_t *plugins = NULL;
static metric_t *metrics = NULL;
static int running = 1;

// ---------------- 注册接口 ----------------
void register_plugin_read(const char *name, read_cb_t cb) {
    plugin_t *p = malloc(sizeof(plugin_t));
    memset(p, 0, sizeof(plugin_t));
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->read = cb;
    p->next = plugins;
    plugins = p;
    printf("[core] Registered plugin: %s\n", name);
}

void register_metric(const char *plugin, const char *metric, const char *type, int interval) {
    metric_t *m = malloc(sizeof(metric_t));
    memset(m, 0, sizeof(metric_t));
    strncpy(m->plugin, plugin, sizeof(m->plugin) - 1);
    strncpy(m->name, metric, sizeof(m->name) - 1);
    strncpy(m->type, type, sizeof(m->type) - 1);
    m->interval = interval;
    m->next = metrics;
    metrics = m;
    printf("[core] Registered metric: %-12s (plugin=%s, type=%s, interval=%d)\n",
           m->name, m->plugin, m->type, m->interval);
}

// ---------------- JSON 解析（极简实现） ----------------
// 因为不引入外部库，我们用简单字符串匹配解析 json 配置
char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = 0;
    fclose(f);
    return buf;
}

void parse_json_and_register(const char *plugin_name, const char *json_path, register_metric_cb_t cb) {
    char *data = read_file(json_path);
    if (!data) {
        fprintf(stderr, "[core] Cannot open %s\n", json_path);
        return;
    }
    printf("[core] Parsing JSON config for %s ...\n", plugin_name);

    char *metrics_array = strstr(data, "\"metrics\"");
    if (!metrics_array) {
        free(data);
        return;
    }

    char *p = metrics_array;
    while ((p = strstr(p, "{"))) {
        char *end = strstr(p, "}");
        if (!end) break;

        char block[512] = {0};
        strncpy(block, p, end - p);

        char metric[64] = {0}, type[32] = {0};
        int interval = 0;

        sscanf(block, "%*[^\"]\"name\"%*[^\"]\"%63[^\"]", metric);
        sscanf(block, "%*[^\"]\"type\"%*[^\"]\"%31[^\"]", type);
        sscanf(block, "%*[^\"]\"interval\"%*[^0-9]%d", &interval);

        if (strlen(metric) > 0)
            cb(plugin_name, metric, type, interval);

        p = end + 1;
    }
    free(data);
}

// ---------------- 调度线程 ----------------
void *scheduler_thread(void *arg) {
    printf("[core] Scheduler started.\n");
    while (running) {
        time_t now = time(NULL);
        for (metric_t *m = metrics; m; m = m->next) {
            if (difftime(now, m->last_run) < m->interval)
                continue;
            m->last_run = now;

            for (plugin_t *p = plugins; p; p = p->next) {
                if (strcmp(p->name, m->plugin) == 0 && p->read) {
                    printf("[core] Dispatching %s.read(%s)\n", p->name, m->name);
                    p->read(m->name);
                }
            }
        }
        sleep(1);
    }
    return NULL;
}

// ---------------- 插件加载 ----------------
void load_all_plugins(const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!strstr(ent->d_name, ".so"))
            continue;

        char so_path[512], json_path[512];
        snprintf(so_path, sizeof(so_path), "%s/%s", dirpath, ent->d_name);
        snprintf(json_path, sizeof(json_path), "%s/%.*s.json", dirpath,
                 (int)(strchr(ent->d_name, '.') - ent->d_name), ent->d_name);

        printf("[core] Loading plugin: %s\n", so_path);
        void *handle = dlopen(so_path, RTLD_NOW | RTLD_GLOBAL);
        if (!handle) {
            fprintf(stderr, "dlopen failed: %s\n", dlerror());
            continue;
        }

        void (*module_register)(void) = dlsym(handle, "module_register");
        if (!module_register) {
            fprintf(stderr, "No module_register() in %s\n", so_path);
            dlclose(handle);
            continue;
        }

        // 调用插件注册函数
        module_register();

        // 读取对应的 json 配置文件
        char plugin_name[128];
        sscanf(ent->d_name, "%127[^.]", plugin_name);
        parse_json_and_register(plugin_name, json_path, register_metric);
    }
    closedir(dir);
}

// ---------------- 主程序入口 ----------------
int main(void) {
    printf("[core] === JSON-driven Plugin Framework ===\n");

    load_all_plugins("./plugins");

    pthread_t tid;
    pthread_create(&tid, NULL, scheduler_thread, NULL);

    sleep(12);
    running = 0;
    pthread_join(tid, NULL);

    printf("[core] === Shutdown complete ===\n");
    return 0;
}

// plugins/plugin_cpu.c
#include <stdio.h>
#include <string.h>

void register_plugin_read(const char *name, void (*cb)(const char *));
void register_metric(const char *plugin, const char *metric, const char *type, int interval);

static void cpu_read(const char *metric_name) {
    if (strcmp(metric_name, "cpu_usage") == 0)
        printf("[plugin_cpu] metric=%s => %.2f%%\n", metric_name, 42.7);
    else if (strcmp(metric_name, "mem_free") == 0)
        printf("[plugin_cpu] metric=%s => %.2fMB\n", metric_name, 2048.0);
    else
        printf("[plugin_cpu] Unknown metric: %s\n", metric_name);
}

void module_register(void) {
    printf("[plugin_cpu] module_register()\n");
    register_plugin_read("plugin_cpu", cpu_read);
}

// plugins/plugin_temp.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void register_plugin_read(const char *name, void (*cb)(const char *));
void register_metric(const char *plugin, const char *metric, const char *type, int interval);

static void temp_read(const char *metric_name) {
    if (strcmp(metric_name, "temp_sensor") == 0)
        printf("[plugin_temp] metric=%s => %.2f°C\n", metric_name, 25.0 + rand() % 5);
    else if (strcmp(metric_name, "humidity") == 0)
        printf("[plugin_temp] metric=%s => %.1f%%\n", metric_name, 40.0 + rand() % 10);
}

void module_register(void) {
    printf("[plugin_temp] module_register()\n");
    register_plugin_read("plugin_temp", temp_read);
}
```

- plugin_cpu.json, plugin_temp.json
``json
// plugin_cpu.json
{
  "plugin": "plugin_cpu",
  "metrics": [
    { "name": "cpu_usage", "type": "gauge", "interval": 2, "description": "CPU usage percentage" },
    { "name": "mem_free",  "type": "gauge", "interval": 5, "description": "Free memory in MB" }
  ]
}

// plugin_temp.json
{
  "plugin": "plugin_temp",
  "metrics": [
    { "name": "temp_sensor", "type": "gauge", "interval": 3, "description": "Temperature in Celsius" },
    { "name": "humidity", "type": "gauge", "interval": 4, "description": "Relative humidity in %" }
  ]
}
``

- compile and run
```bash
mkdir -p plugins

gcc -fPIC -shared -o plugins/plugin_cpu.so plugins/plugin_cpu.c
gcc -fPIC -shared -o plugins/plugin_temp.so plugins/plugin_temp.c
gcc -rdynamic -o main main.c -ldl -pthread

./main

```

## collectd 选项
```bash
# collectd -h
Usage: collectd [OPTIONS]

Available options:
  General:
    -C <file>       Configuration file.
                    Default: /etc/collectd.conf
    -t              Test config and exit.
    -T              Test plugin read and exit.
    -P <file>       PID-file.
                    Default: /var/run/collectd.pid
    -f              Don't fork to the background.
    -B              Don't create the BaseDir
    -h              Display help (this message)

Builtin defaults:
  Config file       /etc/collectd.conf
  PID file          /var/run/collectd.pid
  Plugin directory  /usr/lib/collectd
  Data directory    /var/lib/collectd

collectd 5.12.0.git, http://collectd.org/
by Florian octo Forster <octo@collectd.org>

```
[Collectd 101](https://wiki.anuket.io/display/HOME/Collectd+101)  
[collectd.conf(5)](https://www.collectd.org/documentation/manpages/collectd.conf.html)  
[types.db(5)](https://www.collectd.org/documentation/manpages/types.db.html)  
[Plugin architecture](https://github.com/collectd/collectd/wiki/Plugin-architecture)  
[Adding Labels to Collectd Metrics](https://github.com/collectd/collectd/issues/3094)  