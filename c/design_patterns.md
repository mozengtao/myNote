## Singleton pattern
```c
#include <stdio.h>
#include <pthread.h>

typedef struct Printer {
    int port;
    int pageCount;
} Printer;

/* simple implementation */
Printer *getPrinter() {
    static Printer printer;
    static int initialized = 0;

    if (!initialized) {
        printer.port = 0x123;
        printer.pageCount = 0;
        initialized = 1;
    }

    return &printer;
}
```
### Option 1: pthread_once (Recommended for POSIX systems)
```c
/*
 * Option 1: pthread_once (Recommended for POSIX systems)
 * - Guaranteed to run initialization exactly once
 * - No explicit locking needed by caller
 * - Most robust and efficient solution
 */
static Printer g_printer;
static pthread_once_t g_printer_once = PTHREAD_ONCE_INIT;

static void initPrinter(void) {
    g_printer.port = 0x123;
    g_printer.pageCount = 0;
}

Printer *getPrinter(void) {
    pthread_once(&g_printer_once, initPrinter);
    return &g_printer;
}
```

### Option 2: Double-Checked Locking with Mutex
```c
/*
 * Option 2: Double-Checked Locking with Mutex
 * - Use when pthread_once is not available
 * - Requires careful memory ordering
 */
static Printer g_printer;
static volatile int g_initialized = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
 *
Printer *getPrinter(void) {
    if (!g_initialized) {                    // First check (no lock)
        pthread_mutex_lock(&g_mutex);
        if (!g_initialized) {                // Second check (with lock)
            g_printer.port = 0x123;
            g_printer.pageCount = 0;
            __sync_synchronize();            // Memory barrier
            g_initialized = 1;
        }
        pthread_mutex_unlock(&g_mutex);
    }
    return &g_printer;
}
```

### Option 3: C11 call_once (if C11 available)
```c
/*
 * Option 3: C11 call_once (if C11 available)
 */
#include <threads.h>
static Printer g_printer;
static once_flag g_flag = ONCE_FLAG_INIT;

static void initPrinter(void) {
    g_printer.port = 0x123;
    g_printer.pageCount = 0;
}

Printer *getPrinter(void) {
    call_once(&g_flag, initPrinter);
    return &g_printer;
}
```

### main function
```c
int main(void) {
    Printer *printer1 = getPrinter();
    Printer *printer2 = getPrinter();

    printf("Printer1: %p, Printer2: %p\n", printer1, printer2);

    if (printer1 == printer2) {
        printf("Printer1 and Printer2 are the same\n");
    } else {
        printf("Printer1 and Printer2 are different\n");
    }

    return 0;
}
```

## Factory pattern
```c
/* Factory pattern */
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    VEHICLE_TYPE_CAR,
    VEHICLE_TYPE_TRUCK,
} VehicleType;

const char *getVehicleTypeString(VehicleType type) {
    switch (type) {
        case VEHICLE_TYPE_CAR:
            return "Car";
        case VEHICLE_TYPE_TRUCK:
            return "Truck";
    }
    return "Unknown";
}

typedef struct Vehicle {
    int speed;
    int gear;
    VehicleType type;

    void (*accelerate)(struct Vehicle *self);
    void (*brake)(struct Vehicle *self);
    void (*print)(struct Vehicle *self);
} Vehicle;

void car_accelerate(Vehicle *self) {
    printf("Car accelerating\n");
}

void car_brake(Vehicle *self) {
    printf("Car braking\n");
}

void car_print(Vehicle *self) {
    printf("Car: speed: %d, gear: %d, type: %s\n", self->speed, self->gear, getVehicleTypeString(self->type));
}

void truck_accelerate(Vehicle *self) {
    printf("Truck accelerating\n");
}

void truck_brake(Vehicle *self) {
    printf("Truck braking\n");
}

void truck_print(Vehicle *self) {
    printf("Truck: speed: %d, gear: %d, type: %s\n", self->speed, self->gear, getVehicleTypeString(self->type));
}


Vehicle *createVehicle(int speed, int gear, VehicleType type) {
    Vehicle *vehicle = (Vehicle *)malloc(sizeof(Vehicle));
    if (vehicle == NULL) {
        printf("Failed to create vehicle\n");
        return NULL;
    }

    switch (type) {
        case VEHICLE_TYPE_CAR:
            vehicle->accelerate = car_accelerate;
            vehicle->brake = car_brake;
            vehicle->print = car_print;
            break;
        case VEHICLE_TYPE_TRUCK:
            vehicle->accelerate = truck_accelerate;
            vehicle->brake = truck_brake;
            vehicle->print = truck_print;
            break;
        default:
            printf("Invalid vehicle type\n");
            free(vehicle);
            return NULL;
    }

    vehicle->speed = speed;
    vehicle->gear = gear;
    vehicle->type = type;

    return vehicle;
}

int main(void) {
    Vehicle *car = createVehicle(100, 1, VEHICLE_TYPE_CAR);
    if (car == NULL) {
        printf("Failed to create car\n");
        return 1;
    }
    car->print(car);
    car->accelerate(car);
    car->brake(car);

    Vehicle *truck = createVehicle(100, 1, VEHICLE_TYPE_TRUCK);
    if (truck == NULL) {
        printf("Failed to create truck\n");
        return 1;
    }
    truck->print(truck);
    truck->accelerate(truck);
    truck->brake(truck);

    free(car);
    free(truck);

    return 0;
}
```