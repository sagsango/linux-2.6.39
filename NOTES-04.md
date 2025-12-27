# virtio_blk registration
MODULE LOAD
  |
  v
register_virtio_driver(virtio_blk)
  |
PCI SCAN
  |
virtio_pci_probe()
  |
create virtio_device
  |
register_virtio_device()
  |
virtio_bus.match()
  |
virtblk_probe()
  |
create virtqueue + gendisk
  |
add_disk()
  |
/dev/vda appears




# data-flow
Userspace
└── /dev/vda
    |
Block layer
└── request_queue
    |
virtio_blk driver
└── struct virtio_blk
    |
virtqueue "requests"
└── vring (desc/avail/used)
    |
KVM (memory mapping)
    |
QEMU virtio-blk backend
    |
Host storage




# Full registration flow 
1. Guest kernel boot
│
├─ PCI core initialized
│    ├─ pci_bus_type registered
│    ├─ pci_driver model active
│
├─ virtio core initialized
│    ├─ virtio_bus registered
│    ├─ virtio_dev_match installed
│
├─ virtio-pci driver already loaded (built-in or module)
│
└─ QEMU has already exposed PCI devices
     └─ virtio-blk-pci device present in PCI config space


2. insmod virtio_block.ko
virtio_bus
├── drivers:
│   └── virtio_blk_driver
└── devices:
    └── (maybe empty, maybe already present)


3. PCI scan
PCI scan happens in two cases:
    - At boot
    - When a PCI driver is loaded later

What heppen in PCI scan
    - PCI core walks PCI config space
    bus 0
    ├─ device 0
    ├─ device 1
    ├─ device 2
    │    └─ function 0
    │         ├─ vendor ID
    │         ├─ device ID
    │         ├─ BARs
    o         └─ capabilities

    - PCI core creates struct pci_dev
    - PCI driver matching happens
        pci_bus_type.match()
        └── compares pdev against pci_driver.id_table
        For virtio:
            virtio_pci_id_table matches vendor 0x1AF4
        So:
        PCI SCAN
            |
            v
        virtio_pci_probe(pdev)

    - What heppen in virtio_pci_probe(): 
    virtio_pci_probe(pdev)
    │
    ├─ pci_enable_device(pdev)
    │
    ├─ pci_request_regions(pdev)
    │
    ├─ pci_iomap()
    │    └─ map BAR into kernel virtual address
    │
    ├─ allocate struct virtio_pci_device
    │    └─ embeds struct virtio_device (vdev)
    │
    ├─ read virtio device ID from PCI config
    │    └─ vdev.id.device = VIRTIO_ID_BLOCK
    │
    ├─ vdev.config = virtio_pci_config_ops
    │
    ├─ setup interrupts
    │    ├─ INTx or
    │    ├─ MSI or
    │    └─ MSI-X
    │
    └─ register_virtio_device(&vdev)

    - At this point
        We still have no virtqueues
        We still have no block device
        We just created a generic virtio device
    - register_virtio_device() — ENTER VIRTIO CORE
        register_virtio_device()
        │
        └── device_register(&vdev->dev)
        └── device added to virtio_bus
    - DRIVER BINDING
        virtio_bus.match()
        └── virtio_dev_match()
            virtio_device.id.device == virtio_driver.id_table[i].device
            In our case: VIRTIO_ID_BLOCK == VIRTIO_ID_BLOCK
                so call: virtblk_probe(vdev)
    - virtblk_probe(vdev):
        vdev exists
        transport ops exist (vdev->config)
        NO virtqueues
        NO disk

        - Read host capabilities
            virtio_config_val(...)
            Calls:
                vdev->config->get()
                └── virtio_pci_config_ops.get()
                └── reads PCI BAR
                └── QEMU backend responds
        - Allocate driver-private state
            vdev->priv = vblk
            virtio_device
            └── priv → struct virtio_blk
        - Create virtqueue (THIS IS BIG)
            virtio_find_single_vq(vdev, blk_done, "requests")
            Which expands to:
                virtio_find_single_vq()
                └── vdev->config->find_vqs()
                    └── virtio_pci_find_vqs()
                    ├─ allocate vring memory
                    ├─ setup desc/avail/used
                    ├─ setup interrupt vector
                    └─ create struct virtqueue
            This is wherei we create:
            desc[]
            avail[]
            used[]

        - add_disk() — USERSpace VISIBILITY
            This is the final step.
            add_disk()
            │
            ├─ registers gendisk
            ├─ creates sysfs nodes
            │   └─ /sys/block/vda
            └─ sends uevent
                └─ udev creates /dev/vda


# full flow
insmod virtio_blk.ko
 |
 | register_blkdev()
 | register_virtio_driver()
 v
virtio_bus (driver added)
 |
 | PCI core already scanned bus
 v
PCI core matches virtio-pci
 |
 v
virtio_pci_probe()
 |
 | map BARs
 | setup IRQs
 | create virtio_device
 v
register_virtio_device()
 |
 v
virtio_bus.match()
 |
 v
virtblk_probe()
 |
 | read host config
 | create virtqueue (vring)
 | create gendisk
 v
add_disk()
 |
 v
udev
 |
 v
/dev/vda appears

