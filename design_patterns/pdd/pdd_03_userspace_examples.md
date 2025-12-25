# PDD in User-Space C: Complete Examples

## Introduction

```
+------------------------------------------------------------------+
|  APPLYING KERNEL PDD PATTERNS TO USER-SPACE                      |
+------------------------------------------------------------------+

    The same patterns work in user-space:
    
    1. OPS TABLES (function pointer structs) as interfaces
    2. REGISTRATION for pluggable implementations
    3. CONTEXT OBJECTS for per-instance state
    4. STRICT DEPENDENCY DIRECTION
    
    Key difference: User-space adds header file organization
```

---

## Example 1: CLI Tool with Pluggable Storage Backend

### Directory Layout

```
storage_tool/
├── include/
│   ├── presentation/
│   │   └── cli.h              # Command-line interface
│   ├── domain/
│   │   ├── storage.h          # Storage domain interface
│   │   └── validator.h        # Validation rules
│   └── data/
│       ├── storage_ops.h      # Storage operations interface
│       ├── file_storage.h     # File backend
│       └── sqlite_storage.h   # SQLite backend
├── src/
│   ├── presentation/
│   │   └── cli.c
│   ├── domain/
│   │   ├── storage.c
│   │   └── validator.c
│   └── data/
│       ├── file_storage.c
│       └── sqlite_storage.c
└── main.c
```

### Domain Layer (Core Business Logic)

```c
/* include/domain/storage.h - DOMAIN interface */
#ifndef DOMAIN_STORAGE_H
#define DOMAIN_STORAGE_H

#include <stddef.h>
#include <stdbool.h>

/* Domain entity */
struct record {
    unsigned int id;
    char key[64];
    char value[256];
    unsigned long timestamp;
};

/* Result types - domain concepts */
enum storage_error {
    STORAGE_OK = 0,
    STORAGE_NOT_FOUND,
    STORAGE_DUPLICATE,
    STORAGE_INVALID,
    STORAGE_FULL,
    STORAGE_IO_ERROR,
};

/*
 * Storage operations interface - DATA LAYER MUST IMPLEMENT THIS
 * Domain defines WHAT is needed, Data provides HOW
 */
struct storage_ops {
    const char *name;
    
    int (*init)(void *ctx, const char *path);
    void (*cleanup)(void *ctx);
    
    enum storage_error (*put)(void *ctx, const struct record *rec);
    enum storage_error (*get)(void *ctx, unsigned int id, struct record *out);
    enum storage_error (*del)(void *ctx, unsigned int id);
    enum storage_error (*list)(void *ctx, struct record *out, 
                               size_t max, size_t *count);
};

/*
 * Domain storage context
 * Holds reference to ops (interface) and ctx (implementation state)
 */
struct storage {
    const struct storage_ops *ops;  /* Interface */
    void *ctx;                       /* Implementation-specific state */
    size_t record_count;            /* Domain tracks this */
    size_t max_records;             /* Domain policy */
};

/* DOMAIN API - Presentation calls these */
int storage_open(struct storage *s, const struct storage_ops *ops,
                 void *ctx, const char *path, size_t max_records);
void storage_close(struct storage *s);

enum storage_error storage_add(struct storage *s, const char *key, 
                               const char *value);
enum storage_error storage_find(struct storage *s, unsigned int id,
                                struct record *out);
enum storage_error storage_remove(struct storage *s, unsigned int id);
enum storage_error storage_list_all(struct storage *s, struct record *out,
                                    size_t max, size_t *count);

/* Domain utilities */
const char *storage_error_str(enum storage_error err);

#endif /* DOMAIN_STORAGE_H */
```

```c
/* src/domain/storage.c - DOMAIN implementation */
#include "domain/storage.h"
#include "domain/validator.h"
#include <string.h>
#include <time.h>

static unsigned int next_id = 1;

int storage_open(struct storage *s, const struct storage_ops *ops,
                 void *ctx, const char *path, size_t max_records)
{
    if (!s || !ops || !ctx)
        return -1;
    
    s->ops = ops;
    s->ctx = ctx;
    s->max_records = max_records;
    s->record_count = 0;
    
    /* Delegate initialization to data layer */
    return ops->init(ctx, path);
}

void storage_close(struct storage *s)
{
    if (s && s->ops && s->ops->cleanup)
        s->ops->cleanup(s->ctx);
}

enum storage_error storage_add(struct storage *s, const char *key,
                               const char *value)
{
    /* DOMAIN POLICY: Check capacity */
    if (s->record_count >= s->max_records)
        return STORAGE_FULL;
    
    /* DOMAIN POLICY: Validate input */
    if (!validate_key(key))
        return STORAGE_INVALID;
    if (!validate_value(value))
        return STORAGE_INVALID;
    
    /* Build domain entity */
    struct record rec = {
        .id = next_id++,
        .timestamp = (unsigned long)time(NULL),
    };
    strncpy(rec.key, key, sizeof(rec.key) - 1);
    strncpy(rec.value, value, sizeof(rec.value) - 1);
    
    /* Delegate storage to data layer */
    enum storage_error err = s->ops->put(s->ctx, &rec);
    
    /* DOMAIN tracks state */
    if (err == STORAGE_OK)
        s->record_count++;
    
    return err;
}

enum storage_error storage_find(struct storage *s, unsigned int id,
                                struct record *out)
{
    /* DOMAIN POLICY: Validate ID */
    if (id == 0)
        return STORAGE_INVALID;
    
    /* Delegate to data layer */
    return s->ops->get(s->ctx, id, out);
}

enum storage_error storage_remove(struct storage *s, unsigned int id)
{
    /* DOMAIN POLICY: Validate ID */
    if (id == 0)
        return STORAGE_INVALID;
    
    enum storage_error err = s->ops->del(s->ctx, id);
    
    if (err == STORAGE_OK)
        s->record_count--;
    
    return err;
}

const char *storage_error_str(enum storage_error err)
{
    switch (err) {
    case STORAGE_OK:        return "success";
    case STORAGE_NOT_FOUND: return "record not found";
    case STORAGE_DUPLICATE: return "duplicate key";
    case STORAGE_INVALID:   return "invalid input";
    case STORAGE_FULL:      return "storage full";
    case STORAGE_IO_ERROR:  return "I/O error";
    default:                return "unknown error";
    }
}
```

### Data Layer (Storage Backends)

```c
/* include/data/storage_ops.h - Forward declaration */
#ifndef DATA_STORAGE_OPS_H
#define DATA_STORAGE_OPS_H

#include "domain/storage.h"

/* Available backends */
extern const struct storage_ops file_storage_ops;
extern const struct storage_ops sqlite_storage_ops;

/* Backend context allocation */
void *file_storage_create(void);
void file_storage_destroy(void *ctx);

void *sqlite_storage_create(void);
void sqlite_storage_destroy(void *ctx);

#endif
```

```c
/* src/data/file_storage.c - FILE backend implementation */
#include "data/storage_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct file_ctx {
    char path[256];
    FILE *fp;
    struct record *cache;
    size_t cache_size;
    size_t cache_cap;
};

void *file_storage_create(void)
{
    struct file_ctx *ctx = calloc(1, sizeof(*ctx));
    if (ctx) {
        ctx->cache_cap = 1024;
        ctx->cache = calloc(ctx->cache_cap, sizeof(struct record));
    }
    return ctx;
}

void file_storage_destroy(void *ptr)
{
    struct file_ctx *ctx = ptr;
    if (ctx) {
        free(ctx->cache);
        free(ctx);
    }
}

static int file_init(void *ptr, const char *path)
{
    struct file_ctx *ctx = ptr;
    strncpy(ctx->path, path, sizeof(ctx->path) - 1);
    
    /* Load existing records */
    ctx->fp = fopen(path, "rb");
    if (ctx->fp) {
        while (fread(&ctx->cache[ctx->cache_size], sizeof(struct record), 
                     1, ctx->fp) == 1) {
            ctx->cache_size++;
        }
        fclose(ctx->fp);
    }
    
    return 0;
}

static void file_cleanup(void *ptr)
{
    struct file_ctx *ctx = ptr;
    
    /* Persist all records */
    FILE *fp = fopen(ctx->path, "wb");
    if (fp) {
        fwrite(ctx->cache, sizeof(struct record), ctx->cache_size, fp);
        fclose(fp);
    }
}

static enum storage_error file_put(void *ptr, const struct record *rec)
{
    struct file_ctx *ctx = ptr;
    
    if (ctx->cache_size >= ctx->cache_cap)
        return STORAGE_FULL;
    
    ctx->cache[ctx->cache_size++] = *rec;
    return STORAGE_OK;
}

static enum storage_error file_get(void *ptr, unsigned int id,
                                   struct record *out)
{
    struct file_ctx *ctx = ptr;
    
    for (size_t i = 0; i < ctx->cache_size; i++) {
        if (ctx->cache[i].id == id) {
            *out = ctx->cache[i];
            return STORAGE_OK;
        }
    }
    return STORAGE_NOT_FOUND;
}

static enum storage_error file_del(void *ptr, unsigned int id)
{
    struct file_ctx *ctx = ptr;
    
    for (size_t i = 0; i < ctx->cache_size; i++) {
        if (ctx->cache[i].id == id) {
            /* Shift remaining records */
            memmove(&ctx->cache[i], &ctx->cache[i + 1],
                    (ctx->cache_size - i - 1) * sizeof(struct record));
            ctx->cache_size--;
            return STORAGE_OK;
        }
    }
    return STORAGE_NOT_FOUND;
}

static enum storage_error file_list(void *ptr, struct record *out,
                                    size_t max, size_t *count)
{
    struct file_ctx *ctx = ptr;
    
    *count = (ctx->cache_size < max) ? ctx->cache_size : max;
    memcpy(out, ctx->cache, *count * sizeof(struct record));
    return STORAGE_OK;
}

/* Export ops table - DATA implements DOMAIN interface */
const struct storage_ops file_storage_ops = {
    .name    = "file",
    .init    = file_init,
    .cleanup = file_cleanup,
    .put     = file_put,
    .get     = file_get,
    .del     = file_del,
    .list    = file_list,
};
```

### Presentation Layer (CLI)

```c
/* include/presentation/cli.h */
#ifndef PRESENTATION_CLI_H
#define PRESENTATION_CLI_H

/* CLI entry point - parses args, calls domain */
int cli_run(int argc, char **argv);

#endif
```

```c
/* src/presentation/cli.c - PRESENTATION layer */
#include "presentation/cli.h"
#include "domain/storage.h"
#include "data/storage_ops.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <backend> <path> <command> [args]\n", prog);
    fprintf(stderr, "Backends: file, sqlite\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  add <key> <value>   Add a record\n");
    fprintf(stderr, "  get <id>            Get a record\n");
    fprintf(stderr, "  del <id>            Delete a record\n");
    fprintf(stderr, "  list                List all records\n");
}

static void print_record(const struct record *r)
{
    printf("[%u] %s = %s (ts=%lu)\n", r->id, r->key, r->value, r->timestamp);
}

int cli_run(int argc, char **argv)
{
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *backend = argv[1];
    const char *path = argv[2];
    const char *command = argv[3];
    
    /* PRESENTATION: Select backend based on user input */
    const struct storage_ops *ops;
    void *ctx;
    
    if (strcmp(backend, "file") == 0) {
        ops = &file_storage_ops;
        ctx = file_storage_create();
    } else if (strcmp(backend, "sqlite") == 0) {
        ops = &sqlite_storage_ops;
        ctx = sqlite_storage_create();
    } else {
        fprintf(stderr, "Unknown backend: %s\n", backend);
        return 1;
    }
    
    /* Initialize domain storage */
    struct storage storage;
    if (storage_open(&storage, ops, ctx, path, 10000) < 0) {
        fprintf(stderr, "Failed to open storage\n");
        return 1;
    }
    
    enum storage_error err = STORAGE_OK;
    
    /* PRESENTATION: Dispatch commands to domain */
    if (strcmp(command, "add") == 0 && argc >= 6) {
        err = storage_add(&storage, argv[4], argv[5]);
        if (err == STORAGE_OK)
            printf("Record added\n");
        else
            fprintf(stderr, "Error: %s\n", storage_error_str(err));
            
    } else if (strcmp(command, "get") == 0 && argc >= 5) {
        struct record rec;
        err = storage_find(&storage, atoi(argv[4]), &rec);
        if (err == STORAGE_OK)
            print_record(&rec);
        else
            fprintf(stderr, "Error: %s\n", storage_error_str(err));
            
    } else if (strcmp(command, "del") == 0 && argc >= 5) {
        err = storage_remove(&storage, atoi(argv[4]));
        if (err == STORAGE_OK)
            printf("Record deleted\n");
        else
            fprintf(stderr, "Error: %s\n", storage_error_str(err));
            
    } else if (strcmp(command, "list") == 0) {
        struct record records[100];
        size_t count;
        err = storage_list_all(&storage, records, 100, &count);
        if (err == STORAGE_OK) {
            printf("Found %zu records:\n", count);
            for (size_t i = 0; i < count; i++)
                print_record(&records[i]);
        }
    } else {
        print_usage(argv[0]);
    }
    
    storage_close(&storage);
    
    /* Cleanup backend */
    if (strcmp(backend, "file") == 0)
        file_storage_destroy(ctx);
    else
        sqlite_storage_destroy(ctx);
    
    return (err == STORAGE_OK) ? 0 : 1;
}
```

---

## Example 2: Embedded HAL with Business Rules

### Directory Layout

```
sensor_system/
├── include/
│   ├── hal/                    # Hardware Abstraction (DATA)
│   │   ├── sensor_ops.h        # Sensor interface
│   │   └── gpio_ops.h          # GPIO interface
│   ├── domain/
│   │   ├── monitor.h           # Monitoring rules
│   │   └── alarm.h             # Alarm policies
│   └── app/                    # Application (PRESENTATION)
│       └── console.h
├── src/
│   ├── hal/
│   │   ├── sensor_adc.c        # ADC sensor driver
│   │   └── gpio_linux.c        # Linux GPIO
│   ├── domain/
│   │   ├── monitor.c
│   │   └── alarm.c
│   └── app/
│       └── console.c
└── main.c
```

### HAL Layer (Data)

```c
/* include/hal/sensor_ops.h - DATA interface */
#ifndef HAL_SENSOR_OPS_H
#define HAL_SENSOR_OPS_H

#include <stdint.h>

/* Sensor reading - raw data */
struct sensor_reading {
    uint32_t raw_value;      /* ADC counts or similar */
    uint64_t timestamp_us;   /* Microseconds */
    uint8_t channel;
    uint8_t status;          /* 0 = OK, else error */
};

/* Sensor operations - DATA layer implements */
struct sensor_ops {
    const char *name;
    
    int (*init)(void *ctx, int channel);
    void (*deinit)(void *ctx);
    
    int (*read)(void *ctx, struct sensor_reading *out);
    int (*calibrate)(void *ctx, int offset);
};

/* Available sensor drivers */
extern const struct sensor_ops adc_sensor_ops;
extern const struct sensor_ops i2c_sensor_ops;

#endif
```

```c
/* src/hal/sensor_adc.c - ADC sensor implementation */
#include "hal/sensor_ops.h"
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

struct adc_ctx {
    int channel;
    int fd;
    int calibration_offset;
};

static int adc_init(void *ptr, int channel)
{
    struct adc_ctx *ctx = ptr;
    ctx->channel = channel;
    ctx->calibration_offset = 0;
    
    /* Open ADC device (simulated path) */
    char path[64];
    snprintf(path, sizeof(path), "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw", channel);
    ctx->fd = open(path, O_RDONLY);
    
    /* For demo, allow failure */
    return 0;
}

static void adc_deinit(void *ptr)
{
    struct adc_ctx *ctx = ptr;
    if (ctx->fd >= 0)
        close(ctx->fd);
}

static int adc_read(void *ptr, struct sensor_reading *out)
{
    struct adc_ctx *ctx = ptr;
    struct timespec ts;
    
    clock_gettime(CLOCK_MONOTONIC, &ts);
    out->timestamp_us = ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
    out->channel = ctx->channel;
    
    if (ctx->fd >= 0) {
        char buf[32];
        lseek(ctx->fd, 0, SEEK_SET);
        ssize_t n = read(ctx->fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            out->raw_value = atoi(buf) + ctx->calibration_offset;
            out->status = 0;
            return 0;
        }
    }
    
    /* Simulate reading if hardware not available */
    out->raw_value = 2048 + (rand() % 200) - 100 + ctx->calibration_offset;
    out->status = 0;
    return 0;
}

static int adc_calibrate(void *ptr, int offset)
{
    struct adc_ctx *ctx = ptr;
    ctx->calibration_offset = offset;
    return 0;
}

const struct sensor_ops adc_sensor_ops = {
    .name      = "adc",
    .init      = adc_init,
    .deinit    = adc_deinit,
    .read      = adc_read,
    .calibrate = adc_calibrate,
};

void *adc_sensor_create(void)
{
    return calloc(1, sizeof(struct adc_ctx));
}

void adc_sensor_destroy(void *ptr)
{
    free(ptr);
}
```

### Domain Layer (Business Rules)

```c
/* include/domain/monitor.h - DOMAIN interface */
#ifndef DOMAIN_MONITOR_H
#define DOMAIN_MONITOR_H

#include "hal/sensor_ops.h"

/* Domain entity: processed measurement */
struct measurement {
    double temperature_c;    /* Converted to physical units */
    double voltage_v;
    uint64_t timestamp_us;
    uint8_t quality;         /* 0-100 quality score */
};

/* Domain policies */
enum alarm_level {
    ALARM_NONE = 0,
    ALARM_WARNING,
    ALARM_CRITICAL,
    ALARM_EMERGENCY,
};

/* Alarm handler callback - presentation implements */
typedef void (*alarm_handler_t)(enum alarm_level level, 
                                const char *message,
                                void *user_data);

/* Monitor context */
struct monitor {
    const struct sensor_ops *sensor_ops;
    void *sensor_ctx;
    
    /* Domain configuration (policies) */
    double warning_threshold;
    double critical_threshold;
    double emergency_threshold;
    unsigned int averaging_samples;
    
    /* Domain state */
    double running_average;
    unsigned int sample_count;
    enum alarm_level current_alarm;
    
    /* Callback to presentation */
    alarm_handler_t alarm_handler;
    void *alarm_user_data;
};

/* Domain API */
int monitor_init(struct monitor *m, 
                 const struct sensor_ops *ops, void *ctx,
                 int channel);
void monitor_cleanup(struct monitor *m);

/* Configure domain policies */
void monitor_set_thresholds(struct monitor *m,
                            double warning, double critical, double emergency);
void monitor_set_averaging(struct monitor *m, unsigned int samples);
void monitor_set_alarm_handler(struct monitor *m, 
                               alarm_handler_t handler, void *user_data);

/* Domain operations */
int monitor_sample(struct monitor *m, struct measurement *out);
enum alarm_level monitor_check_alarms(struct monitor *m);

#endif
```

```c
/* src/domain/monitor.c - DOMAIN implementation */
#include "domain/monitor.h"
#include <string.h>
#include <stdio.h>

/* Domain constants */
#define ADC_REFERENCE_MV  3300
#define ADC_RESOLUTION    4096
#define MV_PER_DEGREE     10.0

/* Convert raw ADC to temperature - DOMAIN logic */
static double raw_to_temperature(uint32_t raw)
{
    double voltage_mv = (raw * ADC_REFERENCE_MV) / (double)ADC_RESOLUTION;
    return (voltage_mv - 500.0) / MV_PER_DEGREE;  /* LM35 formula */
}

int monitor_init(struct monitor *m, 
                 const struct sensor_ops *ops, void *ctx,
                 int channel)
{
    memset(m, 0, sizeof(*m));
    m->sensor_ops = ops;
    m->sensor_ctx = ctx;
    
    /* Default domain policies */
    m->warning_threshold = 50.0;
    m->critical_threshold = 70.0;
    m->emergency_threshold = 85.0;
    m->averaging_samples = 10;
    
    return ops->init(ctx, channel);
}

void monitor_cleanup(struct monitor *m)
{
    if (m->sensor_ops)
        m->sensor_ops->deinit(m->sensor_ctx);
}

void monitor_set_thresholds(struct monitor *m,
                            double warning, double critical, double emergency)
{
    /* DOMAIN validation */
    if (warning < critical && critical < emergency) {
        m->warning_threshold = warning;
        m->critical_threshold = critical;
        m->emergency_threshold = emergency;
    }
}

void monitor_set_averaging(struct monitor *m, unsigned int samples)
{
    if (samples > 0 && samples <= 100)
        m->averaging_samples = samples;
}

void monitor_set_alarm_handler(struct monitor *m,
                               alarm_handler_t handler, void *user_data)
{
    m->alarm_handler = handler;
    m->alarm_user_data = user_data;
}

int monitor_sample(struct monitor *m, struct measurement *out)
{
    struct sensor_reading raw;
    
    /* Get raw data from DATA layer */
    if (m->sensor_ops->read(m->sensor_ctx, &raw) < 0)
        return -1;
    
    if (raw.status != 0) {
        out->quality = 0;
        return -1;
    }
    
    /* DOMAIN: Convert to physical units */
    out->temperature_c = raw_to_temperature(raw.raw_value);
    out->voltage_v = (raw.raw_value * ADC_REFERENCE_MV) / 
                     (double)(ADC_RESOLUTION * 1000);
    out->timestamp_us = raw.timestamp_us;
    
    /* DOMAIN: Update running average */
    double alpha = 1.0 / m->averaging_samples;
    if (m->sample_count == 0)
        m->running_average = out->temperature_c;
    else
        m->running_average = alpha * out->temperature_c + 
                            (1 - alpha) * m->running_average;
    m->sample_count++;
    
    /* DOMAIN: Calculate quality based on variance */
    out->quality = 95;  /* Simplified */
    
    return 0;
}

enum alarm_level monitor_check_alarms(struct monitor *m)
{
    double temp = m->running_average;
    enum alarm_level new_level;
    
    /* DOMAIN POLICY: Determine alarm level */
    if (temp >= m->emergency_threshold)
        new_level = ALARM_EMERGENCY;
    else if (temp >= m->critical_threshold)
        new_level = ALARM_CRITICAL;
    else if (temp >= m->warning_threshold)
        new_level = ALARM_WARNING;
    else
        new_level = ALARM_NONE;
    
    /* DOMAIN: Notify presentation if level changed */
    if (new_level != m->current_alarm && m->alarm_handler) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Temperature %.1f°C", temp);
        m->alarm_handler(new_level, msg, m->alarm_user_data);
    }
    
    m->current_alarm = new_level;
    return new_level;
}
```

---

## Example 3: Network Service with Protocol/Logic Separation

### Directory Layout

```
keyvalue_server/
├── include/
│   ├── protocol/              # PRESENTATION
│   │   ├── parser.h
│   │   └── response.h
│   ├── domain/
│   │   ├── store.h
│   │   └── session.h
│   └── transport/             # DATA
│       └── tcp_server.h
├── src/
│   ├── protocol/
│   │   ├── parser.c
│   │   └── response.c
│   ├── domain/
│   │   ├── store.c
│   │   └── session.c
│   └── transport/
│       └── tcp_server.c
└── main.c
```

### Protocol Layer (Presentation)

```c
/* include/protocol/parser.h - PRESENTATION */
#ifndef PROTOCOL_PARSER_H
#define PROTOCOL_PARSER_H

#include <stddef.h>

/* Protocol: GET key\r\n, SET key value\r\n, DEL key\r\n */

enum cmd_type {
    CMD_UNKNOWN = 0,
    CMD_GET,
    CMD_SET,
    CMD_DEL,
    CMD_QUIT,
};

struct parsed_cmd {
    enum cmd_type type;
    char key[256];
    char value[1024];
};

/* Parse raw input into command structure */
int protocol_parse(const char *input, size_t len, struct parsed_cmd *out);

/* Format response for protocol */
int protocol_format_ok(char *buf, size_t size);
int protocol_format_value(char *buf, size_t size, const char *value);
int protocol_format_error(char *buf, size_t size, const char *msg);
int protocol_format_nil(char *buf, size_t size);

#endif
```

```c
/* src/protocol/parser.c - PRESENTATION layer */
#include "protocol/parser.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* PRESENTATION: Parse wire protocol into domain terms */
int protocol_parse(const char *input, size_t len, struct parsed_cmd *out)
{
    memset(out, 0, sizeof(*out));
    
    /* Skip whitespace */
    while (len > 0 && isspace(*input)) {
        input++;
        len--;
    }
    
    if (len == 0)
        return -1;
    
    /* Parse command */
    if (strncasecmp(input, "GET ", 4) == 0) {
        out->type = CMD_GET;
        input += 4;
        sscanf(input, "%255s", out->key);
        
    } else if (strncasecmp(input, "SET ", 4) == 0) {
        out->type = CMD_SET;
        input += 4;
        /* Parse: key value */
        int n = sscanf(input, "%255s %1023[^\r\n]", out->key, out->value);
        if (n < 2)
            return -1;
            
    } else if (strncasecmp(input, "DEL ", 4) == 0) {
        out->type = CMD_DEL;
        input += 4;
        sscanf(input, "%255s", out->key);
        
    } else if (strncasecmp(input, "QUIT", 4) == 0) {
        out->type = CMD_QUIT;
        
    } else {
        out->type = CMD_UNKNOWN;
        return -1;
    }
    
    return 0;
}

/* PRESENTATION: Format responses */
int protocol_format_ok(char *buf, size_t size)
{
    return snprintf(buf, size, "+OK\r\n");
}

int protocol_format_value(char *buf, size_t size, const char *value)
{
    return snprintf(buf, size, "$%zu\r\n%s\r\n", strlen(value), value);
}

int protocol_format_error(char *buf, size_t size, const char *msg)
{
    return snprintf(buf, size, "-ERR %s\r\n", msg);
}

int protocol_format_nil(char *buf, size_t size)
{
    return snprintf(buf, size, "$-1\r\n");
}
```

### Domain Layer (Store Logic)

```c
/* include/domain/store.h - DOMAIN */
#ifndef DOMAIN_STORE_H
#define DOMAIN_STORE_H

#include <stdbool.h>
#include <stddef.h>

/* Domain result codes */
enum store_result {
    STORE_OK,
    STORE_NOT_FOUND,
    STORE_KEY_EXISTS,
    STORE_ERROR,
};

/* Domain entity */
struct store_entry {
    char key[256];
    char value[1024];
    unsigned long created_at;
    unsigned long accessed_at;
    unsigned int access_count;
};

/* Store operations - allows different backends */
struct store_backend {
    int (*put)(void *ctx, const char *key, const char *value);
    int (*get)(void *ctx, const char *key, char *value, size_t size);
    int (*del)(void *ctx, const char *key);
};

struct store {
    const struct store_backend *backend;
    void *backend_ctx;
    
    /* Domain policies */
    size_t max_key_len;
    size_t max_value_len;
    size_t max_entries;
    size_t current_entries;
};

/* Domain API */
int store_init(struct store *s, const struct store_backend *backend,
               void *ctx, size_t max_entries);
void store_cleanup(struct store *s);

enum store_result store_set(struct store *s, const char *key, const char *value);
enum store_result store_get(struct store *s, const char *key, 
                           char *value, size_t size);
enum store_result store_delete(struct store *s, const char *key);

#endif
```

```c
/* src/domain/store.c - DOMAIN implementation */
#include "domain/store.h"
#include <string.h>
#include <ctype.h>

/* DOMAIN: Validate key format */
static bool validate_key(const char *key, size_t max_len)
{
    size_t len = strlen(key);
    
    if (len == 0 || len > max_len)
        return false;
    
    /* DOMAIN POLICY: Keys must be alphanumeric + underscore */
    for (size_t i = 0; i < len; i++) {
        if (!isalnum(key[i]) && key[i] != '_')
            return false;
    }
    
    return true;
}

int store_init(struct store *s, const struct store_backend *backend,
               void *ctx, size_t max_entries)
{
    s->backend = backend;
    s->backend_ctx = ctx;
    s->max_key_len = 255;
    s->max_value_len = 1023;
    s->max_entries = max_entries;
    s->current_entries = 0;
    return 0;
}

enum store_result store_set(struct store *s, const char *key, const char *value)
{
    /* DOMAIN: Validate input */
    if (!validate_key(key, s->max_key_len))
        return STORE_ERROR;
    
    if (strlen(value) > s->max_value_len)
        return STORE_ERROR;
    
    /* DOMAIN: Check capacity policy */
    if (s->current_entries >= s->max_entries)
        return STORE_ERROR;
    
    /* Delegate to DATA layer */
    if (s->backend->put(s->backend_ctx, key, value) == 0) {
        s->current_entries++;
        return STORE_OK;
    }
    
    return STORE_ERROR;
}

enum store_result store_get(struct store *s, const char *key,
                           char *value, size_t size)
{
    if (!validate_key(key, s->max_key_len))
        return STORE_ERROR;
    
    if (s->backend->get(s->backend_ctx, key, value, size) == 0)
        return STORE_OK;
    
    return STORE_NOT_FOUND;
}

enum store_result store_delete(struct store *s, const char *key)
{
    if (!validate_key(key, s->max_key_len))
        return STORE_ERROR;
    
    if (s->backend->del(s->backend_ctx, key) == 0) {
        s->current_entries--;
        return STORE_OK;
    }
    
    return STORE_NOT_FOUND;
}
```

---

## Example 4: File Processing Pipeline

### Directory Layout

```
file_processor/
├── include/
│   ├── pipeline/              # DOMAIN
│   │   ├── processor.h
│   │   └── filter.h
│   ├── io/                    # DATA
│   │   ├── reader_ops.h
│   │   └── writer_ops.h
│   └── cli/                   # PRESENTATION
│       └── args.h
├── src/
│   ├── pipeline/
│   │   └── processor.c
│   ├── io/
│   │   ├── file_reader.c
│   │   ├── file_writer.c
│   │   └── gzip_writer.c
│   └── cli/
│       └── args.c
└── main.c
```

### Domain: Pipeline Processor

```c
/* include/pipeline/processor.h - DOMAIN */
#ifndef PIPELINE_PROCESSOR_H
#define PIPELINE_PROCESSOR_H

#include <stddef.h>
#include <stdbool.h>

/* I/O interfaces - DATA layer implements */
struct reader_ops {
    int (*open)(void *ctx, const char *path);
    ssize_t (*read)(void *ctx, void *buf, size_t size);
    void (*close)(void *ctx);
};

struct writer_ops {
    int (*open)(void *ctx, const char *path);
    ssize_t (*write)(void *ctx, const void *buf, size_t size);
    int (*flush)(void *ctx);
    void (*close)(void *ctx);
};

/* Filter function - DOMAIN policy */
typedef bool (*line_filter_fn)(const char *line, size_t len, void *ctx);
typedef void (*line_transform_fn)(char *line, size_t *len, void *ctx);

struct pipeline_stage {
    line_filter_fn filter;      /* Return true to keep line */
    line_transform_fn transform; /* Modify line in place */
    void *ctx;
};

/* Pipeline configuration - DOMAIN */
struct pipeline {
    const struct reader_ops *reader_ops;
    void *reader_ctx;
    
    const struct writer_ops *writer_ops;
    void *writer_ctx;
    
    struct pipeline_stage *stages;
    size_t stage_count;
    
    /* Statistics - DOMAIN tracks */
    size_t lines_read;
    size_t lines_written;
    size_t bytes_read;
    size_t bytes_written;
};

/* Domain API */
int pipeline_init(struct pipeline *p,
                  const struct reader_ops *reader, void *reader_ctx,
                  const struct writer_ops *writer, void *writer_ctx);

int pipeline_add_stage(struct pipeline *p, 
                       line_filter_fn filter,
                       line_transform_fn transform,
                       void *ctx);

int pipeline_run(struct pipeline *p, 
                 const char *input_path,
                 const char *output_path);

void pipeline_cleanup(struct pipeline *p);

#endif
```

```c
/* src/pipeline/processor.c - DOMAIN */
#include "pipeline/processor.h"
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 4096
#define MAX_STAGES 16

int pipeline_init(struct pipeline *p,
                  const struct reader_ops *reader, void *reader_ctx,
                  const struct writer_ops *writer, void *writer_ctx)
{
    memset(p, 0, sizeof(*p));
    p->reader_ops = reader;
    p->reader_ctx = reader_ctx;
    p->writer_ops = writer;
    p->writer_ctx = writer_ctx;
    p->stages = calloc(MAX_STAGES, sizeof(struct pipeline_stage));
    return p->stages ? 0 : -1;
}

int pipeline_add_stage(struct pipeline *p,
                       line_filter_fn filter,
                       line_transform_fn transform,
                       void *ctx)
{
    if (p->stage_count >= MAX_STAGES)
        return -1;
    
    p->stages[p->stage_count].filter = filter;
    p->stages[p->stage_count].transform = transform;
    p->stages[p->stage_count].ctx = ctx;
    p->stage_count++;
    return 0;
}

/* DOMAIN: Process a single line through all stages */
static bool process_line(struct pipeline *p, char *line, size_t *len)
{
    for (size_t i = 0; i < p->stage_count; i++) {
        struct pipeline_stage *stage = &p->stages[i];
        
        /* Apply filter */
        if (stage->filter && !stage->filter(line, *len, stage->ctx))
            return false;  /* Line filtered out */
        
        /* Apply transform */
        if (stage->transform)
            stage->transform(line, len, stage->ctx);
    }
    return true;
}

int pipeline_run(struct pipeline *p,
                 const char *input_path,
                 const char *output_path)
{
    char buf[MAX_LINE];
    char line[MAX_LINE];
    size_t line_len = 0;
    
    /* Open I/O through DATA layer */
    if (p->reader_ops->open(p->reader_ctx, input_path) < 0)
        return -1;
    
    if (p->writer_ops->open(p->writer_ctx, output_path) < 0) {
        p->reader_ops->close(p->reader_ctx);
        return -1;
    }
    
    /* DOMAIN: Process line by line */
    ssize_t n;
    while ((n = p->reader_ops->read(p->reader_ctx, buf, sizeof(buf))) > 0) {
        p->bytes_read += n;
        
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                line[line_len] = '\0';
                p->lines_read++;
                
                /* DOMAIN: Run through pipeline stages */
                if (process_line(p, line, &line_len)) {
                    line[line_len++] = '\n';
                    ssize_t w = p->writer_ops->write(p->writer_ctx, 
                                                      line, line_len);
                    if (w > 0) {
                        p->bytes_written += w;
                        p->lines_written++;
                    }
                }
                line_len = 0;
            } else if (line_len < MAX_LINE - 2) {
                line[line_len++] = buf[i];
            }
        }
    }
    
    p->writer_ops->flush(p->writer_ctx);
    p->writer_ops->close(p->writer_ctx);
    p->reader_ops->close(p->reader_ctx);
    
    return 0;
}

void pipeline_cleanup(struct pipeline *p)
{
    free(p->stages);
}
```

---

## Example 5: Plugin-Based Application

### Directory Layout

```
plugin_app/
├── include/
│   ├── plugin/                # DOMAIN defines plugin interface
│   │   └── plugin_api.h
│   ├── core/                  # DOMAIN logic
│   │   └── engine.h
│   └── ui/                    # PRESENTATION
│       └── menu.h
├── plugins/                   # DATA (loaded at runtime)
│   ├── plugin_hello.c
│   └── plugin_calc.c
├── src/
│   ├── core/
│   │   ├── engine.c
│   │   └── loader.c
│   └── ui/
│       └── menu.c
└── main.c
```

### Plugin API (Domain Interface)

```c
/* include/plugin/plugin_api.h - DOMAIN defines interface */
#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#define PLUGIN_API_VERSION 1

/* Plugin provides these operations */
struct plugin_ops {
    const char *name;
    const char *version;
    const char *description;
    
    int (*init)(void *ctx);
    void (*cleanup)(void *ctx);
    
    int (*execute)(void *ctx, const char *input, char *output, size_t size);
    const char *(*get_help)(void *ctx);
};

/* Plugin must export this symbol */
#define PLUGIN_EXPORT __attribute__((visibility("default")))

/* Macro for plugin to define its entry point */
#define DEFINE_PLUGIN(ops_struct) \
    PLUGIN_EXPORT const struct plugin_ops *plugin_get_ops(void) { \
        return &(ops_struct); \
    }

#endif
```

### Domain: Plugin Engine

```c
/* include/core/engine.h - DOMAIN */
#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H

#include "plugin/plugin_api.h"

#define MAX_PLUGINS 32

struct loaded_plugin {
    void *handle;                  /* dlopen handle */
    const struct plugin_ops *ops;
    void *ctx;                     /* Plugin context */
    int enabled;
};

struct plugin_engine {
    struct loaded_plugin plugins[MAX_PLUGINS];
    int plugin_count;
    
    /* Domain policies */
    int allow_duplicate_names;
    size_t max_output_size;
};

/* Domain API */
int engine_init(struct plugin_engine *e);
void engine_cleanup(struct plugin_engine *e);

int engine_load_plugin(struct plugin_engine *e, const char *path);
int engine_unload_plugin(struct plugin_engine *e, const char *name);

int engine_list_plugins(struct plugin_engine *e, 
                        const struct plugin_ops **out, int max);

int engine_execute(struct plugin_engine *e, const char *plugin_name,
                   const char *input, char *output, size_t size);

#endif
```

```c
/* src/core/engine.c - DOMAIN */
#include "core/engine.h"
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>

typedef const struct plugin_ops *(*plugin_entry_fn)(void);

int engine_init(struct plugin_engine *e)
{
    memset(e, 0, sizeof(*e));
    e->max_output_size = 4096;
    return 0;
}

void engine_cleanup(struct plugin_engine *e)
{
    for (int i = 0; i < e->plugin_count; i++) {
        if (e->plugins[i].ops && e->plugins[i].ops->cleanup)
            e->plugins[i].ops->cleanup(e->plugins[i].ctx);
        if (e->plugins[i].handle)
            dlclose(e->plugins[i].handle);
    }
}

int engine_load_plugin(struct plugin_engine *e, const char *path)
{
    if (e->plugin_count >= MAX_PLUGINS)
        return -1;
    
    /* Load shared library - DATA layer concern */
    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle)
        return -1;
    
    /* Get plugin entry point */
    plugin_entry_fn entry = dlsym(handle, "plugin_get_ops");
    if (!entry) {
        dlclose(handle);
        return -1;
    }
    
    const struct plugin_ops *ops = entry();
    if (!ops || !ops->name) {
        dlclose(handle);
        return -1;
    }
    
    /* DOMAIN: Check for duplicates if policy forbids */
    if (!e->allow_duplicate_names) {
        for (int i = 0; i < e->plugin_count; i++) {
            if (strcmp(e->plugins[i].ops->name, ops->name) == 0) {
                dlclose(handle);
                return -1;
            }
        }
    }
    
    /* Initialize plugin */
    struct loaded_plugin *p = &e->plugins[e->plugin_count];
    p->handle = handle;
    p->ops = ops;
    p->ctx = calloc(1, 1024);  /* Generic context */
    p->enabled = 1;
    
    if (ops->init && ops->init(p->ctx) < 0) {
        free(p->ctx);
        dlclose(handle);
        return -1;
    }
    
    e->plugin_count++;
    return 0;
}

int engine_execute(struct plugin_engine *e, const char *plugin_name,
                   const char *input, char *output, size_t size)
{
    /* DOMAIN: Find plugin */
    for (int i = 0; i < e->plugin_count; i++) {
        if (e->plugins[i].enabled &&
            strcmp(e->plugins[i].ops->name, plugin_name) == 0) {
            
            /* DOMAIN: Enforce output size limit */
            size_t max = (size < e->max_output_size) ? size : e->max_output_size;
            
            return e->plugins[i].ops->execute(e->plugins[i].ctx,
                                              input, output, max);
        }
    }
    return -1;  /* Plugin not found */
}
```

### Example Plugin (Data Layer)

```c
/* plugins/plugin_calc.c - DATA (plugin implementation) */
#include "plugin/plugin_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int calc_init(void *ctx)
{
    (void)ctx;
    return 0;
}

static void calc_cleanup(void *ctx)
{
    (void)ctx;
}

static int calc_execute(void *ctx, const char *input, 
                        char *output, size_t size)
{
    (void)ctx;
    
    /* Simple expression: "a op b" */
    double a, b;
    char op;
    
    if (sscanf(input, "%lf %c %lf", &a, &op, &b) != 3) {
        snprintf(output, size, "Error: Invalid expression");
        return -1;
    }
    
    double result;
    switch (op) {
    case '+': result = a + b; break;
    case '-': result = a - b; break;
    case '*': result = a * b; break;
    case '/': 
        if (b == 0) {
            snprintf(output, size, "Error: Division by zero");
            return -1;
        }
        result = a / b; 
        break;
    default:
        snprintf(output, size, "Error: Unknown operator");
        return -1;
    }
    
    snprintf(output, size, "%.6g", result);
    return 0;
}

static const char *calc_help(void *ctx)
{
    (void)ctx;
    return "Calculator: Enter 'a op b' where op is +, -, *, /";
}

static struct plugin_ops calc_ops = {
    .name = "calc",
    .version = "1.0",
    .description = "Simple calculator plugin",
    .init = calc_init,
    .cleanup = calc_cleanup,
    .execute = calc_execute,
    .get_help = calc_help,
};

DEFINE_PLUGIN(calc_ops)
```

---

## Summary: User-Space PDD Patterns

```
+------------------------------------------------------------------+
|  KEY PATTERNS FROM EXAMPLES                                      |
+------------------------------------------------------------------+

    1. OPS TABLES AS INTERFACES
       ┌─────────────────────────────────────────────────────────┐
       │  struct xxx_ops {                                        │
       │      int (*operation)(void *ctx, ...);                   │
       │  };                                                       │
       │  Domain defines, Data implements, Presentation uses     │
       └─────────────────────────────────────────────────────────┘

    2. CONTEXT OBJECTS
       ┌─────────────────────────────────────────────────────────┐
       │  void *ctx passed to all operations                     │
       │  Allows multiple instances, no global state            │
       └─────────────────────────────────────────────────────────┘

    3. HEADER ORGANIZATION
       ┌─────────────────────────────────────────────────────────┐
       │  include/domain/   - Interfaces, policies              │
       │  include/data/     - Backend implementations            │
       │  include/present/  - UI/protocol handlers              │
       └─────────────────────────────────────────────────────────┘

    4. COMPILE-TIME SEPARATION
       ┌─────────────────────────────────────────────────────────┐
       │  Domain compiles without Presentation headers          │
       │  Domain compiles without Data implementation headers   │
       │  Only needs interface headers                           │
       └─────────────────────────────────────────────────────────┘
```

**中文总结：**
- **示例1**：CLI 工具——可替换的文件/SQLite 存储后端
- **示例2**：嵌入式 HAL——传感器驱动 + 监控业务规则
- **示例3**：网络服务——协议解析与业务逻辑分离
- **示例4**：文件处理管道——可组合的过滤器和转换器
- **示例5**：插件系统——运行时加载的功能模块

