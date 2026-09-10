/*
 * pci_fixup_table_sim.c
 *
 * 场景：模拟 `DECLARE_PCI_FIXUP_*` 系列宏——把"针对特定硬件 vendor/device id
 * 的修补函数"连同其匹配条件一起放进专用链接器 section，设备探测时按
 * vendor/device id 匹配执行，而不是像 06 篇 initcall 那样无条件全部执行。
 *
 * 真实定义：
 *   - struct pci_fixup                 见 include/linux/pci.h:1376
 *   - enum pci_fixup_pass（early/header/final/enable/resume/suspend...）
 *     见 include/linux/pci.h:1380
 *   - DECLARE_PCI_FIXUP_SECTION()/_EARLY()/_HEADER()/_FINAL() 宏
 *     见 include/linux/pci.h:1391-1401
 *   - pci_do_fixups()（真正按 vendor/device 匹配并调用 hook）
 *     见 drivers/pci/quirks.c:2909
 *   - pci_fixup_device()（按 pass 选择对应的 section 边界符号）
 *     见 drivers/pci/quirks.c:2938
 *   - 真实的 __start_pci_fixups_early[]/__end_pci_fixups_early[] 等边界符号
 *     见 drivers/pci/quirks.c:2922-2935（本仓库版本用 extern 声明 + 自定义
 *     链接脚本片段定义，见 include/asm-generic/vmlinux.lds.h:236-238）
 *
 * 与 __setup（本目录另一个示例 boot_param_setup_sim.c）的差异：
 *   - __setup 按"字符串名字"精确/前缀匹配。
 *   - PCI fixup 按"两个整数字段（vendor/device）"匹配，且支持 PCI_ANY_ID
 *     通配符——这是同一个"链接器 section 自注册 + 按条件匹配执行"元模式，
 *     换了一种匹配维度（数值范围/通配 vs 字符串前缀）的另一个真实变种。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o pci_fixup_table_sim pci_fixup_table_sim.c
 * 运行：./pci_fixup_table_sim
 */

#include <stdio.h>
#include <string.h>

#define PCI_ANY_ID	((unsigned short)~0)

/* 一个被探测到的 PCI 设备，类似简化版 struct pci_dev */
struct pci_dev {
	unsigned short vendor;
	unsigned short device;
	const char    *slot_name;
};

/* 对应真实 include/linux/pci.h:1376 struct pci_fixup */
struct pci_fixup {
	unsigned short vendor, device;	/* PCI_ANY_ID 表示通配 */
	void (*hook)(struct pci_dev *dev);
} __attribute__((aligned(sizeof(long))));	/* 强制对齐，避免 section 内出现 padding
						 * 缝隙（见 boot_param_setup_sim.c 里
						 * 复现过的同一类"手工元数据布局"陷阱） */

/* 对应真实 include/linux/pci.h:1380 enum pci_fixup_pass（本例只演示 3 个阶段） */
enum pci_fixup_pass {
	pci_fixup_early,
	pci_fixup_header,
	pci_fixup_final,
};

/* 对应真实 include/linux/pci.h:1391 DECLARE_PCI_FIXUP_SECTION()：
 * section 名字必须是合法 C 标识符才能让 GNU ld 自动生成
 * __start_<section>/__stop_<section> 边界符号（真实内核版本用带点的
 * section 名 + 自定义链接脚本片段来定义边界，这里用不带点的名字换成
 * ld 自动生成，效果等价，见本文件头部注释）。
 *
 * 注意：这里必须用 __section__（两侧双下划线）而不是 section——真实
 * include/linux/pci.h:1392 就是这么写的，原因是这个宏自己的形参名字
 * 恰好也叫 section，如果属性名也写成 section(...)，预处理器的形参替换
 * 会把 __attribute__((section(...))) 里的 "section" 这个词也当成形参一起换掉，
 * 变成 __attribute__((pci_fixup_early_sec(...)))这种语法错误——这里第一次
 * 尝试用 section(...) 时就是被这个真实存在的命名冲突坑了一次，改成
 * __section__ 之后才编译通过，保留在这里作为"宏形参命名与关键字/属性名
 * 冲突"陷阱的真实记录。 */
#define DECLARE_PCI_FIXUP_SECTION(section, name, vendor, device, hook)	\
	static const struct pci_fixup __pci_fixup_##name __attribute__((used))\
		__attribute__((__section__(#section)))				\
		= { vendor, device, hook }

#define DECLARE_PCI_FIXUP_EARLY(vendor, device, hook)				\
	DECLARE_PCI_FIXUP_SECTION(pci_fixup_early_sec, vendor##device##hook, vendor, device, hook)
#define DECLARE_PCI_FIXUP_FINAL(vendor, device, hook)				\
	DECLARE_PCI_FIXUP_SECTION(pci_fixup_final_sec, vendor##device##hook, vendor, device, hook)

extern struct pci_fixup __start_pci_fixup_early_sec[], __stop_pci_fixup_early_sec[];
extern struct pci_fixup __start_pci_fixup_final_sec[], __stop_pci_fixup_final_sec[];

/* ------------------------------------------------------------------ */
/* 模拟几个真实存在意义上的 PCI 厂商修补函数（vendor id 采用真实值，       */
/* device id 为演示用的虚构值）                                         */
/* ------------------------------------------------------------------ */

#define PCI_VENDOR_ID_INTEL	0x8086	/* 真实存在，见 include/linux/pci_ids.h */
#define PCI_VENDOR_ID_BROADCOM	0x14e4	/* 真实存在，见 include/linux/pci_ids.h */
#define PCI_VENDOR_ID_REALTEK	0x10ec	/* 真实存在，见 include/linux/pci_ids.h */

/* early 阶段：探测 BAR 之前，通常用于修正配置空间读出的错误值 */
static void quirk_intel_disable_msi(struct pci_dev *dev)
{
	printf("    [early ] quirk_intel_disable_msi: disabling MSI quirk for %s (%04x:%04x)\n",
	       dev->slot_name, dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, 0x1234, quirk_intel_disable_msi);

/* early 阶段：厂商通配（PCI_ANY_ID），命中该厂商任意设备 */
static void quirk_broadcom_any_device(struct pci_dev *dev)
{
	printf("    [early ] quirk_broadcom_any_device: vendor-wide workaround for %s (%04x:%04x)\n",
	       dev->slot_name, dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_BROADCOM, PCI_ANY_ID, quirk_broadcom_any_device);

/* final 阶段：设备已经完成基本枚举，通常用于设置驱动无关的兼容性开关 */
static void quirk_realtek_final(struct pci_dev *dev)
{
	printf("    [final ] quirk_realtek_final: applying final-stage fixup for %s (%04x:%04x)\n",
	       dev->slot_name, dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_REALTEK, 0x8168, quirk_realtek_final);

static void quirk_intel_final(struct pci_dev *dev)
{
	printf("    [final ] quirk_intel_final: applying final-stage fixup for %s (%04x:%04x)\n",
	       dev->slot_name, dev->vendor, dev->device);
}
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_INTEL, 0x1234, quirk_intel_final);

/* ------------------------------------------------------------------ */
/* Core：对应真实 drivers/pci/quirks.c pci_do_fixups()/pci_fixup_device() */
/* ------------------------------------------------------------------ */

static void pci_do_fixups(struct pci_dev *dev, struct pci_fixup *f, struct pci_fixup *end)
{
	while (f < end) {
		if ((f->vendor == dev->vendor || f->vendor == PCI_ANY_ID) &&
		    (f->device == dev->device || f->device == PCI_ANY_ID))
			f->hook(dev);		/* <-- 盲调，Core 不知道具体是哪个厂商的修补逻辑 */
		f++;
	}
}

static void pci_fixup_device(enum pci_fixup_pass pass, struct pci_dev *dev)
{
	switch (pass) {
	case pci_fixup_early:
		pci_do_fixups(dev, __start_pci_fixup_early_sec, __stop_pci_fixup_early_sec);
		break;
	case pci_fixup_header:
		break;	/* 本例未演示这一阶段 */
	case pci_fixup_final:
		pci_do_fixups(dev, __start_pci_fixup_final_sec, __stop_pci_fixup_final_sec);
		break;
	}
}

/* 对应真实 PCI 枚举核心路径（pci_scan_device() 简化版） */
static void pci_probe_device(struct pci_dev *dev)
{
	printf("-- 探测设备 %s (vendor=%04x device=%04x) --\n",
	       dev->slot_name, dev->vendor, dev->device);
	printf("  [pci core] pci_fixup_device(pci_fixup_early, ...)\n");
	pci_fixup_device(pci_fixup_early, dev);
	printf("  [pci core] (探测 BAR、读取配置空间...)\n");
	printf("  [pci core] pci_fixup_device(pci_fixup_final, ...)\n");
	pci_fixup_device(pci_fixup_final, dev);
}

int main(void)
{
	struct pci_dev devices[4] = {
		{ PCI_VENDOR_ID_INTEL,    0x1234, "0000:00:1f.0" },	/* 精确匹配 early+final 各一条 */
		{ PCI_VENDOR_ID_BROADCOM, 0x4321, "0000:03:00.0" },	/* 只匹配 early 的厂商通配规则 */
		{ PCI_VENDOR_ID_REALTEK,  0x8168, "0000:04:00.0" },	/* 只匹配 final 的精确规则 */
		{ 0x1af4,                 0x1000, "0000:05:00.0" },	/* virtio 设备，任何规则都不匹配 */
	};
	int i;

	printf("=== pci_fixup_table_sim demo ===\n\n");

	printf("已注册的 early-phase fixup 数量: %ld\n",
	       (long)(__stop_pci_fixup_early_sec - __start_pci_fixup_early_sec));
	printf("已注册的 final-phase fixup 数量: %ld\n\n",
	       (long)(__stop_pci_fixup_final_sec - __start_pci_fixup_final_sec));

	for (i = 0; i < 4; i++)
		pci_probe_device(&devices[i]);

	printf("\n=== done ===\n");
	printf("说明：pci_probe_device()/pci_fixup_device()（Core）从未 #include 任何\n"
	       "      具体厂商的头文件，只是按 (vendor,device) 数值在两个 section 里线性\n"
	       "      查找匹配项再盲调；virtio 设备（1af4:1000）没有任何规则匹配它，\n"
	       "      pci_do_fixups() 遍历完整个 section 什么也没做，这是完全正常、\n"
	       "      预期内的\"没有候选命中\"情况，不是错误。\n");
	return 0;
}
