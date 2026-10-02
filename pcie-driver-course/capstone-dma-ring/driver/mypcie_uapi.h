/*
 * mypcie_uapi.h - User-Kernel Interface for mypcie driver
 */

#ifndef _UAPI_MYPCIE_H_
#define _UAPI_MYPCIE_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define MYPCIE_IOC_MAGIC 'p'

struct mypcie_ioc_transfer {
	__u32 length;           /* 传输字节数 */
	__u32 direction;        /* 0: Device写内存, 1: 读内存 */
	__u8  data[256];        /* 数据载荷 */
};

#define MYPCIE_IOCTL_SUBMIT_REQ _IOWR(MYPCIE_IOC_MAGIC, 1, struct mypcie_ioc_transfer)

#endif /* _UAPI_MYPCIE_H_ */
