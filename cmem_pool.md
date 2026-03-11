## memory pool
```c
/* memory pool management */

#include <stdio.h>
#include <stdlib.h>

typedef struct Object {
    int is_free;
    char data[100];
} Object;

typedef struct ObjectPool {
    Object *objects;
    int size;
    int free_count;
} ObjectPool;

ObjectPool *create_object_pool(int size) {
    ObjectPool *pool = (ObjectPool *)malloc(sizeof(ObjectPool));
    if (pool == NULL) {
        printf("Failed to create object pool\n");
        return NULL;
    }

    pool->objects = (Object *)malloc(size * sizeof(Object));
    if (pool->objects == NULL) {
        printf("Failed to create objects\n");
        free(pool);
        return NULL;
    }

    pool->size = size;
    pool->free_count = size;

    for (int i = 0; i < size; i++) {
        pool->objects[i].is_free = 1;
    }

    return pool;
}

void destroy_object_pool(ObjectPool *pool) {
    if (pool == NULL) {
        printf("Failed to destroy object pool\n");
        return;
    }

    free(pool->objects);
    free(pool);
}

Object *allocate_object(ObjectPool *pool) {
    if (pool == NULL || pool->free_count == 0) {
        return NULL;
    }

    for (int i = 0; i < pool->size; i++) {
        if (pool->objects[i].is_free) {
            pool->objects[i].is_free = 0;
            pool->free_count--;
            return &pool->objects[i];
        }
    }

    return NULL;
}

void free_object(Object *obj, ObjectPool *pool) {
    if (obj == NULL || pool == NULL) {
        printf("Failed to free object\n");
        return;
    }

    if(obj >= pool->objects && obj < pool->objects + pool->size) {
        obj->is_free = 1;
        pool->free_count++;
    } else {
        printf("Object is not in the pool\n");
    }
}

int main(void) {
    ObjectPool *pool = create_object_pool(100);
    if (pool == NULL) {
        printf("Failed to create object pool\n");
        return 1;
    }

    Object *obj1 = allocate_object(pool);
    if (obj1 == NULL) {
        printf("Failed to allocate object\n");
        return 1;
    }
    printf("Object1 allocated: %p, pool free count: %d\n", obj1, pool->free_count);

    Object *obj2 = allocate_object(pool);
    if (obj2 == NULL) {
        printf("Failed to allocate object\n");
        return 1;
    }
    printf("Object2 allocated: %p, pool free count: %d\n", obj2, pool->free_count);

    free_object(obj1, pool);
    printf("Object1 freed: %p, pool free count: %d\n", obj1, pool->free_count);

    free_object(obj2, pool);
    printf("Object2 freed: %p, pool free count: %d\n", obj2, pool->free_count);

    destroy_object_pool(pool);
    return 0;
}
```