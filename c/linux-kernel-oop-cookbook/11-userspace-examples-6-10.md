# 第五部分（B）：用户空间完整实例 6–10

> 承接上一章。继续用纯 C 演示 Kernel 同构的对象模型。
> 源码目录：[`examples/`](examples/)。输出均为真实 `./binary` 结果。

## 阅读提示

每读完一个例子，问自己：

1. 哪个 struct 是 Object？
2. 哪个 ops 是 Virtual Table？
3. 哪里用了 container_of？
4. 如果删掉 container_of，还能不能写这个设计？

若第 4 题的答案是「不能优雅地写」，你就真正理解了第三支柱。

---

## 示例6：数据库驱动：MySQL / SQLite / PostgreSQL

### 为什么这个例子重要

驱动接口统一，后端可替换——典型 Strategy / Driver 模式

### 三大支柱映射

- **struct**：对象状态（含嵌入的 base）
- **ops**：行为接口（框架只认 ops，不认具体类型）
- **container_of**：回调中恢复派生对象私有字段

### 完整代码（`examples/06_database.c`）

```c
/*
 * 06_database.c - Database driver interface: MySQL / SQLite / PostgreSQL
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct db_driver;

struct db_ops {
	int  (*open)(struct db_driver *self, const char *dsn);
	int  (*query)(struct db_driver *self, const char *sql);
	int  (*close)(struct db_driver *self);
};

struct db_driver {
	const char *name;
	int connected;
	const struct db_ops *ops;
};

static int db_open(struct db_driver *self, const char *dsn)
{
	return (self && self->ops && self->ops->open)
		? self->ops->open(self, dsn) : -1;
}

static int db_query(struct db_driver *self, const char *sql)
{
	return (self && self->ops && self->ops->query)
		? self->ops->query(self, sql) : -1;
}

static int db_close(struct db_driver *self)
{
	return (self && self->ops && self->ops->close)
		? self->ops->close(self) : -1;
}

/* MySQL */
struct mysql_driver {
	struct db_driver base;
	const char *host;
	int port;
};

static int mysql_open(struct db_driver *self, const char *dsn)
{
	struct mysql_driver *m = container_of(self, struct mysql_driver, base);

	m->host = "127.0.0.1";
	m->port = 3306;
	self->connected = 1;
	printf("[MySQL] open dsn=%s -> %s:%d\n", dsn, m->host, m->port);
	return 0;
}

static int mysql_query(struct db_driver *self, const char *sql)
{
	struct mysql_driver *m = container_of(self, struct mysql_driver, base);

	printf("[MySQL] query on %s:%d => \"%s\" -> 3 rows\n",
	       m->host, m->port, sql);
	return 3;
}

static int mysql_close(struct db_driver *self)
{
	printf("[MySQL] close\n");
	self->connected = 0;
	return 0;
}

static const struct db_ops mysql_ops = {
	.open  = mysql_open,
	.query = mysql_query,
	.close = mysql_close,
};

/* SQLite */
struct sqlite_driver {
	struct db_driver base;
	const char *path;
};

static int sqlite_open(struct db_driver *self, const char *dsn)
{
	struct sqlite_driver *s = container_of(self, struct sqlite_driver, base);

	s->path = dsn;
	self->connected = 1;
	printf("[SQLite] open file=%s\n", s->path);
	return 0;
}

static int sqlite_query(struct db_driver *self, const char *sql)
{
	struct sqlite_driver *s = container_of(self, struct sqlite_driver, base);

	printf("[SQLite] query on %s => \"%s\" -> 1 row\n", s->path, sql);
	return 1;
}

static int sqlite_close(struct db_driver *self)
{
	printf("[SQLite] close\n");
	self->connected = 0;
	return 0;
}

static const struct db_ops sqlite_ops = {
	.open  = sqlite_open,
	.query = sqlite_query,
	.close = sqlite_close,
};

/* PostgreSQL */
struct pgsql_driver {
	struct db_driver base;
	const char *db;
	int backend_pid;
};

static int pgsql_open(struct db_driver *self, const char *dsn)
{
	struct pgsql_driver *p = container_of(self, struct pgsql_driver, base);

	p->db = "appdb";
	p->backend_pid = 4242;
	self->connected = 1;
	printf("[PgSQL] open dsn=%s db=%s backend_pid=%d\n",
	       dsn, p->db, p->backend_pid);
	return 0;
}

static int pgsql_query(struct db_driver *self, const char *sql)
{
	struct pgsql_driver *p = container_of(self, struct pgsql_driver, base);

	printf("[PgSQL] query pid=%d => \"%s\" -> 5 rows\n",
	       p->backend_pid, sql);
	return 5;
}

static int pgsql_close(struct db_driver *self)
{
	printf("[PgSQL] close\n");
	self->connected = 0;
	return 0;
}

static const struct db_ops pgsql_ops = {
	.open  = pgsql_open,
	.query = pgsql_query,
	.close = pgsql_close,
};

static void use_db(struct db_driver *drv, const char *dsn, const char *sql)
{
	int rows;

	db_open(drv, dsn);
	rows = db_query(drv, sql);
	printf("  -> framework got %d rows from %s\n", rows, drv->name);
	db_close(drv);
	printf("\n");
}

int main(void)
{
	struct mysql_driver  mysql = {
		.base = { .name = "mysql",  .ops = &mysql_ops  },
	};
	struct sqlite_driver sqlite = {
		.base = { .name = "sqlite", .ops = &sqlite_ops },
	};
	struct pgsql_driver  pgsql = {
		.base = { .name = "pgsql",  .ops = &pgsql_ops  },
	};

	printf("=== 06 Database Driver ===\n");
	use_db(&mysql.base,  "mysql://local",   "SELECT * FROM users");
	use_db(&sqlite.base, "/tmp/app.db",     "SELECT count(*) FROM t");
	use_db(&pgsql.base,  "pgsql://local",   "SELECT id FROM orders");
	printf("Done.\n");
	return 0;
}
```

### 真实运行输出

```text
=== 06 Database Driver ===
[MySQL] open dsn=mysql://local -> 127.0.0.1:3306
[MySQL] query on 127.0.0.1:3306 => "SELECT * FROM users" -> 3 rows
  -> framework got 3 rows from mysql
[MySQL] close

[SQLite] open file=/tmp/app.db
[SQLite] query on /tmp/app.db => "SELECT count(*) FROM t" -> 1 row
  -> framework got 1 rows from sqlite
[SQLite] close

[PgSQL] open dsn=pgsql://local db=appdb backend_pid=4242
[PgSQL] query pid=4242 => "SELECT id FROM orders" -> 5 rows
  -> framework got 5 rows from pgsql
[PgSQL] close

Done.
```

### 若写成 C++ 会是什么样

```cpp
struct DbDriver {
  virtual int open(string dsn) = 0;
  virtual int query(string sql) = 0;
  virtual int close() = 0;
};
class MysqlDriver : public DbDriver { string host; int port; };
```

### 设计原则回扣

```
Framework code:
  obj->ops->method(obj, ...)

Derived implementation:
  derived = container_of(obj, typeof(*derived), base_member)
  use derived->private_fields
```

这与 Kernel 中 `file->f_op->read(file, ...)` 再在驱动里 `container_of` 回派生对象，是**同一套协议**。

### 本例总结

> 这里体现了 OO Design 的：**抽象（稳定接口）+ 多态（可替换实现）+ 继承（嵌入恢复）**。

---

## 示例7：文件系统：EXT4 / XFS / FAT（Mini VFS）

### 为什么这个例子重要

超级块嵌入 + s_op 分派；最接近真实 VFS 对象模型

### 三大支柱映射

- **struct**：对象状态（含嵌入的 base）
- **ops**：行为接口（框架只认 ops，不认具体类型）
- **container_of**：回调中恢复派生对象私有字段

### 完整代码（`examples/07_filesystem.c`）

```c
/*
 * 07_filesystem.c - Mini VFS: EXT4 / XFS / FAT via super_operations style
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct super_block;
struct inode;

struct super_ops {
	struct inode *(*alloc_inode)(struct super_block *sb);
	int  (*write_inode)(struct inode *inode);
	void (*put_super)(struct super_block *sb);
};

struct inode_ops {
	int (*lookup)(struct inode *dir, const char *name);
	int (*create)(struct inode *dir, const char *name);
};

struct inode {
	unsigned long ino;
	struct super_block *sb;
	const struct inode_ops *i_op;
};

struct super_block {
	const char *fstype;
	const char *dev;
	const struct super_ops *s_op;
	void *fs_info; /* private per-fs state */
};

/* ---- EXT4 ---- */

struct ext4_sb_info {
	struct super_block sb; /* embedded base */
	int block_size;
	unsigned long next_ino;
};

struct ext4_inode {
	struct inode vfs_inode;
	int blocks;
};

static struct inode *ext4_alloc_inode(struct super_block *sb)
{
	struct ext4_sb_info *sbi = container_of(sb, struct ext4_sb_info, sb);
	struct ext4_inode *ei = calloc(1, sizeof(*ei));

	if (!ei)
		return NULL;
	ei->vfs_inode.ino = sbi->next_ino++;
	ei->vfs_inode.sb  = sb;
	ei->blocks = 0;
	printf("[EXT4] alloc_inode ino=%lu block_size=%d\n",
	       ei->vfs_inode.ino, sbi->block_size);
	return &ei->vfs_inode;
}

static int ext4_write_inode(struct inode *inode)
{
	struct ext4_inode *ei = container_of(inode, struct ext4_inode, vfs_inode);

	printf("[EXT4] write_inode ino=%lu blocks=%d\n", inode->ino, ei->blocks);
	return 0;
}

static void ext4_put_super(struct super_block *sb)
{
	struct ext4_sb_info *sbi = container_of(sb, struct ext4_sb_info, sb);

	printf("[EXT4] put_super dev=%s\n", sb->dev);
	free(sbi);
}

static const struct super_ops ext4_sops = {
	.alloc_inode = ext4_alloc_inode,
	.write_inode = ext4_write_inode,
	.put_super   = ext4_put_super,
};

/* ---- XFS ---- */

struct xfs_sb_info {
	struct super_block sb;
	unsigned long agcount;
	unsigned long next_ino;
};

struct xfs_inode {
	struct inode vfs_inode;
	unsigned long agno;
};

static struct inode *xfs_alloc_inode(struct super_block *sb)
{
	struct xfs_sb_info *xsb = container_of(sb, struct xfs_sb_info, sb);
	struct xfs_inode *xi = calloc(1, sizeof(*xi));

	if (!xi)
		return NULL;
	xi->vfs_inode.ino = xsb->next_ino++;
	xi->vfs_inode.sb  = sb;
	xi->agno = xi->vfs_inode.ino % xsb->agcount;
	printf("[XFS] alloc_inode ino=%lu agno=%lu agcount=%lu\n",
	       xi->vfs_inode.ino, xi->agno, xsb->agcount);
	return &xi->vfs_inode;
}

static int xfs_write_inode(struct inode *inode)
{
	struct xfs_inode *xi = container_of(inode, struct xfs_inode, vfs_inode);

	printf("[XFS] write_inode ino=%lu agno=%lu\n", inode->ino, xi->agno);
	return 0;
}

static void xfs_put_super(struct super_block *sb)
{
	struct xfs_sb_info *xsb = container_of(sb, struct xfs_sb_info, sb);

	printf("[XFS] put_super dev=%s\n", sb->dev);
	free(xsb);
}

static const struct super_ops xfs_sops = {
	.alloc_inode = xfs_alloc_inode,
	.write_inode = xfs_write_inode,
	.put_super   = xfs_put_super,
};

/* ---- FAT ---- */

struct fat_sb_info {
	struct super_block sb;
	int fat_bits; /* 12/16/32 */
	unsigned long next_ino;
};

struct fat_inode {
	struct inode vfs_inode;
	unsigned int cluster;
};

static struct inode *fat_alloc_inode(struct super_block *sb)
{
	struct fat_sb_info *fsi = container_of(sb, struct fat_sb_info, sb);
	struct fat_inode *fi = calloc(1, sizeof(*fi));

	if (!fi)
		return NULL;
	fi->vfs_inode.ino = fsi->next_ino++;
	fi->vfs_inode.sb  = sb;
	fi->cluster = (unsigned int)fi->vfs_inode.ino + 2;
	printf("[FAT] alloc_inode ino=%lu cluster=%u fat_bits=%d\n",
	       fi->vfs_inode.ino, fi->cluster, fsi->fat_bits);
	return &fi->vfs_inode;
}

static int fat_write_inode(struct inode *inode)
{
	struct fat_inode *fi = container_of(inode, struct fat_inode, vfs_inode);

	printf("[FAT] write_inode ino=%lu cluster=%u\n",
	       inode->ino, fi->cluster);
	return 0;
}

static void fat_put_super(struct super_block *sb)
{
	struct fat_sb_info *fsi = container_of(sb, struct fat_sb_info, sb);

	printf("[FAT] put_super dev=%s\n", sb->dev);
	free(fsi);
}

static const struct super_ops fat_sops = {
	.alloc_inode = fat_alloc_inode,
	.write_inode = fat_write_inode,
	.put_super   = fat_put_super,
};

/* VFS framework helpers */
static void vfs_mount_demo(struct super_block *sb)
{
	struct inode *ino;

	printf("--- mount %s on %s ---\n", sb->fstype, sb->dev);
	ino = sb->s_op->alloc_inode(sb);
	if (ino) {
		sb->s_op->write_inode(ino);
		/* free derived inode via knowing layout - demo only */
		free(ino);
	}
}

int main(void)
{
	struct ext4_sb_info *ext4;
	struct xfs_sb_info *xfs;
	struct fat_sb_info *fat;

	printf("=== 07 Filesystem (Mini VFS) ===\n");

	ext4 = calloc(1, sizeof(*ext4));
	ext4->sb.fstype = "ext4";
	ext4->sb.dev = "/dev/sda1";
	ext4->sb.s_op = &ext4_sops;
	ext4->block_size = 4096;
	ext4->next_ino = 1;
	vfs_mount_demo(&ext4->sb);
	ext4->sb.s_op->put_super(&ext4->sb);

	xfs = calloc(1, sizeof(*xfs));
	xfs->sb.fstype = "xfs";
	xfs->sb.dev = "/dev/sdb1";
	xfs->sb.s_op = &xfs_sops;
	xfs->agcount = 4;
	xfs->next_ino = 1;
	vfs_mount_demo(&xfs->sb);
	xfs->sb.s_op->put_super(&xfs->sb);

	fat = calloc(1, sizeof(*fat));
	fat->sb.fstype = "vfat";
	fat->sb.dev = "/dev/sdc1";
	fat->sb.s_op = &fat_sops;
	fat->fat_bits = 32;
	fat->next_ino = 1;
	vfs_mount_demo(&fat->sb);
	fat->sb.s_op->put_super(&fat->sb);

	printf("Done.\n");
	return 0;
}
```

### 真实运行输出

```text
=== 07 Filesystem (Mini VFS) ===
--- mount ext4 on /dev/sda1 ---
[EXT4] alloc_inode ino=1 block_size=4096
[EXT4] write_inode ino=1 blocks=0
[EXT4] put_super dev=/dev/sda1
--- mount xfs on /dev/sdb1 ---
[XFS] alloc_inode ino=1 agno=1 agcount=4
[XFS] write_inode ino=1 agno=1
[XFS] put_super dev=/dev/sdb1
--- mount vfat on /dev/sdc1 ---
[FAT] alloc_inode ino=1 cluster=3 fat_bits=32
[FAT] write_inode ino=1 cluster=3
[FAT] put_super dev=/dev/sdc1
Done.
```

### 若写成 C++ 会是什么样

```cpp
struct SuperBlock {
  const SuperOps* s_op;
};
struct Ext4SbInfo {
  SuperBlock sb; // embedded
  int block_size;
};
// container_of(sb, Ext4SbInfo, sb)
```

### 设计原则回扣

```
Framework code:
  obj->ops->method(obj, ...)

Derived implementation:
  derived = container_of(obj, typeof(*derived), base_member)
  use derived->private_fields
```

这与 Kernel 中 `file->f_op->read(file, ...)` 再在驱动里 `container_of` 回派生对象，是**同一套协议**。

### 本例总结

> 这里体现了 OO Design 的：**抽象（稳定接口）+ 多态（可替换实现）+ 继承（嵌入恢复）**。

---

## 示例8：设备驱动框架：PCI / USB / Platform

### 为什么这个例子重要

bus.match + driver.probe：驱动模型的核心绑定协议

### 三大支柱映射

- **struct**：对象状态（含嵌入的 base）
- **ops**：行为接口（框架只认 ops，不认具体类型）
- **container_of**：回调中恢复派生对象私有字段

### 完整代码（`examples/08_driver.c`）

```c
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
```

### 真实运行输出

```text
=== 08 Device Driver Framework ===
[PCI bus] match pci0000:00:19.0 vendor=0x8086 device=0x15a1 vs 8086:*
[PCI nic] probe pci0000:00:19.0 irq=16 -> OK
  -> probe result=0

[USB bus] match usb-1-2 id=046d:c31c vs hid:keyboard
[USB kbd] probe usb-1-2 046d:c31c -> OK
  -> probe result=0

[Platform bus] match serial8250 vs driver serial8250
[Platform uart] probe serial8250 irq=4 mem=0x3f8 -> OK
  -> probe result=0

[PCI nic] remove pci0000:00:19.0
[USB kbd] remove usb-1-2
[Platform uart] remove serial8250
Done.
```

### 若写成 C++ 会是什么样

```cpp
struct Device { BusType* bus; Driver* driver; };
struct PciDevice : Device { u16 vendor, device_id; };
struct PciDriver : Driver { id_table; };
// bus->match(dev, drv); drv->probe(dev);
```

### 设计原则回扣

```
Framework code:
  obj->ops->method(obj, ...)

Derived implementation:
  derived = container_of(obj, typeof(*derived), base_member)
  use derived->private_fields
```

这与 Kernel 中 `file->f_op->read(file, ...)` 再在驱动里 `container_of` 回派生对象，是**同一套协议**。

### 本例总结

> 这里体现了 OO Design 的：**抽象（稳定接口）+ 多态（可替换实现）+ 继承（嵌入恢复）**。

---

## 示例9：消息队列：Producer / Consumer / Dispatcher

### 为什么这个例子重要

发布-订阅 + 消费者多态 on_message

### 三大支柱映射

- **struct**：对象状态（含嵌入的 base）
- **ops**：行为接口（框架只认 ops，不认具体类型）
- **container_of**：回调中恢复派生对象私有字段

### 完整代码（`examples/09_mq.c`）

```c
/*
 * 09_mq.c - Message queue: Producer / Consumer / Dispatcher
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

#define MQ_CAP 16

struct message {
	int id;
	char payload[64];
};

struct mq_node;

struct mq_ops {
	void (*on_message)(struct mq_node *self, const struct message *msg);
	void (*destroy)(struct mq_node *self);
};

struct mq_node {
	const char *name;
	const struct mq_ops *ops;
};

struct message_queue {
	struct message buf[MQ_CAP];
	int head, tail, count;
	struct mq_node *consumers[8];
	int n_consumers;
};

static void mq_init(struct message_queue *q)
{
	memset(q, 0, sizeof(*q));
}

static void mq_subscribe(struct message_queue *q, struct mq_node *node)
{
	if (q->n_consumers < 8) {
		q->consumers[q->n_consumers++] = node;
		printf("[MQ] subscribe consumer \"%s\"\n", node->name);
	}
}

static int mq_push(struct message_queue *q, int id, const char *payload)
{
	struct message *m;

	if (q->count >= MQ_CAP)
		return -1;
	m = &q->buf[q->tail];
	m->id = id;
	strncpy(m->payload, payload, sizeof(m->payload) - 1);
	m->payload[sizeof(m->payload) - 1] = '\0';
	q->tail = (q->tail + 1) % MQ_CAP;
	q->count++;
	printf("[Producer] enqueue id=%d payload=\"%s\"\n", id, payload);
	return 0;
}

static void mq_dispatch(struct message_queue *q)
{
	int i;

	printf("[Dispatcher] draining %d messages\n", q->count);
	while (q->count > 0) {
		struct message *m = &q->buf[q->head];

		for (i = 0; i < q->n_consumers; i++) {
			struct mq_node *n = q->consumers[i];

			if (n->ops && n->ops->on_message)
				n->ops->on_message(n, m);
		}
		q->head = (q->head + 1) % MQ_CAP;
		q->count--;
	}
}

/* Logger consumer */
struct logger_consumer {
	struct mq_node base;
	int seen;
};

static void logger_on_message(struct mq_node *self, const struct message *msg)
{
	struct logger_consumer *lc =
		container_of(self, struct logger_consumer, base);

	lc->seen++;
	printf("  [LoggerConsumer] #%d saw id=%d \"%s\"\n",
	       lc->seen, msg->id, msg->payload);
}

static const struct mq_ops logger_ops = {
	.on_message = logger_on_message,
};

/* Metrics consumer */
struct metrics_consumer {
	struct mq_node base;
	int bytes;
};

static void metrics_on_message(struct mq_node *self, const struct message *msg)
{
	struct metrics_consumer *mc =
		container_of(self, struct metrics_consumer, base);

	mc->bytes += (int)strlen(msg->payload);
	printf("  [MetricsConsumer] total_bytes=%d (msg id=%d)\n",
	       mc->bytes, msg->id);
}

static const struct mq_ops metrics_ops = {
	.on_message = metrics_on_message,
};

/* Alert consumer */
struct alert_consumer {
	struct mq_node base;
	int threshold;
	int alerts;
};

static void alert_on_message(struct mq_node *self, const struct message *msg)
{
	struct alert_consumer *ac =
		container_of(self, struct alert_consumer, base);

	if (msg->id >= ac->threshold) {
		ac->alerts++;
		printf("  [AlertConsumer] ALERT id=%d >= threshold=%d\n",
		       msg->id, ac->threshold);
	} else {
		printf("  [AlertConsumer] id=%d ok\n", msg->id);
	}
}

static const struct mq_ops alert_ops = {
	.on_message = alert_on_message,
};

int main(void)
{
	struct message_queue q;
	struct logger_consumer  logger = {
		.base = { .name = "logger",  .ops = &logger_ops  },
	};
	struct metrics_consumer metrics = {
		.base = { .name = "metrics", .ops = &metrics_ops },
	};
	struct alert_consumer   alert = {
		.base = { .name = "alert",   .ops = &alert_ops   },
		.threshold = 2,
	};

	printf("=== 09 Message Queue Framework ===\n");
	mq_init(&q);
	mq_subscribe(&q, &logger.base);
	mq_subscribe(&q, &metrics.base);
	mq_subscribe(&q, &alert.base);

	mq_push(&q, 1, "heartbeat");
	mq_push(&q, 2, "disk-full");
	mq_push(&q, 3, "oom-killer");

	mq_dispatch(&q);

	printf("Summary: logger.seen=%d metrics.bytes=%d alerts=%d\n",
	       logger.seen, metrics.bytes, alert.alerts);
	printf("Done.\n");
	return 0;
}
```

### 真实运行输出

```text
=== 09 Message Queue Framework ===
[MQ] subscribe consumer "logger"
[MQ] subscribe consumer "metrics"
[MQ] subscribe consumer "alert"
[Producer] enqueue id=1 payload="heartbeat"
[Producer] enqueue id=2 payload="disk-full"
[Producer] enqueue id=3 payload="oom-killer"
[Dispatcher] draining 3 messages
  [LoggerConsumer] #1 saw id=1 "heartbeat"
  [MetricsConsumer] total_bytes=9 (msg id=1)
  [AlertConsumer] id=1 ok
  [LoggerConsumer] #2 saw id=2 "disk-full"
  [MetricsConsumer] total_bytes=18 (msg id=2)
  [AlertConsumer] ALERT id=2 >= threshold=2
  [LoggerConsumer] #3 saw id=3 "oom-killer"
  [MetricsConsumer] total_bytes=28 (msg id=3)
  [AlertConsumer] ALERT id=3 >= threshold=2
Summary: logger.seen=3 metrics.bytes=28 alerts=2
Done.
```

### 若写成 C++ 会是什么样

```cpp
struct MqNode {
  virtual void on_message(const Message&) = 0;
};
class Dispatcher {
  void subscribe(MqNode*);
  void dispatch();
};
```

### 设计原则回扣

```
Framework code:
  obj->ops->method(obj, ...)

Derived implementation:
  derived = container_of(obj, typeof(*derived), base_member)
  use derived->private_fields
```

这与 Kernel 中 `file->f_op->read(file, ...)` 再在驱动里 `container_of` 回派生对象，是**同一套协议**。

### 本例总结

> 这里体现了 OO Design 的：**抽象（稳定接口）+ 多态（可替换实现）+ 继承（嵌入恢复）**。

---

## 示例10：游戏引擎：Entity / NPC / Monster / Player

### 为什么这个例子重要

实体组件式对象：update/render/on_hit 虚函数循环

### 三大支柱映射

- **struct**：对象状态（含嵌入的 base）
- **ops**：行为接口（框架只认 ops，不认具体类型）
- **container_of**：回调中恢复派生对象私有字段

### 完整代码（`examples/10_game_engine.c`）

```c
/*
 * 10_game_engine.c - Game engine entities: NPC / Monster / Player
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct entity;

struct entity_ops {
	void (*update)(struct entity *self, float dt);
	void (*render)(struct entity *self);
	void (*on_hit)(struct entity *self, int damage);
	void (*destroy)(struct entity *self);
};

struct entity {
	const char *name;
	float x, y;
	int hp;
	const struct entity_ops *ops;
};

static void entity_update(struct entity *self, float dt)
{
	if (self && self->ops && self->ops->update)
		self->ops->update(self, dt);
}

static void entity_render(struct entity *self)
{
	if (self && self->ops && self->ops->render)
		self->ops->render(self);
}

static void entity_hit(struct entity *self, int damage)
{
	if (self && self->ops && self->ops->on_hit)
		self->ops->on_hit(self, damage);
}

static void entity_destroy(struct entity *self)
{
	if (self && self->ops && self->ops->destroy)
		self->ops->destroy(self);
}

/* Player */
struct player {
	struct entity base;
	int score;
	int mana;
};

static void player_update(struct entity *self, float dt)
{
	struct player *p = container_of(self, struct player, base);

	self->x += 10.0f * dt;
	p->mana += 1;
	printf("[Player] %s update pos=(%.1f,%.1f) mana=%d score=%d\n",
	       self->name, self->x, self->y, p->mana, p->score);
}

static void player_render(struct entity *self)
{
	struct player *p = container_of(self, struct player, base);

	printf("[Player] RENDER %s hp=%d score=%d\n",
	       self->name, self->hp, p->score);
}

static void player_on_hit(struct entity *self, int damage)
{
	struct player *p = container_of(self, struct player, base);

	self->hp -= damage;
	printf("[Player] %s hit -%d hp=%d\n", self->name, damage, self->hp);
	if (self->hp <= 0)
		printf("[Player] %s DIED (score=%d)\n", self->name, p->score);
}

static void player_destroy(struct entity *self)
{
	free(container_of(self, struct player, base));
}

static const struct entity_ops player_ops = {
	.update  = player_update,
	.render  = player_render,
	.on_hit  = player_on_hit,
	.destroy = player_destroy,
};

static struct entity *player_create(const char *name, float x, float y)
{
	struct player *p = calloc(1, sizeof(*p));

	if (!p)
		return NULL;
	p->base.name = name;
	p->base.x = x;
	p->base.y = y;
	p->base.hp = 100;
	p->base.ops = &player_ops;
	p->score = 0;
	p->mana = 50;
	return &p->base;
}

/* NPC */
struct npc {
	struct entity base;
	const char *dialog;
};

static void npc_update(struct entity *self, float dt)
{
	(void)dt;
	printf("[NPC] %s idles at (%.1f,%.1f)\n",
	       self->name, self->x, self->y);
}

static void npc_render(struct entity *self)
{
	struct npc *n = container_of(self, struct npc, base);

	printf("[NPC] RENDER %s says \"%s\"\n", self->name, n->dialog);
}

static void npc_on_hit(struct entity *self, int damage)
{
	(void)damage;
	printf("[NPC] %s is friendly - hit ignored\n", self->name);
}

static void npc_destroy(struct entity *self)
{
	free(container_of(self, struct npc, base));
}

static const struct entity_ops npc_ops = {
	.update  = npc_update,
	.render  = npc_render,
	.on_hit  = npc_on_hit,
	.destroy = npc_destroy,
};

static struct entity *npc_create(const char *name, float x, float y,
				 const char *dialog)
{
	struct npc *n = calloc(1, sizeof(*n));

	if (!n)
		return NULL;
	n->base.name = name;
	n->base.x = x;
	n->base.y = y;
	n->base.hp = 1;
	n->base.ops = &npc_ops;
	n->dialog = dialog;
	return &n->base;
}

/* Monster */
struct monster {
	struct entity base;
	int attack_power;
	float aggro_range;
};

static void monster_update(struct entity *self, float dt)
{
	struct monster *m = container_of(self, struct monster, base);

	self->x -= 5.0f * dt;
	printf("[Monster] %s hunts pos=(%.1f,%.1f) atk=%d aggro=%.1f\n",
	       self->name, self->x, self->y, m->attack_power, m->aggro_range);
}

static void monster_render(struct entity *self)
{
	struct monster *m = container_of(self, struct monster, base);

	printf("[Monster] RENDER %s hp=%d atk=%d\n",
	       self->name, self->hp, m->attack_power);
}

static void monster_on_hit(struct entity *self, int damage)
{
	self->hp -= damage;
	printf("[Monster] %s hit -%d hp=%d\n", self->name, damage, self->hp);
	if (self->hp <= 0)
		printf("[Monster] %s SLAIN\n", self->name);
}

static void monster_destroy(struct entity *self)
{
	free(container_of(self, struct monster, base));
}

static const struct entity_ops monster_ops = {
	.update  = monster_update,
	.render  = monster_render,
	.on_hit  = monster_on_hit,
	.destroy = monster_destroy,
};

static struct entity *monster_create(const char *name, float x, float y,
				     int atk, float aggro)
{
	struct monster *m = calloc(1, sizeof(*m));

	if (!m)
		return NULL;
	m->base.name = name;
	m->base.x = x;
	m->base.y = y;
	m->base.hp = 40;
	m->base.ops = &monster_ops;
	m->attack_power = atk;
	m->aggro_range = aggro;
	return &m->base;
}

/* Engine tick */
static void world_tick(struct entity **ents, int n, float dt)
{
	int i;

	printf("--- tick dt=%.2f ---\n", dt);
	for (i = 0; i < n; i++)
		entity_update(ents[i], dt);
	for (i = 0; i < n; i++)
		entity_render(ents[i]);
}

int main(void)
{
	struct entity *world[3];
	int i;

	printf("=== 10 Game Engine ===\n");
	world[0] = player_create("Hero", 0.0f, 0.0f);
	world[1] = npc_create("Villager", 5.0f, 0.0f, "Welcome, traveler!");
	world[2] = monster_create("Goblin", 20.0f, 0.0f, 8, 12.0f);

	world_tick(world, 3, 1.0f);

	printf("--- combat ---\n");
	entity_hit(world[2], 15);
	entity_hit(world[0], 5);
	entity_hit(world[1], 1);
	entity_hit(world[2], 30);

	printf("--- cleanup ---\n");
	for (i = 0; i < 3; i++)
		entity_destroy(world[i]);
	printf("Done.\n");
	return 0;
}
```

### 真实运行输出

```text
=== 10 Game Engine ===
--- tick dt=1.00 ---
[Player] Hero update pos=(10.0,0.0) mana=51 score=0
[NPC] Villager idles at (5.0,0.0)
[Monster] Goblin hunts pos=(15.0,0.0) atk=8 aggro=12.0
[Player] RENDER Hero hp=100 score=0
[NPC] RENDER Villager says "Welcome, traveler!"
[Monster] RENDER Goblin hp=40 atk=8
--- combat ---
[Monster] Goblin hit -15 hp=25
[Player] Hero hit -5 hp=95
[NPC] Villager is friendly - hit ignored
[Monster] Goblin hit -30 hp=-5
[Monster] Goblin SLAIN
--- cleanup ---
Done.
```

### 若写成 C++ 会是什么样

```cpp
struct Entity {
  virtual void update(float dt) = 0;
  virtual void render() = 0;
  virtual void on_hit(int dmg) = 0;
};
class Player : public Entity { int score, mana; };
```

### 设计原则回扣

```
Framework code:
  obj->ops->method(obj, ...)

Derived implementation:
  derived = container_of(obj, typeof(*derived), base_member)
  use derived->private_fields
```

这与 Kernel 中 `file->f_op->read(file, ...)` 再在驱动里 `container_of` 回派生对象，是**同一套协议**。

### 本例总结

> 这里体现了 OO Design 的：**抽象（稳定接口）+ 多态（可替换实现）+ 继承（嵌入恢复）**。

---

## 第五部分总收束

十个例子覆盖了不同领域，但对象协议完全相同：

```
+-------------+     +-------------+     +----------------+
| struct      |-----+ ops table   |-----+ container_of   |
| (object)    |     | (methods)   |     | (inheritance)  |
+-------------+     +-------------+     +----------------+
        \                |                 /
         \               |                /
          +--------------+---------------+
                         |
                         v
              Portable Object Model in C
```

请立刻做一次验证：

```bash
cd examples && make clean && make run
```

如果你改了某个派生对象的私有字段，却不必改框架代码——恭喜，你已经在用 Kernel 同款 OOD 了。

下一章将彻底拆开 `container_of` 的每一个字节。
