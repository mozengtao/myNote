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