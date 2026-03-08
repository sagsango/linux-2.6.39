LINUX 2.6.39 DMA ROADMAP
========================

Goal
====
Build a clear, systematic understanding of DMA in Linux 2.6.39 by reading the code in the right order.


0. BIG PICTURE
==============

Think of DMA in Linux 2.6.39 as 5 layers:

    +------------------------------+
    | Driver                       |
    | e.g. NIC / block / SCSI      |
    +--------------+---------------+
                   |
                   v
    +------------------------------+
    | Generic DMA API              |
    | dma_alloc_coherent()         |
    | dma_map_single()             |
    | dma_map_page()               |
    | dma_map_sg()                 |
    | dma_unmap_*()                |
    | dma_sync_*()                 |
    +--------------+---------------+
                   |
                   v
    +------------------------------+
    | dma_map_ops / arch backend   |
    | direct DMA                   |
    | SWIOTLB bounce               |
    | IOMMU path                   |
    +--------------+---------------+
                   |
                   v
    +------------------------------+
    | page allocator / memory      |
    | zones / low memory / pages   |
    +--------------+---------------+
                   |
                   v
    +------------------------------+
    | Device sees DMA address      |
    | bus address / IOVA / bounce  |
    +------------------------------+


There are 2 major DMA usage classes:

    1. COHERENT / CONSISTENT DMA
       - descriptor rings
       - command blocks
       - control structures
       - long-lived shared memory between CPU and device

    2. STREAMING DMA
       - packet payloads
       - block I/O data
       - temporary transfer buffers
       - map -> use -> unmap lifecycle

Short mnemonic:

    coherent  = shared metadata structures
    streaming = temporary data transfer


1. CORE MENTAL MODEL
====================

Keep these address spaces separate at all times:

    1. CPU virtual address
       - normal kernel pointer
       - what C code dereferences

    2. CPU physical address
       - actual RAM location

    3. DMA address (dma_addr_t)
       - what driver programs into device descriptor/register

    4. IOVA / bus-visible address
       - what device sees through IOMMU or platform translation

Possible relationships:

    Direct DMA:
        dma_addr ~= physical address (+ optional platform offset)

    IOMMU DMA:
        dma_addr = IOVA
        IOMMU translates IOVA -> physical

    SWIOTLB DMA:
        dma_addr points to bounce buffer
        original CPU buffer may be elsewhere

This is the key reason DMA code looks complicated: CPU address != device address in general.


2. BEST READING ORDER
=====================

PHASE A: API CONTRACT
---------------------
Read first:

    Documentation/DMA-API.txt
    include/linux/dma-mapping.h

Goal:
    - understand driver contract
    - coherent vs streaming
    - sync semantics
    - map/unmap lifecycle
    - DMA mask rules

Do NOT jump into arch code first.


PHASE B: OLD PCI WRAPPERS
-------------------------
Read:

    include/linux/pci-dma-compat.h
    include/linux/pci.h

Important because many drivers in this era still use:

    pci_alloc_consistent()
    pci_map_single()
    pci_unmap_single()
    pci_map_sg()
    pci_unmap_sg()

Mental map:

    pci_* DMA wrappers
          -> dma_* generic API
          -> dma_map_ops backend


PHASE C: CENTRAL DMA ABSTRACTION
--------------------------------
Read carefully in:

    include/linux/dma-mapping.h

Focus on:

    struct dma_map_ops

Typical function pointers you will see conceptually:

    alloc
    free
    map_page
    unmap_page
    map_sg
    unmap_sg
    sync_single_for_cpu
    sync_single_for_device
    sync_sg_for_cpu
    sync_sg_for_device
    mapping_error
    dma_supported

Core idea:

    Driver calls generic DMA API
           |
           v
    Linux resolves device/arch dma_map_ops
           |
           v
    Backend performs actual DMA mapping work

This is the systematic structure you were asking about.


PHASE D: X86 BACKEND
--------------------
Read:

    arch/x86/include/asm/dma-mapping.h
    arch/x86/kernel/pci-dma.c

Questions to answer:

    - How are dma ops chosen on x86?
    - What is the default direct DMA path?
    - When does SWIOTLB get used?
    - When does IOMMU path get used?
    - How does dma_alloc_coherent() really allocate memory?
    - How does dma_map_single() really produce dma_addr_t?


PHASE E: BOUNCE BUFFERING
-------------------------
Read:

    lib/swiotlb.c
    include/linux/swiotlb.h

Why this matters:

    Device might be limited to 32-bit DMA
    RAM may live above 4GB
    Buffer may not be device-reachable

So kernel may do:

    original buffer
       -> bounce buffer in reachable memory
       -> device DMA uses bounce address
       -> copy back if needed


PHASE F: DEBUG HELPERS
----------------------
Read later:

    lib/dma-debug.c

Useful for understanding invariants and catching misuse, but not first-pass material.


PHASE G: REAL DRIVER
--------------------
After the API and backend make sense, read one driver.

Best starter choice:

    A NIC driver

Why:
    - coherent descriptor ring allocation
    - streaming TX/RX buffer mapping
    - easy to see full lifecycle

Driver pattern to look for:

    probe()
      -> set DMA mask
      -> allocate rings with dma_alloc_coherent()

    transmit path
      -> dma_map_single() packet buffer
      -> write dma_addr_t into descriptor
      -> notify hardware

    completion path
      -> dma_unmap_single()
      -> reclaim/free buffer


3. COHERENT VS STREAMING DMA
============================

COHERENT DMA
------------
Used for:
    - descriptor rings
    - command blocks
    - control metadata

Path:

    driver
      -> dma_alloc_coherent(dev, size, &dma_handle, gfp)
           -> dma_map_ops->alloc()
                -> allocate suitable memory
                -> return CPU virtual address
                -> return DMA address

Driver receives:
    cpu_addr   = software uses this pointer
    dma_handle = hardware/device uses this address

Typical properties:
    - long-lived allocation
    - shared between CPU and device
    - simpler ownership model
    - often used for ring metadata, not large payloads


STREAMING DMA
-------------
Used for:
    - packet payloads
    - block I/O buffers
    - temporary transfers

Path:

    driver has normal buffer
      -> dma_map_single(dev, cpu_addr, size, dir)
           -> dma_map_ops->map_page()/map_single equivalent backend
           -> direct / swiotlb / iommu path
           -> returns dma_addr_t
      -> driver writes dma_addr_t to device descriptor
      -> device DMA happens
      -> completion
      -> dma_unmap_single(dev, dma_addr, size, dir)

Typical properties:
    - temporary
    - map before transfer
    - unmap after transfer
    - payload-oriented

Short contrast:

    coherent  = allocate special shared memory
    streaming = temporarily map existing memory


4. ADDRESS AND OWNERSHIP MODEL
==============================

When reading any DMA code, ask:

    - Who owns the buffer right now?
      CPU or device?

    - Which address is being passed?
      cpu pointer, physical address, or dma_addr_t?

    - Is this mapping direct, bounced, or IOMMU-backed?

    - Is this coherent memory or streaming memory?

    - Is cache sync required here?

On x86, coherency is often easier than on non-coherent architectures, but still learn the API rules, not just x86 behavior.


5. DMA MASK STORY
=================

This is one of the most important conceptual parts.

Key functions:

    dma_set_mask()
    dma_set_coherent_mask()
    dma_supported()
    pci_set_dma_mask()
    pci_set_consistent_dma_mask()

Mental model:

    Device says:
        I can only generate N-bit DMA addresses.

    Kernel says:
        Then I must ensure the buffer I hand you is reachable.

Possible outcomes:

    1. allocate from reachable memory
    2. use IOMMU mapping
    3. use SWIOTLB bounce buffer
    4. fail mapping / probe

Classic example:

    32-bit PCI device + RAM above 4GB

Without IOMMU or suitable low-memory allocation, the device cannot reach high RAM directly.

So kernel may:
    - allocate low memory for coherent DMA
    - bounce streaming mappings through SWIOTLB


6. DIRECT DMA / SWIOTLB / IOMMU
===============================

A. DIRECT DMA
-------------

Simple case:

    CPU buffer
      -> physical address reachable by device
      -> device DMA address is directly usable

Picture:

    cpu buffer
      -> phys addr
      -> dma addr
      -> device accesses RAM directly

Best mental model:

    dma_addr is effectively a device-usable physical address


B. SWIOTLB
----------

Used when direct DMA is not possible due to mask/addressability constraints.

Picture:

    original CPU buffer
         |
         +--> not reachable by device
         |
         v
    allocate bounce buffer in reachable memory
         |
         +--> copy in for DMA_TO_DEVICE
         +--> device DMA uses bounce buffer address
         +--> copy out for DMA_FROM_DEVICE

Core effect:

    device does NOT DMA to original buffer directly

Important read target:

    lib/swiotlb.c

Focus on:
    - slot allocation
    - bounce pool management
    - map/unmap behavior
    - copy-in/copy-out timing


C. IOMMU
--------

Used when platform/kernel config gives a translation layer for DMA.

Picture:

    device issues DMA to IOVA
          |
          v
    IOMMU translates IOVA -> physical RAM

So:

    dma_addr_t may be an IOVA, not raw physical address

This is why drivers must treat dma_addr_t as opaque device address, not as a physical address.


7. CORE SOURCE FILES TO READ
=============================

Primary files:

    Documentation/DMA-API.txt
    include/linux/dma-mapping.h
    include/linux/pci-dma-compat.h
    include/linux/pci.h
    arch/x86/include/asm/dma-mapping.h
    arch/x86/kernel/pci-dma.c
    include/linux/swiotlb.h
    lib/swiotlb.c
    lib/dma-debug.c


8. EXACT QUESTIONS TO ASK WHILE READING
=======================================

For every function, ask:

    1. Is this coherent or streaming?
    2. Is this just a wrapper or the actual implementation?
    3. Is it returning CPU address, DMA address, or both?
    4. Does it assume direct DMA?
    5. Does it check the device DMA mask?
    6. Can it fall back to SWIOTLB?
    7. Can it route into IOMMU mapping?
    8. Does cache synchronization matter here?
    9. What work actually happens on unmap?
   10. Under what condition can mapping fail?

This question set will prevent you from getting lost.


9. MOST IMPORTANT API CALLS TO TRACE
====================================

Read and trace these:

    dma_alloc_coherent()
    dma_free_coherent()
    dma_map_single()
    dma_unmap_single()
    dma_map_page()
    dma_unmap_page()
    dma_map_sg()
    dma_unmap_sg()
    dma_sync_single_for_cpu()
    dma_sync_single_for_device()
    dma_supported()
    dma_set_mask()
    dma_set_coherent_mask()

And old PCI wrappers:

    pci_alloc_consistent()
    pci_free_consistent()
    pci_map_single()
    pci_unmap_single()
    pci_map_sg()
    pci_unmap_sg()
    pci_set_dma_mask()
    pci_set_consistent_dma_mask()


10. COMPACT CALL FLOW MAP
=========================

GENERIC DMA FLOW
----------------

    Driver
      |
      |-- set DMA mask
      |
      |-- coherent allocation path
      |      dma_alloc_coherent()
      |           |
      |           v
      |      dma_map_ops->alloc()
      |           |
      |           +--> direct allocation path
      |           +--> SWIOTLB-aware path
      |           +--> IOMMU-aware path
      |
      |-- streaming map path
      |      dma_map_single() / dma_map_page()
      |           |
      |           v
      |      dma_map_ops->map_page()
      |           |
      |           +--> direct map
      |           +--> SWIOTLB bounce
      |           +--> IOMMU map
      |
      |-- device performs DMA using dma_addr_t
      |
      |-- completion path
      |      dma_unmap_single() / dma_unmap_page()
      |           |
      |           v
      |      dma_map_ops->unmap_page()
      |           |
      |           +--> teardown / copy-back / cleanup
      |
      |-- optional sync helpers if required
      |      dma_sync_*()
      |
      v
    driver reclaims buffer / ring entry


11. GREP PLAN FOR YOUR TREE
===========================

Run these in linux-2.6.39:

    grep -R "struct dma_map_ops" -n .
    grep -R "dma_alloc_coherent" -n .
    grep -R "dma_free_coherent" -n .
    grep -R "dma_map_single" -n .
    grep -R "dma_map_page" -n .
    grep -R "dma_map_sg" -n .
    grep -R "dma_unmap_single" -n .
    grep -R "dma_unmap_page" -n .
    grep -R "dma_unmap_sg" -n .
    grep -R "dma_sync_single_for_cpu" -n .
    grep -R "dma_sync_single_for_device" -n .
    grep -R "dma_supported" -n .
    grep -R "dma_set_mask" -n .
    grep -R "pci_map_single" -n .
    grep -R "pci_alloc_consistent" -n .
    grep -R "swiotlb" -n .

Suggested notebook template:

    API/function            Wrapper?   Backend?   Direct/SWIOTLB/IOMMU   Notes
    --------------------------------------------------------------------------
    pci_map_single          yes        generic    depends                old PCI wrapper
    dma_map_single          maybe      dma_ops    depends                streaming
    dma_alloc_coherent      maybe      dma_ops    depends                coherent alloc
    swiotlb_map_page        no         swiotlb    bounce                 copy-based path


12. FASTEST HIGH-VALUE READING PATH
===================================

DAY 1
-----
    Documentation/DMA-API.txt
    include/linux/dma-mapping.h

DAY 2
-----
    include/linux/pci-dma-compat.h
    arch/x86/include/asm/dma-mapping.h

DAY 3
-----
    arch/x86/kernel/pci-dma.c

DAY 4
-----
    lib/swiotlb.c

DAY 5
-----
    one NIC driver using both coherent rings and streaming payload maps

This is enough to build a strong DMA model.


13. WHAT NOT TO GET LOST IN ON FIRST PASS
=========================================

Do NOT sink too deep initially into:

    - every non-x86 architecture
    - every dma-debug detail
    - every scatterlist helper corner case
    - every NUMA allocation detail
    - every IOMMU implementation variant

First lock in this core chain:

    driver API
      -> dma_map_ops abstraction
      -> x86 backend
      -> direct vs SWIOTLB vs IOMMU
      -> coherent vs streaming

Only then widen the scope.


14. WHAT YOU SHOULD EXTRACT FROM EACH MAJOR FILE
================================================

A. Documentation/DMA-API.txt
----------------------------
Extract:
    - definitions
    - coherent vs streaming distinction
    - driver ownership rules
    - sync rules
    - error-handling expectations

B. include/linux/dma-mapping.h
------------------------------
Extract:
    - generic API surface
    - dma_map_ops abstraction
    - wrapper layering
    - per-device DMA entry model

C. include/linux/pci-dma-compat.h
---------------------------------
Extract:
    - how old pci_* wrappers map to dma_* API

D. arch/x86/include/asm/dma-mapping.h
-------------------------------------
Extract:
    - x86-specific helper layer
    - arch hooks/macros/types

E. arch/x86/kernel/pci-dma.c
----------------------------
Extract:
    - actual x86 DMA path selection
    - direct vs SWIOTLB vs IOMMU routing
    - coherent alloc backend
    - streaming map backend

F. lib/swiotlb.c
----------------
Extract:
    - bounce pool model
    - map/unmap behavior
    - copy-in / copy-back rules
    - why address limits matter

G. NIC driver
-------------
Extract:
    - full end-to-end practical flow
    - probe/setup, TX map, RX map, completion unmap


15. COMPACT MENTAL DIAGRAM
==========================

    +--------------------------------------------------------------+
    | DRIVER                                                       |
    |  - set DMA mask                                              |
    |  - allocate descriptor rings                                 |
    |  - map packet/data buffers                                   |
    +---------------------------+----------------------------------+
                                |
                                v
    +--------------------------------------------------------------+
    | GENERIC DMA API                                               |
    |  dma_alloc_coherent()                                         |
    |  dma_map_single(), dma_map_page(), dma_map_sg()               |
    |  dma_unmap_*, dma_sync_*                                      |
    +---------------------------+----------------------------------+
                                |
                                v
    +--------------------------------------------------------------+
    | DMA BACKEND (dma_map_ops)                                     |
    |  alloc / free / map_page / unmap_page / map_sg / sync         |
    +-------------------+-------------------+----------------------+
                        |                   |
                        |                   |
                        v                   v
               +----------------+   +----------------+
               | Direct DMA     |   | SWIOTLB        |
               | device reaches |   | bounce buffer  |
               | target memory  |   | in low memory  |
               +----------------+   +----------------+
                        |
                        | optional alternative backend
                        v
               +----------------+
               | IOMMU          |
               | IOVA -> phys   |
               +----------------+


16. FINAL PRACTICAL GUIDANCE
============================

Best way to study linux-2.6.39 DMA:

    1. Learn API contract first
    2. Learn dma_map_ops abstraction second
    3. Learn x86 backend third
    4. Learn SWIOTLB fourth
    5. Read one real driver last

At every step, keep 3 questions in your head:

    - What address does the CPU use?
    - What address does the device use?
    - Who owns the buffer right now?

If those 3 stay clear, DMA code becomes much easier to understand.


17. NEXT DEEP-DIVE PATH
=======================

Recommended deep-dive order from here:

    Step 1: include/linux/dma-mapping.h
            - understand dma_map_ops
            - understand generic wrappers

    Step 2: arch/x86/kernel/pci-dma.c
            - see actual x86 implementation

    Step 3: lib/swiotlb.c
            - understand bounce path

    Step 4: one NIC driver
            - connect theory to real device usage


18. ONE-LINE SUMMARY
====================

Linux 2.6.39 is a strong kernel for learning DMA because it is mature enough to be systematic, but still old enough that the core DMA model remains visible and understandable.

