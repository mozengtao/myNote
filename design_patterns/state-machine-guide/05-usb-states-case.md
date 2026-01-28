# Case 3: USB Device States

The USB device state machine implements the USB specification's required state management.

---

## Subsystem Context

```
+=============================================================================+
|                    USB DEVICE STATE MACHINE                                  |
+=============================================================================+

    USB devices must follow a specific state machine
    defined by the USB specification.

    States reflect:
    - Physical connection status
    - Power status
    - Configuration status
    - Suspend status


    USB STATE DIAGRAM:
    ==================

    +-------------+
    | NOTATTACHED |  <-- Device physically disconnected
    +------+------+
           | attach (plug in)
           v
    +------+------+
    |  ATTACHED   |  <-- Connected, not powered
    +------+------+
           | power applied
           v
    +------+------+
    |   POWERED   |  <-- Powered, waiting for reset
    +------+------+
           | bus reset
           v
    +------+------+
    |   DEFAULT   |  <-- Reset, address 0
    +------+------+
           | SET_ADDRESS
           v
    +------+------+
    |   ADDRESS   |  <-- Has unique address
    +------+------+
           | SET_CONFIGURATION
           v
    +------+------+
    |  CONFIGURED |  <-- Fully operational
    +------+------+
           | suspend
           v
    +------+------+
    |  SUSPENDED  |  <-- Low power mode
    +-------------+
```

**中文说明：**

USB设备状态机：USB设备必须遵循USB规范定义的状态机。状态反映物理连接状态、电源状态、配置状态、挂起状态。从NOTATTACHED（物理断开）到CONFIGURED（完全可用）需要经过多个中间状态。

---

## State Definitions

```c
/* include/linux/usb/ch9.h */
enum usb_device_state {
    /* Not yet connected to USB */
    USB_STATE_NOTATTACHED = 0,

    /* Cable connected but not powered */
    USB_STATE_ATTACHED,

    /* Powered but not reset */
    USB_STATE_POWERED,

    /* Default state after reset */
    USB_STATE_DEFAULT,

    /* After SET_ADDRESS */
    USB_STATE_ADDRESS,

    /* After SET_CONFIGURATION */
    USB_STATE_CONFIGURED,

    /* Suspended (any of the above) */
    USB_STATE_SUSPENDED,
};
```

---

## Key Functions

```c
/* drivers/usb/core/usb.c */
void usb_set_device_state(struct usb_device *udev,
                          enum usb_device_state new_state)
{
    unsigned long flags;
    
    spin_lock_irqsave(&device_state_lock, flags);
    
    if (udev->state == USB_STATE_NOTATTACHED)
        ; /* Cannot change from NOTATTACHED */
    else if (new_state != USB_STATE_NOTATTACHED) {
        /* Validate transition */
        udev->state = new_state;
    } else {
        /* Going to NOTATTACHED always allowed */
        udev->state = USB_STATE_NOTATTACHED;
    }
    
    spin_unlock_irqrestore(&device_state_lock, flags);
}

/* Check device state before operations */
int usb_submit_urb(struct urb *urb, gfp_t mem_flags)
{
    struct usb_device *dev = urb->dev;
    
    /* Can't submit if not attached */
    if (dev->state == USB_STATE_NOTATTACHED)
        return -ENODEV;
    
    /* Can't submit if suspended */
    if (dev->state == USB_STATE_SUSPENDED)
        return -EHOSTUNREACH;
    
    /* Must be configured for most endpoints */
    if (dev->state < USB_STATE_CONFIGURED)
        return -EHOSTUNREACH;
    
    /* ... submit URB ... */
}
```

**中文说明：**

关键函数：usb_set_device_state()改变设备状态，有验证逻辑（如不能从NOTATTACHED变化，但可以变为NOTATTACHED）。usb_submit_urb()在执行操作前检查状态，确保设备处于正确状态。

---

## Minimal C Simulation

```c
/* Simplified USB state machine */

#include <stdio.h>

enum usb_device_state {
    USB_STATE_NOTATTACHED = 0,
    USB_STATE_ATTACHED,
    USB_STATE_POWERED,
    USB_STATE_DEFAULT,
    USB_STATE_ADDRESS,
    USB_STATE_CONFIGURED,
    USB_STATE_SUSPENDED,
};

const char *usb_state_name(enum usb_device_state s)
{
    static const char *names[] = {
        "NOTATTACHED", "ATTACHED", "POWERED",
        "DEFAULT", "ADDRESS", "CONFIGURED", "SUSPENDED"
    };
    return names[s];
}

struct usb_device {
    enum usb_device_state state;
    int address;
    int config;
};

/* State transition with validation */
int usb_set_device_state(struct usb_device *dev,
                         enum usb_device_state new_state)
{
    printf("  [USB] State: %s -> %s\n",
           usb_state_name(dev->state),
           usb_state_name(new_state));
    
    /* Cannot change from NOTATTACHED except by reattach */
    if (dev->state == USB_STATE_NOTATTACHED &&
        new_state != USB_STATE_ATTACHED) {
        printf("  [USB] ERROR: Cannot change from NOTATTACHED\n");
        return -1;
    }
    
    dev->state = new_state;
    return 0;
}

/* Simulated USB operations */
void usb_attach(struct usb_device *dev)
{
    printf("[EVENT] Device plugged in\n");
    usb_set_device_state(dev, USB_STATE_ATTACHED);
}

void usb_power_on(struct usb_device *dev)
{
    printf("[EVENT] Port powered\n");
    usb_set_device_state(dev, USB_STATE_POWERED);
}

void usb_reset(struct usb_device *dev)
{
    printf("[EVENT] Bus reset\n");
    dev->address = 0;
    usb_set_device_state(dev, USB_STATE_DEFAULT);
}

void usb_set_address(struct usb_device *dev, int addr)
{
    printf("[EVENT] SET_ADDRESS %d\n", addr);
    dev->address = addr;
    usb_set_device_state(dev, USB_STATE_ADDRESS);
}

void usb_set_configuration(struct usb_device *dev, int config)
{
    printf("[EVENT] SET_CONFIGURATION %d\n", config);
    dev->config = config;
    usb_set_device_state(dev, USB_STATE_CONFIGURED);
}

void usb_disconnect(struct usb_device *dev)
{
    printf("[EVENT] Device unplugged\n");
    usb_set_device_state(dev, USB_STATE_NOTATTACHED);
}

/* Operation that checks state */
int usb_submit_urb(struct usb_device *dev)
{
    printf("[URB] Attempting to submit URB...\n");
    
    if (dev->state == USB_STATE_NOTATTACHED) {
        printf("  [URB] ERROR: Device not attached\n");
        return -1;
    }
    if (dev->state < USB_STATE_CONFIGURED) {
        printf("  [URB] ERROR: Device not configured\n");
        return -1;
    }
    
    printf("  [URB] Success: URB submitted\n");
    return 0;
}

int main(void)
{
    struct usb_device dev = {
        .state = USB_STATE_NOTATTACHED,
        .address = 0,
        .config = 0
    };
    
    printf("=== USB STATE MACHINE SIMULATION ===\n\n");
    
    /* Try URB before configured */
    usb_submit_urb(&dev);
    
    /* Normal enumeration sequence */
    printf("\n--- USB Enumeration ---\n");
    usb_attach(&dev);
    usb_power_on(&dev);
    usb_reset(&dev);
    usb_set_address(&dev, 5);
    usb_set_configuration(&dev, 1);
    
    /* Now URB should work */
    printf("\n--- After configuration ---\n");
    usb_submit_urb(&dev);
    
    /* Disconnect */
    printf("\n--- Disconnect ---\n");
    usb_disconnect(&dev);
    usb_submit_urb(&dev);
    
    return 0;
}
```

---

## What Core Does NOT Control

```
    Core Controls:
    --------------
    [X] State definitions
    [X] Valid transitions
    [X] State checks before operations

    Core Does NOT Control:
    ----------------------
    [ ] When device is plugged/unplugged
    [ ] Hardware enumeration timing
    [ ] Device-specific behavior
```

---

## Version

Based on **Linux kernel v3.2** and USB specification.

Key files:
- `include/linux/usb/ch9.h` - State definitions
- `drivers/usb/core/usb.c` - usb_set_device_state()
- `drivers/usb/core/urb.c` - usb_submit_urb()
