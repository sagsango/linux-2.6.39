# linux-2.6.39 ❱❱❱ find * | grep -i virtio
4553:arch/s390/include/asm/kvm_virtio.h

23287:drivers/net/virtio_net.c
23408:drivers/char/hw_random/virtio-rng.c
23447:drivers/char/virtio_console.c

23563:drivers/virtio
23564:drivers/virtio/virtio_pci.c
23565:drivers/virtio/virtio.c
23566:drivers/virtio/Makefile
23567:drivers/virtio/Kconfig
23568:drivers/virtio/virtio_ring.c
23569:drivers/virtio/config.c
23570:drivers/virtio/virtio_balloon.c

25225:drivers/s390/kvm/kvm_virtio.c
27684:drivers/block/virtio_blk.c

31935:include/linux/virtio_console.h
31958:include/linux/virtio_ring.h
31967:include/linux/virtio_config.h
32034:include/linux/virtio_9p.h
32061:include/linux/virtio_ids.h
32119:include/linux/virtio_rng.h
32271:include/linux/virtio_blk.h
32410:include/linux/virtio_balloon.h
32568:include/linux/virtio_net.h
32631:include/linux/virtio.h
32727:include/linux/virtio_pci.h

34274:net/9p/trans_virtio.c

37353:tools/virtio
37354:tools/virtio/Makefile
37355:tools/virtio/linux
37356:tools/virtio/linux/device.h
37357:tools/virtio/linux/virtio.h
37358:tools/virtio/linux/slab.h
37359:tools/virtio/vhost_test
37360:tools/virtio/vhost_test/Makefile
37361:tools/virtio/vhost_test/vhost_test.c
37362:tools/virtio/virtio_test.c
linux-2.6.39 ❱❱❱





# Virtio in Linux is cleanly layered 
+--------------------------------------------------+
| Device drivers (blk, net, rng, console, balloon) |
+--------------------------------------------------+
| Transport layer (virtio-pci, virtio-mmio, ccw)   |
+--------------------------------------------------+
| Core virtio framework                            |
|   - virtio.c                                     |
|   - virtio_ring.c                                |
|   - config.c                                     |
+--------------------------------------------------+
| Generic kernel subsystems (DMA, IRQ, PCI, MM)    |
+--------------------------------------------------+




# How to start (short)
Phase 1 – Core (mandatory)
    virtio_ring.h
    virtio_ring.c
    virtio.c
    virtio.h
Phase 2 – Transport
    virtio_pci.c
Phase 3 – Small device
    virtio-rng.c
Phase 4 – Heavy device
    virtio_blk.c





# how to start (compelete)
(1) Core
include/linux/virtio.h
    Defines struct virtio_device
    Defines struct virtqueue
    Defines device/driver contract
    This is the “bus” abstraction, like PCI or USB.
include/linux/virtio_ring.h
    struct vring_desc
    struct vring_avail
    struct vring_used
    Memory layout rules
    Barriers
    90% of virtio understanding = vring
drivers/virtio/virtio.c
    Device registration
    Driver matching
    Feature negotiation
    Probe/remove flow
    its like pci.c for virtio
drivers/virtio/virtio_ring.c
    vring_add_buf()
    virtqueue_add_sgs()
    virtqueue_kick()
    virtqueue_get_buf()
    descriptor chains are built
    avail index is updated
    used index is consumed
    memory ordering
    ownership
    batching
    performance
drivers/virtio/config.c
    Device config space access
    Feature bits
    Endianness handling

(2) Transport layer: how the guest talks to QEMU
drivers/char/hw_random/virtio-rng.c
drivers/virtio/virtio_pci.c
drivers/block/virtio_blk.c
drivers/net/virtio_net.c
drivers/virtio/virtio_balloon.c
drivers/char/virtio_console.c

(3) A complete virtio request (guest-side flow)
(A) Driver init
    virtio_rng_probe()
        → find virtqueue
        → register hwrng
(B) Request submission
    virtqueue_add_buf()
        → fill vring_desc[]
        → add index to avail ring
(C) Notify host
    virtqueue_kick()
        → virtio_pci_notify()
        → PCI MMIO write
[----+ goes to QEMU +-----]
(D) Host processing
    QEMU virtio device:
        Reads avail ring from guest RAM
        Walks descriptor chain
        Fills buffer
        Updates used ring
        Injects interrupt via KVM
(E) Completion
    IRQ handler
        → virtqueue_get_buf()
        → callback
