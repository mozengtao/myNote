/*
 * QEMU PCIe Device with Hardware DMA Ring and MSI-X Support (mypcie)
 *
 * Designed for PCIe Linux Driver Teaching Capstone Project
 * Vendor ID: 0x1234, Device ID: 0x22e8
 *
 * Registers in BAR0 (0x00 ~ 0x7F):
 *   0x00: [RO] MAGIC       (0x50434945 = "PCIE")
 *   0x04: [RW] CONTROL     (bit 0: ENABLE, bit 1: RING_START)
 *   0x08: [RO] STATUS      (bit 0: READY, bit 1: RING_ACTIVE, bit 2: BUSY)
 *   0x10: [RW] RING_BASE_LO (DMA Ring Physical Address Low 32-bit)
 *   0x14: [RW] RING_BASE_HI (DMA Ring Physical Address High 32-bit)
 *   0x18: [RW] RING_SIZE   (Number of descriptors, e.g. 16)
 *   0x20: [RW] PRODUCER    (Updated by driver via MMIO doorbell)
 *   0x24: [RO] CONSUMER    (Updated by hardware after descriptor completed)
 *   0x28: [WO] DOORBELL    (Driver writes 1 to kick hardware processing)
 *   0x30: [RW] IRQ_STATUS  (bit 0: Ring completion IRQ)
 *   0x34: [WO] IRQ_ACK     (Write 1 to clear)
 *
 * MSI-X:
 *   Vector 0: Ring Descriptor Complete Interrupt
 */

#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/msix.h"
#include "qemu/module.h"
#include "qom/object.h"

#define TYPE_PCI_MYPCIE_DEVICE "mypcie"
typedef struct MyPcieState MyPcieState;
DECLARE_INSTANCE_CHECKER(MyPcieState, MYPCIE, TYPE_PCI_MYPCIE_DEVICE)

#define MYPCIE_MAGIC            0x50434945 /* "PCIE" */
#define MYPCIE_BAR0_SIZE        (64 * 1024)

#define REG_MAGIC               0x00
#define REG_CONTROL             0x04
#define REG_STATUS              0x08
#define REG_RING_BASE_LO        0x10
#define REG_RING_BASE_HI        0x14
#define REG_RING_SIZE           0x18
#define REG_PRODUCER            0x20
#define REG_CONSUMER            0x24
#define REG_DOORBELL            0x28
#define REG_IRQ_STATUS          0x30
#define REG_IRQ_ACK             0x34

#define CTRL_ENABLE             (1U << 0)
#define CTRL_RING_START         (1U << 1)

#define STAT_READY              (1U << 0)
#define STAT_RING_ACTIVE        (1U << 1)
#define STAT_BUSY               (1U << 2)

#define IRQ_RING_COMPLETE       (1U << 0)

/*
 * Hardware Descriptor Layout (16 Bytes)
 */
struct QEMU_PACKED my_hw_desc {
    uint64_t dma_addr;  /* 64-bit DMA buffer host address */
    uint32_t length;    /* Transfer byte length */
    uint32_t flags;     /* bit 0: DIR (0: Write to RAM, 1: Read from RAM), bit 1: DONE */
};

struct MyPcieState {
    PCIDevice pdev;
    MemoryRegion mmio;

    uint32_t control;
    uint32_t status;
    uint64_t ring_base;
    uint32_t ring_size;
    uint32_t producer;
    uint32_t consumer;
    uint32_t irq_status;
};

/*
 * Raise MSI-X Vector 0 or fallback to INTx
 */
static void
mypcie_raise_irq(MyPcieState *s, uint32_t mask)
{
    s->irq_status |= mask;
    if (msix_enabled(&s->pdev)) {
        msix_notify(&s->pdev, 0);
    } else {
        pci_set_irq(&s->pdev, 1);
    }
}

/*
 * Process Descriptors in the DMA Ring
 */
static void
mypcie_process_ring(MyPcieState *s)
{
    if (!(s->control & CTRL_ENABLE) || !(s->control & CTRL_RING_START))
        return;
    if (!s->ring_base || !s->ring_size)
        return;

    s->status |= STAT_BUSY;

    while (s->consumer != s->producer) {
        uint32_t idx = s->consumer % s->ring_size;
        uint64_t desc_addr = s->ring_base + idx * sizeof(struct my_hw_desc);
        struct my_hw_desc desc;

        /* DMA read descriptor from Host RAM */
        pci_dma_read(&s->pdev, desc_addr, &desc, sizeof(desc));

        /* Execute payload DMA simulation */
        if (desc.length > 0 && desc.dma_addr != 0) {
            char scratch[256];
            uint32_t copy_len = desc.length > sizeof(scratch) ? sizeof(scratch) : desc.length;

            if (desc.flags & 0x1) {
                /* Direction 1: Device reads from RAM */
                pci_dma_read(&s->pdev, desc.dma_addr, scratch, copy_len);
            } else {
                /* Direction 0: Device writes test pattern to RAM */
                memset(scratch, 0x5a, copy_len);
                pci_dma_write(&s->pdev, desc.dma_addr, scratch, copy_len);
            }
        }

        /* Mark descriptor DONE (bit 1 = 1) */
        desc.flags |= 0x2;
        pci_dma_write(&s->pdev, desc_addr + offsetof(struct my_hw_desc, flags),
                      &desc.flags, sizeof(desc.flags));

        /* Advance hardware consumer index */
        s->consumer = (s->consumer + 1) % s->ring_size;
    }

    s->status &= ~STAT_BUSY;

    /* Fire completion interrupt */
    mypcie_raise_irq(s, IRQ_RING_COMPLETE);
}

static uint64_t
mypcie_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    MyPcieState *s = opaque;
    uint64_t val = 0;

    switch (addr) {
    case REG_MAGIC:
        val = MYPCIE_MAGIC;
        break;
    case REG_CONTROL:
        val = s->control;
        break;
    case REG_STATUS:
        val = s->status | STAT_READY;
        if (s->control & CTRL_RING_START)
            val |= STAT_RING_ACTIVE;
        break;
    case REG_RING_BASE_LO:
        val = (uint32_t)s->ring_base;
        break;
    case REG_RING_BASE_HI:
        val = (uint32_t)(s->ring_base >> 32);
        break;
    case REG_RING_SIZE:
        val = s->ring_size;
        break;
    case REG_PRODUCER:
        val = s->producer;
        break;
    case REG_CONSUMER:
        val = s->consumer;
        break;
    case REG_IRQ_STATUS:
        val = s->irq_status;
        break;
    default:
        break;
    }
    return val;
}

static void
mypcie_mmio_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    MyPcieState *s = opaque;

    switch (addr) {
    case REG_CONTROL:
        s->control = (uint32_t)val;
        break;
    case REG_RING_BASE_LO:
        s->ring_base = (s->ring_base & 0xFFFFFFFF00000000ULL) | (uint32_t)val;
        break;
    case REG_RING_BASE_HI:
        s->ring_base = (s->ring_base & 0x00000000FFFFFFFFULL) | ((uint64_t)(uint32_t)val << 32);
        break;
    case REG_RING_SIZE:
        s->ring_size = (uint32_t)val;
        break;
    case REG_PRODUCER:
        s->producer = (uint32_t)val;
        break;
    case REG_DOORBELL:
        /* Driver ring doorbell, trigger ring processing */
        mypcie_process_ring(s);
        break;
    case REG_IRQ_ACK:
        s->irq_status &= ~(uint32_t)val;
        if (!s->irq_status && !msix_enabled(&s->pdev)) {
            pci_set_irq(&s->pdev, 0);
        }
        break;
    default:
        break;
    }
}

static const MemoryRegionOps mypcie_mmio_ops = {
    .read = mypcie_mmio_read,
    .write = mypcie_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = { .min_access_size = 4, .max_access_size = 8 },
    .impl  = { .min_access_size = 4, .max_access_size = 8 },
};

static void
mypcie_realize(PCIDevice *pdev, Error **errp)
{
    MyPcieState *s = MYPCIE(pdev);

    pci_config_set_interrupt_pin(pdev->config, 1);

    /* Initialize BAR0: 64KB MMIO */
    memory_region_init_io(&s->mmio, OBJECT(s), &mypcie_mmio_ops, s,
                          "mypcie-bar0", MYPCIE_BAR0_SIZE);
    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);

    /*
     * Initialize MSI-X: 2 vectors (Vector 0: Ring Completion)
     * Placed in BAR0 at offset 0x1000, PBA at offset 0x2000
     */
    if (msix_init(pdev, 2, &s->mmio, 0, 0x1000, &s->mmio, 0, 0x2000, 0, errp)) {
        return;
    }

    s->status = STAT_READY;
}

static void
mypcie_uninit(PCIDevice *pdev)
{
    msix_uninit(pdev, &MYPCIE(pdev)->mmio, &MYPCIE(pdev)->mmio);
}

static void
mypcie_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(class);

    k->realize   = mypcie_realize;
    k->exit      = mypcie_uninit;
    k->vendor_id = 0x1234;
    k->device_id = 0x22e8;
    k->revision  = 0x01;
    k->class_id  = PCI_CLASS_OTHERS;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo mypcie_info = {
    .name          = TYPE_PCI_MYPCIE_DEVICE,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(MyPcieState),
    .class_init    = mypcie_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void
mypcie_register_types(void)
{
    type_register_static(&mypcie_info);
}
type_init(mypcie_register_types)
