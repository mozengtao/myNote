## 组合示例
```c
#include <stdio.h>

typedef struct Engine Engine;
struct Engine {
    char name[32];
    void (*start)(Engine *self);
    void (*stop)(Engine *self);
};

typedef struct Battery Battery;
struct Battery {
    int capacity;
    void (*charge)(Battery *self);
    void (*discharge)(Battery *self);
};


typedef struct Autopilot Autopilot;
struct Autopilot {
    int version;
    void (*engage)(Autopilot *self);
    void (*disengage)(Autopilot *self);
};


typedef struct {
    char model[32];
    Engine *engine;
    Battery *battery;
    Autopilot *autopilot;
} ElectricCar;

void startEngine(Engine *engine) {
    printf("Starting engine %s\n", engine->name);
}

void stopEngine(Engine *engine) {
    printf("Stopping engine %s\n", engine->name);
}

void chargeBattery(Battery *battery) {
    printf("Charging battery %d\n", battery->capacity);
}

void dischargeBattery(Battery *battery) {
    printf("Discharging battery %d\n", battery->capacity);
}

void engageAutopilot(Autopilot *autopilot) {
    printf("Engaging autopilot %d\n", autopilot->version);
}

void disengageAutopilot(Autopilot *autopilot) {
    printf("Disengaging autopilot %d\n", autopilot->version);
}

int main() {
    Engine engine = {"Engine 1", startEngine, stopEngine};
    Battery battery = {100, chargeBattery, dischargeBattery};
    Autopilot autopilot = {1, engageAutopilot, disengageAutopilot};
    ElectricCar car = {"Model 1", &engine, &battery, &autopilot};
    car.engine->start(car.engine);
    car.battery->charge(car.battery);
    car.autopilot->engage(car.autopilot);
    car.engine->stop(car.engine);
    car.battery->discharge(car.battery);
    car.autopilot->disengage(car.autopilot);
    return 0;
}
```