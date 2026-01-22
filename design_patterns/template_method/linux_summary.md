- Linux内核中模板方法模式的设计精髓
    通过定义操作模板和回调机制，将通用处理流程与具体硬件实现分离，实现了高度的可扩展性和可维护性

- 核心设计思想
    - Linux内核中模板方法模式的特点：
    1. 结构体函数指针：定义操作模板
    2. 分离变化与不变：
        不变：核心框架流程
        变化：具体硬件操作
    3. 运行时多态：通过函数指针动态调用
    4. 开闭原则：新驱动只需实现接口，无需修改框架

- 总结对比表
案例     模板结构体         模板方法                     具体实现举例             内核路径
文件系统 file_operations    vfs_read/write             ext4,procfs,tmpfs        fs/*
网络设备 net_device_ops     dev_queue_xmit             e1000,r8169,virtio_net   drivers/net/
块设备   block_device_ops   submit_bio                 SCSI,NVMe,USB存储        drivers/block/
平台驱动 platform_driver    platform_driver_register   GPIO,I2C,SPI控制器       drivers/base/
定时器   timer_list        __run_timers                高精度定时器, 工作队列    kernel/time/