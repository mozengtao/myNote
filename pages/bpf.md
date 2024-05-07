- [eBPF Documentation](https://ebpf.io/what-is-ebpf/)
- [BPF and XDP Reference Guide](https://docs.cilium.io/en/latest/bpf/)

- [bcc](https://github.com/iovisor/bcc)

- [eBPF Tutorial by Example](https://eunomia.dev/tutorials/)
- [eBPF 开发者教程](https://github.com/eunomia-bpf/bpf-developer-tutorial/blob/main/README.zh.md)
- [TIL: eBPF is awesome](https://filipnikolovski.com/posts/ebpf/)

- [Linux kernel profiling with perf](https://perf.wiki.kernel.org/index.php/Tutorial)

- [The art of writing eBPF programs: a primer](https://sysdig.com/blog/the-art-of-writing-ebpf-programs-a-primer/)

- [0x00C - eBPF](https://unzip.dev/0x00c-ebpf/)
    > How does it work? 
    > 
    > With eBPF you can interact with the Kernel from User space in a way that keeps security and stability without patching the Kernel.
    > 
    > An abstraction over eBPF could be bpftrace, but if you want to create an eBPF program from scratch you’d need to:
    > 
    > 1. Write your eBPF program (C/Rust while user-space code can be written via high-level language bindings like python) including a kernel hook (eBPF programs are event-driven) using helpers.
    > 2. Compile your eBPF program into bytecode the Kernel can run.
    > 3. Then your eBPF is loaded and verified (important step, as eBPF exposes a path for unprivileged users to execute in ring0).
    > 4. Attaching the program to its hook - eBPF programs execute when they get an event.
    > 5. Interact back with user-space programs via eBPF maps - the main way to communicate back with user-space.
    > 
    > Originally, BPF (no “e”, sometimes called cBPF) was used for networking-related tasks (used for tcpdump). eBPF extended this functionality by extending out from the networking subsystem allowing you to attach programs to a tracepoint or a kprobe, uprobes, and more… - which opened the door to many other use cases other than networking.
    > 
    > The traditional way of achieving many of the eBPF use cases was using an LKM (a kernel module). The drawbacks of an LKM are plenty: Kernel releases often break your module. New Kernel versions mean you need to rewrite your LKM. Lastly, there is a good chance you’ll crash the kernel - there are no safeties in place. The only other option is to get code into the Linux kernel directly, which could take a few years and might not be approved.

- [Linux Socket Filtering aka Berkeley Packet Filter (BPF)](https://www.kernel.org/doc/Documentation/networking/filter.txt)
- [perf Examples](https://www.brendangregg.com/perf.html)
- [FlameGraph](https://github.com/brendangregg/FlameGraph)
- [The Flame Graph](https://queue.acm.org/detail.cfm?id=2927301)