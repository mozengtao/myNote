# C语言 时区相关函数完整总结（setenv/tzset/localtime 及全套相关函数）
## 一、核心基础函数（进程时区设置必备）
### 1. setenv
- 原型：int setenv(const char *name, const char *value, int overwrite);
- 作用：仅修改**进程级** TZ 环境变量字符串，不触发时区计算，不影响系统全局时区
- 参数：name="TZ"，value=时区名（如America/New_York），overwrite=1（覆盖）
- 返回：0成功，-1失败
- 注意：只修改变量，不生效

### 2. tzset
- 原型：void tzset(void);
- 作用：读取 TZ 环境变量，加载时区文件，计算偏移/夏令时，**刷新全局时区数据**
- 必须调用：动态修改 TZ 后必须手动调用，时区才真正生效
- 初始化全局变量：tzname[0]/tzname[1]、timezone、daylight

### 3. localtime
- 原型：struct tm *localtime(const time_t *timep);
- 作用：将时间戳转为**当前进程本地时区时间**
- 依赖：完全依赖 tzset 刷新后的时区数据，不直接读取 TZ
- 非线程安全

## 二、线程安全版本函数
### 1. localtime_r
- 原型：struct tm *localtime_r(const time_t *timep, struct tm *result);
- 作用：localtime 的线程安全版本，多线程环境必须使用

### 2. ctime_r / asctime_r
- 作用：对应 ctime/asctime 的线程安全版本

## 三、时间转换类函数（时区强相关）
### 1. gmtime / gmtime_r
- 作用：转换为 UTC 时间，**不受时区设置影响**

### 2. mktime
- 原型：time_t mktime(struct tm *tm);
- 作用：将本地时区 tm 结构体转回时间戳，自动处理时区偏移
- 依赖：tzset 设置的时区规则

### 3. timelocal / timelocal_r
- 作用：等价 mktime，将本地时间转为时间戳

## 四、时区信息获取（全局变量 + 辅助函数）
### 1. 全局时区变量（tzset 初始化）
- tzname[0]：标准时区名（EST）
- tzname[1]：夏令时时区名（EDT）
- timezone：UTC 偏移秒数
- daylight：是否启用夏令时（1=是）

### 2. ctime / asctime
- 作用：快速输出本地时间字符串，依赖当前时区

## 五、格式化时间函数
### 1. strftime
- 作用：格式化时间，支持 %Z（时区名）、%z（时区偏移）

### 2. strptime
- 作用：解析时间字符串为 tm 结构体，可识别时区

## 六、环境变量操作函数
### 1. getenv
- 原型：char *getenv("TZ");
- 作用：获取当前进程的时区环境变量值

### 2. unsetenv
- 原型：int unsetenv("TZ");
- 作用：清除 TZ 变量，恢复系统默认时区，需配合 tzset() 生效

## 七、高级 POSIX 时区函数
### 1. tzset_l / localtime_l
- 作用：显式指定区域/时区，不依赖全局 TZ 环境变量

### 2. clock_gettime
- 作用：获取高精度时间，配合 localtime_r 转换为本地时区

## 八、标准使用流程（进程独立时区设置）
1. 设置时区环境变量：setenv("TZ", "America/New_York", 1);
2. 刷新时区生效：tzset();
3. 使用本地时间：localtime_r(&now, &tm_result);

## 九、核心规则
1. setenv 只改字符串，tzset 才让时区生效
2. 所有本地时间函数依赖 tzset 结果
3. 多线程必须用 _r 后缀安全函数
4. UTC 函数(gmtime)不受时区影响
5. 进程级时区设置，不影响系统全局