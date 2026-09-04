/*
 * 08_driver.c - Device driver framework: PCI / USB / Platform
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct device;
struct device_driver;

struct bus_type {
	const char *name;
	int (*match)(struct device *dev, struct device_driver *drv);
	int (*probe)(struct device *dev);
};

struct device_driver {
	const char *name;
	struct bus_type *bus;
	int (*probe)(struct device *dev);
	int (*remove)(struct device *dev);
};

struct device {
	const char *name;
	struct bus_type *bus;
	struct device_driver *driver;
	void *platform_data;
};

/* ---- PCI ---- */

struct pci_device {
	struct device dev; /* embedded */
	unsigned short vendor;
	unsigned short device_id;
	unsigned int irq;
};

struct pci_driver {
	struct device_driver driver;
	const char *id_table;
};

static int pci_match(struct device *dev, struct device_driver *drv)
{
	struct pci_device *pdev = container_of(dev, struct pci_device, dev);
	struct pci_driver *pdrv = container_of(drv, struct pci_driver, driver);

	printf("[PCI bus] match %s vendor=0x%04x device=0x%04x vs %s\n",
	       dev->name, pdev->vendor, pdev->device_id, pdrv->id_table);
	return (pdev->vendor == 0x8086); /* demo: intel only */
}

static int pci_nic_probe(struct device *dev)
{
	struct pci_device *pdev = container_of(dev, struct pci_device, dev);

	printf("[PCI nic] probe %s irq=%u -> OK\n", dev->name, pdev->irq);
	return 0;
}

static int pci_nic_remove(struct device *dev)
{
	printf("[PCI nic] remove %s\n", dev->name);
	return 0;
}

static struct bus_type pci_bus = {
	.name  = "pci",
	.match = pci_match,
};

static struct pci_driver pci_nic_driver = {
	.driver = {
		.name   = "e1000",
		.probe  = pci_nic_probe,
		.remove = pci_nic_remove,
	},
	.id_table = "8086:*",
};

/* ---- USB ---- */

struct usb_device {
	struct device dev;
	unsigned short idVendor;
	unsigned short idProduct;
};

struct usb_driver {
	struct device_driver driver;
	const char *id_table;
};

static int usb_match(struct device *dev, struct device_driver *drv)
{
	struct usb_device *udev = container_of(dev, struct usb_device, dev);
	struct usb_driver *udrv = container_of(drv, struct usb_driver, driver);

	printf("[USB bus] match %s id=%04x:%04x vs %s\n",
	       dev->name, udev->idVendor, udev->idProduct, udrv->id_table);
	return 1;
}

static int usb_kbd_probe(struct device *dev)
{
	struct usb_device *udev = container_of(dev, struct usb_device, dev);

	printf("[USB kbd] probe %s %04x:%04x -> OK\n",
	       dev->name, udev->idVendor, udev->idProduct);
	return 0;
}

static int usb_kbd_remove(struct device *dev)
{
	printf("[USB kbd] remove %s\n", dev->name);
	return 0;
}

static struct bus_type usb_bus = {
	.name  = "usb",
	.match = usb_match,
};

static struct usb_driver usb_kbd_driver = {
	.driver = {
		.name   = "usbhid",
		.probe  = usb_kbd_probe,
		.remove = usb_kbd_remove,
	},
	.id_table = "hid:keyboard",
};

/* ---- Platform ---- */

struct platform_device {
	struct device dev;
	int irq;
	unsigned long mem_start;
};

static int plat_match(struct device *dev, struct device_driver *drv)
{
	printf("[Platform bus] match %s vs driver %s\n",
	       dev->name, drv->name);
	return strcmp(dev->name, drv->name) == 0;
}

static int plat_uart_probe(struct device *dev)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);

	printf("[Platform uart] probe %s irq=%d mem=0x%lx -> OK\n",
	       dev->name, pdev->irq, pdev->mem_start);
	return 0;
}

static int plat_uart_remove(struct device *dev)
{
	printf("[Platform uart] remove %s\n", dev->name);
	return 0;
}

static struct bus_type platform_bus = {
	.name  = "platform",
	.match = plat_match,
};

static struct device_driver plat_uart_driver = {
	.name   = "serial8250",
	.probe  = plat_uart_probe,
	.remove = plat_uart_remove,
};

/* Framework: bind device to driver */
static int driver_probe_device(struct device *dev, struct device_driver *drv)
{
	int ret;

	dev->bus = drv->bus;
	if (dev->bus && dev->bus->match) {
		if (!dev->bus->match(dev, drv)) {
			printf("  -> no match\n");
			return -1;
		}
	}
	dev->driver = drv;
	ret = drv->probe(dev);
	printf("  -> probe result=%d\n\n", ret);
	return ret;
}

int main(void)
{
	struct pci_device nic = {
		.dev = { .name = "pci0000:00:19.0" },
		.vendor = 0x8086,
		.device_id = 0x15a1,
		.irq = 16,
	};
	struct usb_device kbd = {
		.dev = { .name = "usb-1-2" },
		.idVendor = 0x046d,
		.idProduct = 0xc31c,
	};
	struct platform_device uart = {
		.dev = { .name = "serial8250" },
		.irq = 4,
		.mem_start = 0x3f8,
	};

	printf("=== 08 Device Driver Framework ===\n");

	pci_nic_driver.driver.bus = &pci_bus;
	usb_kbd_driver.driver.bus = &usb_bus;
	plat_uart_driver.bus = &platform_bus;

	driver_probe_device(&nic.dev, &pci_nic_driver.driver);
	driver_probe_device(&kbd.dev, &usb_kbd_driver.driver);
	driver_probe_device(&uart.dev, &plat_uart_driver);

	pci_nic_driver.driver.remove(&nic.dev);
	usb_kbd_driver.driver.remove(&kbd.dev);
	plat_uart_driver.remove(&uart.dev);

	printf("Done.\n");
	return 0;
}
