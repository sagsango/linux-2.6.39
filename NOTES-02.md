───────────────────────────────────────────────────────────────
          Linux Kernel 2.6.39  —  Major Subsystems
───────────────────────────────────────────────────────────────
┌─────────────────────────────────────────────────────────────┐
│  User Space Interface Layer                                 │
│   • System-call table  • VDSO  • Netlink  • procfs/sysfs    │
├─────────────────────────────────────────────────────────────┤
│  Core Kernel / Process Management                           │
│   • Scheduler (O(1) + CFS refinements)                      │
│   • Preemption model (CONFIG_PREEMPT)                       │
│   • Signals, timers, workqueues, softirqs, tasklets         │
│   • cgroups (control groups)                                │
├─────────────────────────────────────────────────────────────┤
│  Memory Management (mm/)                                    │
│   • Buddy allocator, slab/slub/slob allocators              │
│   • Anonymous/file-backed VM, swap, NUMA support            │
│   • Transparent Huge Pages (THP)  ← NEW in 2.6.38/39        │
│   • Memory compaction + migration                           │
│   • Page cache, writeback, LRU                              │
├─────────────────────────────────────────────────────────────┤
│  Filesystems                                                │
│   • ext2/ext3/ext4 (ext4 stable, journaling JBD2)           │
│   • Btrfs (experimental)                                    │
│   • XFS, JFS, ReiserFS                                      │
│   • procfs, sysfs, debugfs                                  │
│   • 9p (Plan 9 FS, includes virtio transport)               │
├─────────────────────────────────────────────────────────────┤
│  Block & I/O Layer                                          │
│   • Generic block layer, elevator schedulers (CFQ, Deadline)│
│   • Bio structures, plug/unplug, barriers                   │
│   • SSD-aware discard (TRIM)                                │
│   • MD/DM multipath, LVM                                    │
│   • SCSI stack, ATA/libata, NVMe early support              │
│   • Block-plug I/O (blk-mq groundwork begins)               │
├─────────────────────────────────────────────────────────────┤
│  Device Drivers                                             │
│   • Net: e1000/e1000e, tg3, r8169, wireless mac80211 stack  │
│   • GPU/DRM: i915, radeon, nouveau (open NVIDIA)            │
│   • Sound: ALSA, HD-Audio                                   │
│   • USB: xHCI (USB 3.0), EHCI, OHCI, UHCI                   │
│   • Input: evdev, synaptics, HID                            │
│   • Storage: SCSI, USB-mass, FireWire                       │
│   • Virt: virtio-blk/net/rng/balloon/console                │
│   • Crypto: AES-NI, TPM driver framework                    │
├─────────────────────────────────────────────────────────────┤
│  Networking Stack                                           │
│   • IPv4/IPv6, TCP congestion control (CUBIC default)       │
│   • Netfilter/iptables, conntrack, NAT                      │
│   • Bridge, bonding, VLAN, tun/tap, mac80211 wireless       │
│   • QoS/traffic control (tc)                                │
│   • NET_NAMESPACE (per-net-ns isolation)                    │
├─────────────────────────────────────────────────────────────┤
│  Virtualization & Isolation                                 │
│   • KVM (Kernel-based Virtual Machine)                      │
│   • Virtio subsystem (PCI transport + devices)              │
│   • Vhost-net/vhost-blk kernel backends                     │
│   • IOMMU: Intel VT-d, AMD-Vi, ARM SMMU                     │
│   • cgroups + namespaces (PID, UTS, NET, MNT, IPC)          │
│   • Early work on LXC-style containers                      │
├─────────────────────────────────────────────────────────────┤
│  Security & Credentials                                     │
│   • SELinux, AppArmor, Smack                                │
│   • POSIX capabilities, keyrings                            │
│   • audit subsystem, integrity (IMA/EVM)                    │
├─────────────────────────────────────────────────────────────┤
│  Power Management & CPU Features                            │
│   • cpuidle, cpufreq, ACPI, suspend/resume                  │
│   • Tickless kernel (dynticks)                              │
│   • Multiprocessor (SMP, NUMA, RCU lockless)                │
│   • Hugepage/THP, memory hotplug                            │
│   • Early support for Intel Sandy Bridge, AMD Bulldozer     │
├─────────────────────────────────────────────────────────────┤
│  Debugging & Instrumentation                                │
│   • ftrace, perf events (perf tool)                         │
│   • kgdb, kprobes, tracepoints                              │
│   • lockdep, rcutorture                                     │
└─────────────────────────────────────────────────────────────┘

