LINUX 2.6.39 DMA PROVIDER DRIVER REGISTRATION + IRQ INITIALIZATION NOTES
=======================================================================

Goal
====
Understand where a DMA controller driver gets registered, how it becomes
visible to the DMAEngine framework, and how IRQs are requested and used
throughout initialization.

IMPORTANT DISTINCTION
=====================
There are two different kinds of "DMA drivers" people often mix together:

1. DMA PROVIDER / DMA CONTROLLER DRIVER
   - This is the hardware DMA engine driver.
   - It usually lives under drivers/dma/.
   - It owns the DMA controller registers, IRQ, channels, descriptors.
   - It registers itself with the DMAEngine framework.
   - Example idea: a SoC DMA controller, PL330, Intel/ARM platform DMA engine,
     etc.

2. DMA CLIENT DRIVER
   - This is a normal device driver that USES DMA.
   - Examples: UART, SPI, I2S, MMC, NIC, audio driver, storage driver.
   - It requests a DMA channel and submits transfers.
   - It does NOT own the DMA controller hardware itself.

For this note, we care about the first one:

    DMA PROVIDER / DMA CONTROLLER DRIVER

because your question is:

    where does the DMA driver get registered,
    and how does it request IRQ during initialization?


BIG PICTURE
===========
A DMA controller driver is registered in TWO different ways:

A. It is first registered with the normal Linux driver model / bus layer.
B. Then, inside probe(), it registers itself as a DMAEngine provider.

That means there are really two registrations happening.


REGISTRATION #1: BUS-LEVEL DRIVER REGISTRATION
==============================================
First, the driver is registered with the bus/framework, just like an ordinary
platform or PCI driver.

Typical forms:

    platform_driver_register(&foo_driver);
    pci_register_driver(&foo_pci_driver);

Meaning:

    "I am a driver for this hardware type."

This is NOT yet the DMAEngine provider registration.
It only tells Linux:

    - match me against devices on this bus
    - call my probe() when hardware is found

So the first phase is generic device-driver registration.


REGISTRATION #2: DMAENGINE PROVIDER REGISTRATION
================================================
After the hardware is matched and probe() runs, the DMA controller driver fills
out a struct dma_device and registers that object with the DMAEngine core.

Typical call:

    dma_async_device_register(&foo->dma_dev);

Meaning:

    "I provide DMA channels and DMA services to client drivers."

So the first registration says:

    I am a driver for this device.

The second registration says:

    I am now a DMA service provider for other kernel drivers.


FULL HIGH-LEVEL LIFECYCLE
=========================
The full lifecycle usually looks like this:

    module_init(...)
        -> platform_driver_register(...) or pci_register_driver(...)
            -> bus match happens
                -> probe(dev)
                    -> map registers
                    -> get IRQ number
                    -> request_irq()
                    -> initialize hardware
                    -> initialize channel structs
                    -> fill struct dma_device
                    -> dma_async_device_register(...)
                    -> enable controller/channels/interrupts

Later:

    client driver
        -> dma_request_chan(...) or older request path
        -> prepare descriptor
        -> submit transfer
        -> issue pending

Then when transfer completes:

    DMA controller raises IRQ
        -> DMA controller ISR runs
        -> acknowledge interrupt
        -> mark descriptor complete
        -> callback / cookie completion / tasklet / bottom-half activity

Finally teardown:

    remove()
        -> disable controller
        -> disable/mask interrupts
        -> dma_async_device_unregister(...)
        -> free_irq(...)
        -> unmap registers
        -> release resources


WHY PROBE() MATTERS
===================
The real work does NOT usually happen in module_init() directly.

module_init() typically only registers the driver with the bus/framework.

The real device-specific initialization usually happens inside:

    probe()

because probe() is where the kernel has found a matching hardware instance and
can provide:

    - device object
    - MMIO resources
    - IRQ resources
    - PCI BARs or platform resources
    - DMA mask / device capability context

So when asking:

    where is the DMA driver initialized?

The practical answer is usually:

    inside probe()

not inside module_init() itself.


WHY IRQ SETUP IS INSIDE PROBE()
===============================
A DMA engine normally depends on interrupts for:

    - transfer completion
    - error reporting
    - descriptor recycling
    - channel progress tracking
    - wake-up of waiting client code

So the driver normally does NOT expose itself to DMAEngine clients until its IRQ
handling path is already ready.

That means the practical order is often:

    request_irq()
        BEFORE
    dma_async_device_register()

This ordering makes sense because clients should not be allowed to submit work
until the provider can actually receive and handle completion/error interrupts.


THE TWO SIDES OF THE SYSTEM
===========================
Think about the whole DMA system like this:

    +--------------------------------------------------+
    | DMA PROVIDER DRIVER                              |
    | - owns controller registers                      |
    | - requests IRQ                                   |
    | - registers channels with DMAEngine              |
    +---------------------------+----------------------+
                                |
                                v
    +--------------------------------------------------+
    | DMAENGINE CORE                                   |
    | - common framework                               |
    | - manages device/channel abstraction             |
    +---------------------------+----------------------+
                                |
                                v
    +--------------------------------------------------+
    | DMA CLIENT DRIVER                                |
    | - requests channel                               |
    | - submits memcpy/slave/SG transfer               |
    | - gets callback on completion                    |
    +--------------------------------------------------+

The provider is the low-level hardware-facing side.
The client is the user of DMA services.


TYPICAL INITIALIZATION ORDER IN DETAIL
======================================
A DMA controller driver's probe() often looks like this conceptually:

    1. allocate driver-private structure
    2. enable device / power / clocks / runtime PM state if needed
    3. acquire MMIO/IO resources
    4. ioremap() controller registers
    5. get IRQ number
    6. request_irq()
    7. reset or initialize DMA hardware
    8. initialize software channel structures
    9. initialize descriptor pools / lists / locks / tasklets
   10. fill struct dma_device
   11. advertise capabilities (DMA_MEMCPY, DMA_SLAVE, etc.)
   12. attach operation callbacks
   13. initialize channel list in dma_device
   14. register with DMAEngine core via dma_async_device_register()
   15. enable controller interrupts / channels / scheduling

This is the shape you should expect when reading a real provider driver.


WHAT HAPPENS IN MODULE_INIT()
=============================
The module init function is often very small.

Typical pattern:

    static int __init mydma_init(void)
    {
        return platform_driver_register(&mydma_driver);
    }

or:

    static int __init mydma_init(void)
    {
        return pci_register_driver(&mydma_pci_driver);
    }

Meaning:

    - The module is loaded.
    - Linux is told about the driver.
    - Hardware matching is delegated to the bus subsystem.
    - If a matching device exists, probe() will run.

So module_init() is often just the entry point into the driver model.


EXAMPLE: PLATFORM DRIVER SHAPE
==============================
A provider driver on SoC/platform hardware often looks conceptually like this:

    static struct platform_driver mydma_driver = {
        .probe  = mydma_probe,
        .remove = mydma_remove,
        .driver = {
            .name = "mydma",
        },
    };

    static int __init mydma_init(void)
    {
        return platform_driver_register(&mydma_driver);
    }

In this model:

    module_init()
        -> platform_driver_register()
            -> platform bus matches a device
                -> mydma_probe() runs

The same pattern applies to PCI, just with pci_driver instead.


EXAMPLE: PCI DRIVER SHAPE
=========================
A PCI-based DMA engine provider might look conceptually like this:

    static struct pci_driver mydma_pci_driver = {
        .name     = "mydma",
        .id_table = mydma_pci_ids,
        .probe    = mydma_pci_probe,
        .remove   = mydma_pci_remove,
    };

    static int __init mydma_init(void)
    {
        return pci_register_driver(&mydma_pci_driver);
    }

Then the flow becomes:

    module_init()
        -> pci_register_driver()
            -> PCI core finds matching device
                -> mydma_pci_probe() runs

Again: the real hardware init happens inside probe().


WHAT PROBE() USUALLY DOES FIRST
===============================
Inside probe(), the driver normally starts by building its private software
state.

Common early steps:

    - allocate struct mydma *d using kzalloc()/devm_kzalloc()
    - save dev pointer
    - initialize locks
    - initialize channel lists
    - initialize descriptor queues
    - initialize completion/tasklet/work structures

Example conceptual pattern:

    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;

    spin_lock_init(&d->lock);
    INIT_LIST_HEAD(&d->channels);

Then it begins resource setup.


MMIO / REGISTER MAPPING STAGE
=============================
A DMA controller usually has MMIO registers.
So probe() often does:

    - get resource from platform or PCI core
    - request region
    - ioremap() it

Platform-style conceptual flow:

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    d->regs = ioremap(res->start, resource_size(res));

PCI-style conceptual flow:

    pci_enable_device(pdev);
    pci_request_regions(pdev, "mydma");
    d->regs = pci_iomap(pdev, bar, size);

At this point the driver can access controller registers.


IRQ ACQUISITION STAGE
=====================
After registers are mapped, the driver usually gets the IRQ number.

Platform style:

    irq = platform_get_irq(pdev, 0);

PCI style:

    irq = pdev->irq;

The actual mechanism differs by bus, but conceptually the driver obtains the
interrupt line associated with the DMA controller.


REQUESTING THE IRQ
==================
After getting the IRQ number, the driver installs its handler.

Typical call:

    request_irq(d->irq, mydma_irq, flags, "mydma", d);

This means:

    - when this IRQ fires,
    - call mydma_irq()
    - pass d as the dev_id/private cookie

The dev_id/private pointer is extremely important because the ISR must usually
recover the per-device state.

Typical ISR signature:

    static irqreturn_t mydma_irq(int irq, void *dev_id)

Then inside ISR:

    struct mydma *d = dev_id;

This is how the interrupt handler knows which device instance it is serving.


WHY request_irq() COMES EARLY
=============================
IRQ registration usually happens before the DMA provider is made visible to
clients.

Why?

Because the provider should be able to:

    - detect transfer completion
    - detect errors
    - clear interrupt status
    - wake client state / invoke callbacks

before any client has a chance to submit transfers.

So a practical ordering is:

    map regs
    -> get IRQ
    -> request_irq
    -> init hardware
    -> register with DMAEngine core

This makes the driver operational before exposure.


RESET / HARDWARE INITIALIZATION STAGE
=====================================
Once MMIO and IRQ are ready, the driver initializes the DMA controller itself.

Typical actions:

    - reset controller
    - mask/clear pending interrupts
    - configure arbitration/scheduling
    - initialize per-channel registers
    - put hardware in idle state

Common pattern:

    - write reset bit
    - poll until reset completes
    - clear interrupt status register
    - disable all channels initially

The exact register names are hardware-specific, but conceptually this stage puts
hardware into a known safe state.


SOFTWARE CHANNEL INITIALIZATION
===============================
A DMA engine usually has multiple channels.
The provider driver creates software objects representing those hardware
channels.

Typical per-channel software state:

    - channel id/index
    - register base / offset
    - pending descriptor list
    - active descriptor pointer
    - completed descriptor list
    - lock
    - tasklet/work item
    - embedded dma_chan or virt-dma style object (depending on era/framework)

Conceptually:

    for each hardware channel:
        initialize channel state
        attach channel to provider dma_device channel list

This is one of the most important stages before registration with DMAEngine.


DESCRIPTOR / QUEUE INITIALIZATION
=================================
A DMA controller driver usually manages transfer descriptors in software.

So initialization often includes:

    - free descriptor list
    - pending list
    - active list
    - completed list
    - spinlocks protecting lists
    - tasklets/workqueues for completion handling

Why?

Because the ISR should usually do minimal hardirq work:

    - ack interrupt
    - mark progress/completion
    - maybe schedule tasklet/work

and let deferred context recycle descriptors and invoke callbacks more safely.


FILLING struct dma_device
=========================
This is the central DMAEngine provider object.
The provider driver fills it before calling dma_async_device_register().

Typical fields/concepts that matter:

    d->dma_dev.dev
        - back-pointer to device object

    d->dma_dev.cap_mask
        - what operations this engine supports

    d->dma_dev.channels
        - list of exposed DMA channels

    operation callbacks, such as:
        device_alloc_chan_resources
        device_free_chan_resources
        device_prep_dma_memcpy
        device_prep_slave_sg
        device_issue_pending
        device_tx_status
        device_control

Not every driver fills every callback.
It depends on capabilities and kernel version/framework style.


DMA CAPABILITY MASK
===================
Before registering, the driver advertises what the hardware supports.

Examples of capabilities:

    DMA_MEMCPY
        - memory-to-memory DMA copies

    DMA_SLAVE
        - slave/peripheral transfers

    DMA_CYCLIC
        - cyclic DMA, often used for audio

Conceptually:

    dma_cap_zero(d->dma_dev.cap_mask);
    dma_cap_set(DMA_MEMCPY, d->dma_dev.cap_mask);

This tells the DMAEngine core and client drivers what kinds of requests can be
submitted to this provider.


CHANNEL LIST REGISTRATION
=========================
The dma_device object usually has a channel list.
Each software channel object is linked into that list before provider
registration.

Conceptually:

    INIT_LIST_HEAD(&d->dma_dev.channels);

    for each channel:
        list_add_tail(&chan->device_node, &d->dma_dev.channels);

This is how the framework discovers the provider's channels.


DMAENGINE PROVIDER REGISTRATION CALL
====================================
After hardware state, channel objects, IRQ path, and callbacks are ready, the
provider registers itself with the DMAEngine core.

Typical call:

    dma_async_device_register(&d->dma_dev);

This is the moment when the DMA controller becomes discoverable as a DMA service
provider.

Only after this point should client drivers be able to request channels from
it.

So you can think of this as the "provider is now live" point.


WHY dma_async_device_register() SHOULD COME AFTER IRQ SETUP
===========================================================
If you register the provider too early, clients may start submitting work before
all of these are ready:

    - IRQ handler installed
    - hardware reset/initialized
    - channel state ready
    - interrupt status clear
    - descriptor queues initialized

That would be dangerous.

So the sane order is:

    1. request_irq
    2. initialize hardware
    3. initialize channels / lists / descriptors
    4. fill dma_device
    5. dma_async_device_register

This is the order you should expect in a good provider driver.


WHEN INTERRUPTS ARE ENABLED IN HARDWARE
=======================================
There are two related but different things:

1. request_irq()
   - installs the kernel-side interrupt handler

2. enabling interrupts in the DMA controller hardware
   - allows the controller to actually generate interrupts

These are not the same.

A common safe ordering is:

    request_irq()
    -> clear pending status in controller
    -> initialize hardware/channel state
    -> register provider
    -> finally enable channel/controller interrupt generation

That way, if the hardware generates an interrupt, the kernel already has a
handler ready.


INTERRUPT HANDLER RESPONSIBILITIES
==================================
The DMA controller ISR typically does some combination of:

    - read interrupt status register
    - determine which channel(s) fired
    - detect completion vs error
    - acknowledge/clear interrupt bits
    - update active descriptor state
    - mark cookie/transaction complete
    - schedule bottom-half/tasklet/workqueue
    - maybe start next queued descriptor

Typical conceptual ISR:

    static irqreturn_t mydma_irq(int irq, void *dev_id)
    {
        struct mydma *d = dev_id;
        u32 status;

        status = readl(d->regs + IRQ_STATUS);
        if (!status)
            return IRQ_NONE;

        writel(status, d->regs + IRQ_STATUS);   // ack/clear

        for each channel bit set in status:
            handle completion or error
            maybe move descriptor to done list
            maybe queue tasklet

        return IRQ_HANDLED;
    }

This is the control point that turns hardware completion into software-visible
completion.


WHY THE ISR IS CRITICAL TO DMAENGINE
====================================
The provider is not useful unless it can turn hardware events into framework
state changes.

The ISR is what usually allows the provider to:

    - complete cookies
    - invoke client callbacks
    - report errors
    - recycle descriptors
    - dispatch next transfer

So when studying a DMA controller driver, the ISR is just as important as
probe().


CLIENT-SIDE VIEW AFTER PROVIDER REGISTRATION
============================================
After the provider is registered, client drivers can later do something like:

    dma_request_chan(...)
    prep descriptor
    tx_submit(...)
    issue_pending(...)

Then the provider driver's channel/descriptor logic eventually programs the DMA
controller hardware.

When transfer completes:

    hardware raises IRQ
    -> provider ISR runs
    -> provider updates DMAEngine state
    -> client completion path is triggered

This is the full end-to-end picture.


COMPLETE PROVIDER FLOW IN ONE ASCII DIAGRAM
===========================================

    module load
        |
        v
    module_init()
        |
        v
    platform_driver_register() / pci_register_driver()
        |
        v
    bus matching finds hardware
        |
        v
    probe()
        |
        +--> allocate private struct
        |
        +--> enable device / clocks / power
        |
        +--> get MMIO resource
        |
        +--> ioremap controller registers
        |
        +--> get IRQ number
        |
        +--> request_irq(mydma_irq, ..., dev_id=d)
        |
        +--> reset/init controller hardware
        |
        +--> init software channels
        |
        +--> init descriptor queues / locks / tasklets
        |
        +--> fill struct dma_device
        |
        +--> attach channels to dma_dev.channels
        |
        +--> dma_async_device_register(&d->dma_dev)
        |
        +--> enable controller/channel interrupts
        |
        v
    provider now visible to clients
        |
        v
    client requests channel and submits transfer
        |
        v
    hardware DMA runs
        |
        v
    interrupt fires
        |
        v
    mydma_irq()
        |
        +--> ack interrupt
        +--> detect completion/error
        +--> complete descriptor/cookie
        +--> callback/bottom-half/next transfer
        |
        v
    transfer completion observed by client


PRACTICAL PSEUDO-SKELETON OF A PROVIDER DRIVER
==============================================
The following is a conceptual skeleton showing the shape you should expect.
It is not meant to be copied verbatim.

    struct mydma {
        struct device *dev;
        void __iomem *regs;
        int irq;
        struct dma_device dma_dev;
        struct mydma_chan chans[N];
        spinlock_t lock;
    };

    static irqreturn_t mydma_irq(int irq, void *dev_id)
    {
        struct mydma *d = dev_id;
        u32 status;

        status = readl(d->regs + IRQ_STATUS);
        if (!status)
            return IRQ_NONE;

        writel(status, d->regs + IRQ_STATUS);

        /* Per-channel completion/error handling here */

        return IRQ_HANDLED;
    }

    static int mydma_probe(struct platform_device *pdev)
    {
        struct mydma *d;
        int ret;
        int i;

        d = kzalloc(sizeof(*d), GFP_KERNEL);
        if (!d)
            return -ENOMEM;

        d->dev = &pdev->dev;
        spin_lock_init(&d->lock);

        /* Get MMIO resource, map registers */
        d->regs = ioremap(...);
        if (!d->regs) {
            ret = -ENOMEM;
            goto err_free;
        }

        /* Get interrupt line */
        d->irq = platform_get_irq(pdev, 0);
        if (d->irq < 0) {
            ret = d->irq;
            goto err_unmap;
        }

        /* Install handler before exposing provider */
        ret = request_irq(d->irq, mydma_irq, 0, "mydma", d);
        if (ret)
            goto err_unmap;

        /* Reset/init hardware */
        mydma_hw_reset(d);
        mydma_clear_irqs(d);

        /* Initialize DMAEngine provider object */
        INIT_LIST_HEAD(&d->dma_dev.channels);
        dma_cap_zero(d->dma_dev.cap_mask);
        dma_cap_set(DMA_MEMCPY, d->dma_dev.cap_mask);
        d->dma_dev.dev = &pdev->dev;

        d->dma_dev.device_alloc_chan_resources = mydma_alloc_chan_resources;
        d->dma_dev.device_free_chan_resources  = mydma_free_chan_resources;
        d->dma_dev.device_prep_dma_memcpy      = mydma_prep_dma_memcpy;
        d->dma_dev.device_issue_pending        = mydma_issue_pending;
        d->dma_dev.device_tx_status            = mydma_tx_status;

        for (i = 0; i < N; i++) {
            mydma_chan_init(d, &d->chans[i], i);
            list_add_tail(&d->chans[i].chan.device_node,
                          &d->dma_dev.channels);
        }

        ret = dma_async_device_register(&d->dma_dev);
        if (ret)
            goto err_irq;

        mydma_enable_irqs(d);
        mydma_enable_controller(d);

        return 0;

    err_irq:
        free_irq(d->irq, d);
    err_unmap:
        iounmap(d->regs);
    err_free:
        kfree(d);
        return ret;
    }

This skeleton shows the high-level order clearly:

    map regs
    -> get irq
    -> request irq
    -> init hardware
    -> init dma_device and channels
    -> register with dmaengine core
    -> enable operation


TEARDOWN / REMOVE PATH
======================
The remove path is normally the reverse order of probe().

Common sequence:

    1. disable controller / stop new work
    2. mask interrupts in hardware
    3. dma_async_device_unregister(&d->dma_dev)
    4. free_irq(d->irq, d)
    5. unmap registers
    6. release PCI/platform resources
    7. free private structures

Conceptual remove skeleton:

    static int mydma_remove(struct platform_device *pdev)
    {
        struct mydma *d = platform_get_drvdata(pdev);

        mydma_disable_controller(d);
        mydma_disable_irqs(d);
        dma_async_device_unregister(&d->dma_dev);
        free_irq(d->irq, d);
        iounmap(d->regs);
        kfree(d);
        return 0;
    }

The reverse ordering matters because you do not want interrupts firing or
clients using channels after resources are being torn down.


WHY CLEANUP ORDER MATTERS
=========================
Bad cleanup ordering can cause:

    - interrupt fires after free_irq or after structures are freed
    - client still sees provider while channels are half-torn-down
    - DMA hardware still running after register mapping is removed
    - use-after-free bugs in ISR/tasklet/completion paths

So cleanup order is not cosmetic; it is correctness-critical.


WHAT TO LOOK FOR IN A REAL 2.6.39 DMA PROVIDER DRIVER
=====================================================
When you open a real provider driver under drivers/dma/, search for these in
this order:

    1. module_init / subsys_initcall / platform_driver_register / pci_register_driver
    2. probe()
    3. resource acquisition
       - platform_get_resource / pci_request_regions / pci_iomap / ioremap
    4. IRQ acquisition
       - platform_get_irq / pdev->irq
    5. request_irq()
    6. hardware reset/init
    7. channel init
    8. list initialization
    9. dma_device field initialization
   10. dma_async_device_register()
   11. ISR function
   12. remove()

If you trace those 12 things, you will understand the full lifecycle.


QUESTIONS TO ASK WHILE READING A DMA PROVIDER DRIVER
====================================================
Use these questions while reading the code:

    1. What bus is this driver on?
       - platform or PCI?

    2. Where is the driver first registered?
       - module_init + platform_driver_register/pci_register_driver?

    3. Where is the actual hardware init done?
       - probe()?

    4. Where do MMIO registers get mapped?

    5. Where does the IRQ number come from?

    6. Where is request_irq() called?

    7. Does the driver clear pending interrupts before enabling them?

    8. How are channels represented in software?

    9. When is struct dma_device filled?

   10. When is dma_async_device_register() called?

   11. Is provider registration done only after hardware/IRQ/channel init?

   12. What does the ISR do on completion vs error?

   13. Does ISR complete transfers directly or schedule bottom-half work?

   14. Where is the next queued transfer started?

   15. What is the cleanup order in remove()?

These questions will keep your reading structured.


VERY IMPORTANT CONCEPTUAL SUMMARY
=================================
There are two distinct registration points:

    Registration with BUS/DRIVER MODEL:
        platform_driver_register() / pci_register_driver()

    Registration with DMAENGINE CORE:
        dma_async_device_register()

And IRQ setup usually occurs in between:

    bus registration
        -> probe()
            -> map regs
            -> get IRQ
            -> request_irq()
            -> init hardware + channels + dma_device
            -> dma_async_device_register()

That is the single most important structural idea.


ONE-LINE MENTAL MODEL
=====================
A DMA controller driver is first introduced to Linux as a normal device driver,
and only after its hardware, IRQ, channels, and callbacks are initialized inside
probe() does it register itself as a DMAEngine provider for client drivers.


END-TO-END MINI SUMMARY
=======================
If you want the entire story in the shortest form:

    module_init()
        registers platform/pci driver

    probe()
        maps registers
        gets IRQ number
        calls request_irq()
        resets controller
        initializes channels/descriptors
        fills struct dma_device
        calls dma_async_device_register()
        enables interrupts and hardware

    ISR
        handles completion/error
        clears interrupt
        completes descriptors/cookies
        triggers client callback path

    remove()
        disables hardware and interrupts
        unregisters dma_device
        frees IRQ
        unmaps resources

That is the provider lifecycle you should look for in Linux 2.6.39.

