#define DECLARE_PROCESSOR(name, validate_func, transform_func) \
    static ProcessorVTable name##_vtable = { \
        .validate = validate_func, \
        .transform = transform_func, \
        .cleanup = NULL \
    }; \
    Processor* create_##name##_processor(const char* filename) { \
        Processor* p = malloc(sizeof(Processor)); \
        p->context = malloc(sizeof(ProcessContext)); \
        p->context->filename = (char*)filename; \
        p->vtable = &name##_vtable; \
        p->load = default_load; \
        p->save = default_save; \
        return p; \
    }

// 使用
DECLARE_PROCESSOR(csv, csv_validate, csv_transform)
DECLARE_PROCESSOR(json, json_validate, json_transform)