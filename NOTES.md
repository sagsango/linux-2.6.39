LINUX 2.6.39 PCI SCAN, DISCOVERY, AND DRIVER PROBE NOTES
========================================================

Goal
====
Understand where PCI scanning happens in Linux 2.6.39, how devices are
discovered, how the PCI bus tree is walked, when struct pci_dev objects are
created, and when driver probe() is finally called.

CORE IDEA
=========
PCI scanning happens EARLY during kernel boot, long before any individual PCI
driver's probe() runs.

That means:

    PCI scan happens first
    -> devices are discovered
    -> struct pci_dev objects are created
    -> devices are added to the kernel device model
    -> only then are PCI drivers matched
    -> only then does pci_driver.probe() run

So if you are asking:

    where does my PCI DMA controller driver come from?

The answer begins at:

    PCI bus scanning

not at the driver itself.


BIG PICTURE BOOT FLOW
=====================
The simplified flow looks like this:

    kernel boot
       |
       v
    PCI subsystem initialization
       |
       v
    architecture-specific PCI initialization
       |
       v
    root bus scan begins
       |
       v
    PCI devices discovered by reading config space
       |
       v
    struct pci_dev created for each discovered device
       |
       v
    devices added to Linux device model
       |
       v
    PCI driver matching happens
       |
       v
    matching driver's probe() runs

Compact version:

    boot
      -> PCI subsystem init
      -> pci_scan_root_bus()
      -> pci_scan_child_bus()
      -> pci_scan_device()
      -> pci_device_add()
      -> driver match
      -> pci_driver.probe()


PCI DISCOVERY IS SEPARATE FROM PCI DRIVER PROBE
===============================================
This is one of the most important conceptual separations.

PCI DISCOVERY phase:
    - walk buses
    - read config space
    - identify devices
    - build struct pci_dev objects
    - add devices to kernel model

PCI DRIVER BINDING phase:
    - compare discovered devices against driver ID tables
    - if vendor/device match, call driver probe()

So:

    scan != probe

Scanning finds hardware.
Probing attaches a driver to hardware that was already found.


MAIN PCI CORE FILES TO READ
===========================
In Linux 2.6.39, the main files to read for this topic are:

    drivers/pci/pci.c
    drivers/pci/probe.c
    arch/x86/pci/init.c
    arch/x86/pci/common.c

These files together explain:

    - PCI subsystem init
    - architecture-level PCI setup
    - bus scanning
    - device creation
    - when devices become visible to the driver core


PCI SUBSYSTEM INITIALIZATION ENTRY POINT
========================================
A key early entry point is in:

    drivers/pci/pci.c

You should look for:

    subsys_initcall(pci_subsys_init);

This tells you the PCI subsystem gets initialized during the kernel initcall
sequence.

Conceptually:

    kernel initcall framework
        -> pci_subsys_init()

That means PCI core setup happens during boot, not when a particular PCI driver
module is loaded later.


WHAT pci_subsys_init() DOES
===========================
In high-level conceptual terms, pci_subsys_init() performs early PCI subsystem
setup.

Typical responsibilities include things like:

    - initializing PCI proc/sysfs support
    - initializing PCI bus/core structures
    - preparing the PCI subsystem for scanning and device management

Conceptual form:

    static int __init pci_subsys_init(void)
    {
        pci_proc_init();
        pci_sysfs_init();
        pci_bus_init();
        return 0;
    }

The exact content may vary slightly by kernel version, but the important point
is that this is PCI core setup, not the full root bus scan itself.


ARCHITECTURE-SPECIFIC PCI INITIALIZATION
========================================
The actual start of PCI bus scanning is architecture-dependent.

For x86, the interesting files are usually:

    arch/x86/pci/init.c
    arch/x86/pci/common.c

The architecture code is important because:

    - it knows how to access PCI config space on the platform
    - it knows how root buses are created on that architecture
    - it triggers root-bus scanning

So the PCI core is generic, but the architecture layer often kicks off the real
bus enumeration process.


WHY ARCH CODE IS INVOLVED
=========================
PCI is logically generic, but root-complex discovery and configuration-space
access have architecture/platform-specific aspects.

The architecture layer provides things like:

    - how to read/write PCI config space
    - how to create root buses
    - what host bridge/controller exists
    - platform quirks

So the scan path is usually:

    generic PCI core + arch-specific host bridge / root bus setup


ROOT BUS SCANNING
=================
Once the PCI subsystem and architecture PCI layer are ready, the kernel starts
scanning from the root bus.

Important functions to look for in:

    drivers/pci/probe.c

are:

    pci_scan_root_bus()
    pci_scan_bus()
    pci_scan_child_bus()
    pci_scan_slot()
    pci_scan_device()
    pci_scan_bridge()
    pci_setup_device()
    pci_device_add()

These functions are the heart of PCI discovery.


WHAT pci_scan_root_bus() MEANS
==============================
The root bus is the top-level PCI bus hanging off the host bridge / root
complex.

Conceptually:

    CPU / host bridge
         |
         v
      PCI bus 0

The kernel starts from this root bus and then recursively explores everything
reachable beneath it.

So:

    pci_scan_root_bus()

means:

    create the initial bus object and begin enumeration from there.


WHAT pci_scan_bus() / pci_scan_child_bus() DO
=============================================
After the root bus exists, the kernel scans devices on that bus.

High-level idea:

    for each device number on the bus (0..31)
        for each function number (0..7, if multi-function applies)
            read config space
            if a valid device exists
                create pci_dev object
                initialize device metadata
                if device is a PCI bridge
                    recursively scan downstream secondary bus

That recursive walking is the key to PCI topology discovery.


PCI TOPOLOGY AS A TREE
======================
You should imagine PCI topology as a tree.

Example:

    Root bus 0
       |
       +-- 00:00.0 host / root complex related device
       +-- 00:01.0 ordinary endpoint
       +-- 00:02.0 PCI bridge
                    |
                    v
                  bus 01
                    |
                    +-- 01:00.0 NIC
                    +-- 01:01.0 storage controller
                    +-- 01:02.0 DMA controller

The kernel walks this tree by:

    scan bus
        -> detect device
        -> if endpoint, record it
        -> if bridge, configure secondary/subordinate info and recurse into child bus

That is why scanning is not just a flat loop.
It is recursive.


WHAT pci_scan_slot() DOES
=========================
A PCI slot corresponds to a device number on a bus.
A device may have up to 8 functions.

So pci_scan_slot() conceptually means:

    inspect device number X on this bus
    determine whether function 0 exists
    if multi-function, inspect additional functions too

This is the place where the kernel decides whether a given device number on a
bus corresponds to a real PCI device/function combination.


WHAT pci_scan_device() DOES
===========================
This is a key function in the discovery path.

Conceptually it does things like:

    - read vendor ID and device ID from config space
    - if vendor == 0xffff, device is absent
    - allocate and initialize struct pci_dev
    - read header type/class/BAR-related information
    - attach device to the bus object

So:

    pci_scan_device()

is where a potential device/function becomes a real kernel PCI device object.


HOW THE KERNEL KNOWS WHETHER A PCI DEVICE EXISTS
================================================
The kernel checks PCI configuration space.

Important config-space locations include:

    offset 0x00  vendor ID
    offset 0x02  device ID
    offset 0x0E  header type

Conceptual detection logic:

    read vendor ID
    if vendor ID == 0xffff
        no device present at this function
    else
        valid PCI device exists

This is the classic PCI enumeration test.


CONFIG SPACE ACCESS IS FUNDAMENTAL
==================================
PCI scanning depends on reading PCI config space.
The specific low-level mechanism may be architecture/platform dependent, but the
conceptual flow is always:

    choose bus/device/function/register
        -> read config space
        -> interpret vendor/device/class/header type

Without config-space reads, PCI discovery cannot happen.


WHAT pci_setup_device() DOES
============================
After a device is detected, the kernel initializes software-visible information
about it.

Conceptually pci_setup_device() is responsible for extracting and recording
information such as:

    - vendor/device IDs
    - class code
    - header type
    - subsystem IDs
    - BAR layout information
    - capability lists
    - whether the device is a normal endpoint or a bridge

This is part of turning a raw config-space presence into a meaningful struct
pci_dev.


BRIDGES AND RECURSIVE SCAN
==========================
If the discovered device is a PCI-to-PCI bridge, scanning must continue
below it.

Conceptual flow:

    pci_scan_device()
        -> pci_setup_device()
        -> determine this device is a bridge
        -> pci_scan_bridge()
            -> create/scan child bus
            -> recurse into pci_scan_child_bus()

This is what lets Linux discover all buses behind bridges.

Example:

    bus 0
      +-- bridge at 00:02.0
             |
             v
           bus 1
             +-- endpoint 01:00.0
             +-- endpoint 01:01.0

Without recursive bridge scanning, only bus 0 devices would be visible.


STRUCT pci_dev CREATION
=======================
A central result of scanning is creation of:

    struct pci_dev

for each discovered function.

This object represents the PCI device inside the kernel and later becomes the
thing passed to PCI driver probe().

That means:

    scan phase produces struct pci_dev
    probe phase consumes struct pci_dev

This is an important lifecycle transition.


WHEN THE DEVICE BECOMES VISIBLE TO THE DRIVER CORE
==================================================
After scanning and setup, the device is added to the Linux device model.

A key function to look for is:

    pci_device_add()

in:

    drivers/pci/probe.c

Conceptually, this step:

    - finalizes the PCI device's integration with the generic device model
    - makes it visible in sysfs
    - allows the driver core to match it against PCI drivers

At this point, the discovered PCI device becomes a real kernel device object.


DEVICE MODEL INTEGRATION
========================
Once pci_device_add() runs, the device is effectively inserted into the generic
Linux driver model.

Conceptually this involves:

    device_add()

and related infrastructure, which then enables driver matching and sysfs
visibility.

That is why only AFTER pci_device_add() can ordinary PCI driver binding happen.


SYSFS VISIBILITY
================
After PCI devices are discovered and added, they appear under locations like:

    /sys/bus/pci/devices/

This visibility is a consequence of device-model registration.

So if a PCI device shows up in sysfs but no driver binds yet, it means:

    scan succeeded
    device object exists
    but matching/binding may still not have happened

This is a useful mental distinction.


WHEN PCI DRIVER MATCHING HAPPENS
================================
After device creation and addition to the device model, the PCI core/driver core
can match discovered devices against registered PCI drivers.

A PCI driver typically provides a device ID table like:

    static struct pci_device_id my_ids[] = {
        { PCI_DEVICE(0x8086, 0x1234) },
        { }
    };

The matching logic conceptually does:

    for each discovered pci_dev
        compare vendor/device/class info against registered pci_driver ID tables
        if match found
            call driver's probe()

So matching happens after discovery, not during raw scanning itself.


PCI DRIVER PROBE HAPPENS LATE IN THE FLOW
=========================================
Only after all of the following are true:

    - PCI subsystem initialized
    - architecture PCI setup done
    - root bus scanned
    - device discovered through config space reads
    - struct pci_dev created
    - device added to device model
    - matching pci_driver exists

will:

    pci_driver.probe(struct pci_dev *pdev, ...)

be called.

So if your DMA controller is a PCI device, its probe() is near the END of the
discovery pipeline, not the beginning.


FULL END-TO-END FLOW IN ASCII
=============================

    kernel start
        |
        v
    initcall framework
        |
        v
    subsys_initcall(pci_subsys_init)
        |
        v
    PCI core initialized
        |
        v
    architecture-specific PCI init
        |
        v
    create root bus / host bridge setup
        |
        v
    pci_scan_root_bus()
        |
        v
    pci_scan_child_bus(bus0)
        |
        +--> pci_scan_slot(dev=0)
        |       |
        |       v
        |    pci_scan_device(fn=0..7)
        |       |
        |       +--> read config space
        |       +--> if vendor != 0xffff -> create struct pci_dev
        |       +--> pci_setup_device()
        |       +--> if bridge -> pci_scan_bridge() -> recurse to child bus
        |
        +--> pci_scan_slot(dev=1)
        |
        +--> ... repeat for all slots/functions ...
        |
        v
    pci_device_add()
        |
        v
    device added to Linux device model
        |
        v
    driver core / PCI core matches pci_dev with pci_driver ID table
        |
        v
    pci_driver.probe()

This is the correct mental model.


WHY THIS MATTERS FOR A PCI DMA CONTROLLER DRIVER
================================================
Suppose your DMA controller is a PCI device.
Then its lifecycle is:

    1. PCI scan finds the hardware
    2. struct pci_dev is created
    3. device is added to kernel device model
    4. matching pci_driver is found
    5. pci_driver.probe() runs
    6. driver maps BARs / requests IRQ / initializes hardware
    7. driver registers itself as DMAEngine provider

That means PCI scanning is literally the first step in the provider driver's
lifecycle.

So the scan phase explains:

    where the pdev passed into probe() came from

Answer:

    from PCI enumeration and struct pci_dev creation during scan.


SCAN VS DRIVER INITIALIZATION FOR A PCI DMA CONTROLLER
======================================================
It helps to separate the two stages explicitly.

STAGE 1: PCI ENUMERATION
------------------------
    - kernel reads config space
    - detects device
    - creates struct pci_dev
    - adds it to device model

STAGE 2: DRIVER PROBE / INITIALIZATION
--------------------------------------
    - matching pci_driver is found
    - probe() gets struct pci_dev *pdev
    - driver enables PCI device
    - requests BAR regions
    - maps registers
    - requests IRQ
    - initializes DMA controller
    - registers provider with DMAEngine core

So if you are tracing a PCI DMA engine provider, the scan happens before your
driver file is even involved in any hardware-specific sense.


WHAT TO READ IN drivers/pci/probe.c
===================================
When you open:

    drivers/pci/probe.c

focus on these functions in order:

    pci_scan_root_bus()
    pci_scan_bus()
    pci_scan_child_bus()
    pci_scan_slot()
    pci_scan_device()
    pci_setup_device()
    pci_scan_bridge()
    pci_device_add()

If you understand these 8 functions, you understand most of PCI discovery.


WHAT TO READ IN drivers/pci/pci.c
=================================
When you open:

    drivers/pci/pci.c

focus on:

    pci_subsys_init()
    subsys_initcall(pci_subsys_init)

This tells you where in boot the PCI subsystem gets initialized.


WHAT TO READ IN arch/x86/pci/
=============================
For x86, inspect:

    arch/x86/pci/init.c
    arch/x86/pci/common.c

Here you are looking for:

    - architecture-specific PCI setup
    - root-bus / host-bridge handling
    - where the generic PCI scan gets kicked off from x86 side

These files explain how x86 connects the generic PCI core to real platform
hardware.


QUESTIONS TO ASK WHILE READING THE CODE
=======================================
Use these questions to keep the reading structured:

    1. Where is PCI core initialized during boot?
       - look for pci_subsys_init()

    2. What initcall level is used?
       - subsys_initcall?

    3. Where does x86 create or initialize root bus / host bridge state?

    4. Where does pci_scan_root_bus() get called?

    5. How does pci_scan_child_bus() iterate over slots/functions?

    6. Where does the code read PCI vendor ID?

    7. What condition means "no device present"?
       - vendor == 0xffff

    8. Where is struct pci_dev allocated?

    9. Where are device type/class/header fields filled?
       - pci_setup_device()

   10. Where does bridge recursion happen?
       - pci_scan_bridge()

   11. Where is the discovered device added to the generic device model?
       - pci_device_add()

   12. At what point can a pci_driver's probe() be called?
       - only after device-model integration and ID matching

These questions will make the scan path much easier to follow.


MENTAL MODEL OF A SINGLE DEVICE DISCOVERY
=========================================
For one potential bus/device/function location, the conceptual flow is:

    choose bus/dev/fn
        -> read vendor ID
        -> if 0xffff: nothing there
        -> else: valid function exists
        -> allocate struct pci_dev
        -> read class/header/BAR/caps
        -> attach device to bus
        -> add to Linux device model
        -> driver match may happen later

That is the smallest useful unit of PCI scan understanding.


MENTAL MODEL OF RECURSIVE PCI DISCOVERY
=======================================
For the whole topology:

    start at root bus
        -> scan all slots/functions
        -> whenever a bridge is found:
             create/identify child bus
             recursively scan child bus
        -> continue until all reachable buses are enumerated

So PCI scan is a recursive tree walk driven by config-space reads.


GREP COMMANDS TO EXPLORE THE TREE
=================================
Use these commands in your linux-2.6.39 tree:

    grep -R "pci_subsys_init" -n .
    grep -R "subsys_initcall(pci_subsys_init" -n .
    grep -R "pci_scan_root_bus" -n .
    grep -R "pci_scan_bus" -n .
    grep -R "pci_scan_child_bus" -n .
    grep -R "pci_scan_slot" -n .
    grep -R "pci_scan_device" -n .
    grep -R "pci_setup_device" -n .
    grep -R "pci_scan_bridge" -n .
    grep -R "pci_device_add" -n .

These are the most useful anchors for following the code path.


SMALL PCI TREE DIAGRAM
======================

    CPU / Host Bridge
            |
            v
          Bus 0
         /  |   \
        /   |    \
       v    v     v
    Dev0  Dev1  Bridge
                    |
                    v
                  Bus 1
                 /    \
                v      v
             Dev0    Dev1

Linux scans this structure by:

    - checking each bus
    - checking each device slot
    - checking each function
    - recursing when bridges are found


VERY IMPORTANT SUMMARY
======================
PCI scanning in Linux 2.6.39 happens in the PCI core during boot, with support
from architecture-specific PCI initialization code.

The key source file for discovery is:

    drivers/pci/probe.c

The key scan functions are:

    pci_scan_root_bus()
    pci_scan_child_bus()
    pci_scan_slot()
    pci_scan_device()
    pci_scan_bridge()
    pci_setup_device()
    pci_device_add()

And the key lifecycle distinction is:

    scan discovers devices
    probe attaches drivers to already-discovered devices

That is the single most important structural idea.


ONE-LINE MENTAL MODEL
=====================
PCI scanning creates struct pci_dev objects by recursively walking PCI buses and
reading config space, and only after those devices are added to the kernel
device model can PCI drivers match and run probe().


END-TO-END MINI SUMMARY
=======================
If you want the shortest complete story:

    PCI subsystem initializes during boot
        -> x86/arch PCI code sets up host/root bus logic
        -> pci_scan_root_bus() begins enumeration
        -> pci_scan_child_bus()/pci_scan_slot()/pci_scan_device() read config space
        -> struct pci_dev objects are created
        -> bridges trigger recursive child-bus scans
        -> pci_device_add() exposes devices to the driver model
        -> matching pci_driver.probe() finally runs

That is where the PCI scan is.

