// 处理器注册表
typedef struct {
    const char* name;
    ProcessorVTable* vtable;
} ProcessorType;

ProcessorType processor_registry[] = {
    {"csv", &csv_vtable},
    {"json", &json_vtable},
    {NULL, NULL}
};

Processor* create_processor_by_name(const char* name, const char* filename) {
    for (int i = 0; processor_registry[i].name; i++) {
        if (strcmp(name, processor_registry[i].name) == 0) {
            Processor* p = malloc(sizeof(Processor));
            p->context = malloc(sizeof(ProcessContext));
            p->context->filename = (char*)filename;
            p->vtable = processor_registry[i].vtable;
            p->load = default_load;
            p->save = default_save;
            return p;
        }
    }
    return NULL;
}