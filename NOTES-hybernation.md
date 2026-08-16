# save path
╔══════════════════════════════════════════════════════════════╗
║                         SAVE PATH                            ║
╚══════════════════════════════════════════════════════════════╝

 userspace
    |
    | echo disk > /sys/power/state
    |
    v
┌──────────────────────────────┐
│ kernel/power/main.c          │
│                              │
│ state_store()                │
└──────────────┬───────────────┘
               |
               | state == PM_SUSPEND_DISK
               v
┌──────────────────────────────┐
│ kernel/power/hibernate.c     │
│                              │
│ hibernate()                  │
└──────────────┬───────────────┘
               |
               v
┌──────────────────────────────┐
│ kernel/power/process.c       │
│                              │
│ freeze_processes()           │
│ freeze_kernel_threads()      │
└──────────────┬───────────────┘
               |
               v
       USERSpace FROZEN
               |
               v
┌──────────────────────────────┐
│ drivers/base/power/main.c    │
│                              │
│ Device PM callbacks          │
│                              │
│ prepare                      │
│ suspend                      │
│ late suspend                 │
│ hibernate                    │
│ late hibernate               │
└──────────────┬───────────────┘
               |
               v
┌──────────────────────────────┐
│ kernel/power/snapshot.c      │
│                              │
│ Memory snapshot              │
│ Memory bitmaps               │
│ Page selection               │
│ Snapshot pages               │
└──────────────┬───────────────┘
               |
               v
          RAM IMAGE
               |
               v
┌──────────────────────────────┐
│ kernel/power/swap.c          │
│                              │
│ swsusp_write()               │
└──────────────┬───────────────┘
               |
               v
       /dev/sdb (8:16)
               |
               v
      HIBERNATION IMAGE
               |
               v
┌──────────────────────────────┐
│ kernel/power/hibernate.c     │
│                              │
│ power_down()                 │
└──────────────┬───────────────┘
               |
               v
        ACPI S4 / POWER OFFo





# restore path

╔══════════════════════════════════════════════════════════════╗
║                        RESTORE PATH                          ║
╚══════════════════════════════════════════════════════════════╝

                       POWER ON
                          |
                          v
                    NEW KERNEL
                          |
                          v
                     initcalls
                          |
                          v
┌──────────────────────────────┐
│ kernel/power/hibernate.c     │
│                              │
│ software_resume()            │
└──────────────┬───────────────┘
               |
               v
┌──────────────────────────────┐
│ kernel/power/swap.c          │
│                              │
│ swsusp_check()               │
└──────────────┬───────────────┘
               |
               v
        /dev/sdb (8:16)
               |
               v
        Read image header
               |
               v
        Check signature
               |
          ┌────┴────┐
          │         │
         NO        YES
          │         │
          v         v
     Normal boot  RESUME
                    |
                    v
┌──────────────────────────────┐
│ kernel/power/process.c       │
│                              │
│ freeze_processes()           │
└──────────────┬───────────────┘
               |
               v
┌──────────────────────────────┐
│ kernel/power/swap.c          │
│                              │
│ swsusp_read()                │
└──────────────┬───────────────┘
               |
               v
       Read image pages
               |
               v
       Decompress image
               |
               v
┌──────────────────────────────┐
│ kernel/power/snapshot.c      │
│                              │
│ Restore snapshot             │
│ Restore physical pages       │
│ Restore memory state         │
└──────────────┬───────────────┘
               |
               v
┌──────────────────────────────┐
│ kernel/power/hibernate.c     │
│                              │
│ load_image_and_restore()     │
└──────────────┬───────────────┘
               |
               v
┌──────────────────────────────┐
│ arch/x86/...                 │
│                              │
│ swsusp_arch_resume()         │
└──────────────┬───────────────┘
               |
               v
        RESTORE CPU STATE
               |
               v
        RESTORE OLD RAM
               |
               v
       RESTORE OLD KERNEL
               |
               v
     OLD KERNEL CONTINUES
