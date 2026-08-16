STEP #1. run gdb
run-linux-2.6 ❱❱❱ cd linux-2.6.39/
linux-2.6.39 ❱❱❱ gdb vmlinux
GNU gdb (Ubuntu 15.0.50.20240403-0ubuntu1) 15.0.50.20240403-git
Copyright (C) 2024 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "x86_64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<https://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from vmlinux...
(gdb) target remote :1234
Remote debugging using :1234
0x0000fff0 in ?? ()
(gdb) bt
#0  0x0000fff0 in ?? ()
(gdb) b start_kernel
Breakpoint 11 at 0xc1722417: file init/main.c, line 457.
(gdb) b state_store
(gdb) b hibernate
(gdb) b hibernation_snapshot
(gdb) b software_resume
(gdb) c
Continuing.


Step #2. Going to hybernate
Program received signal SIGINT, Interrupt.
Enable debuginfod for this session? (y or [n]) y
Downloading source file /labs/linux-2.6.39/arch/x86/include/asm/irqflags.h
[                       ###                                                                             ]^CCancelling download of source file /labs/linux-2.6.39/arch/x86/include/asm/irqflags.h...
0xc1006ea8 in native_safe_halt () at /labs/linux-2.6.39/arch/x86/include/asm/irqflags.h:49

This GDB supports auto-downloading debuginfo from the following URLs:
  <https://debuginfod.ubuntu.com>
Debuginfod has been enabled.
To make this setting permanent, add 'set debuginfod enabled on' to .gdbinit.
warning: 49	/labs/linux-2.6.39/arch/x86/include/asm/irqflags.h: No such file or directory
(gdb) bt
#0  0xc1006ea8 in native_safe_halt () at /labs/linux-2.6.39/arch/x86/include/asm/irqflags.h:49
#1  arch_safe_halt () at /labs/linux-2.6.39/arch/x86/include/asm/irqflags.h:90
#2  default_idle () at arch/x86/kernel/process.c:388
#3  0xc1001581 in cpu_idle () at arch/x86/kernel/process_32.c:112
#4  0xc147e19e in rest_init () at init/main.c:375
#5  0xc172271e in start_kernel () at init/main.c:626
#6  0xc17220ba in i386_start_kernel () at arch/x86/kernel/head32.c:69
#7  0x00000000 in ?? ()
(gdb)



Breakpoint 2.1, state_store (kobj=0xcf42fd90, attr=0xc16a7f20 <state_attr>, buf=0xce428000 "disk\n", n=5)
at kernel/power/main.c:171
171	{
(gdb) bt
#0  state_store (kobj=0xcf42fd90, attr=0xc16a7f20 <state_attr>, buf=0xce428000 "disk\n", n=5)
    at kernel/power/main.c:171
#1  0xc1183f94 in kobj_attr_store (kobj=<optimized out>, attr=<optimized out>, buf=<optimized out>,
    count=5) at lib/kobject.c:699
#2  0xc10f5d13 in flush_write_buffer (dentry=<optimized out>, count=<optimized out>, buffer=0xce40ad90)
    at fs/sysfs/file.c:209
#3  sysfs_write_file (file=file@entry=0xcf461900, buf=buf@entry=0x8245208 "disk\n\346#\b",
    count=<optimized out>, ppos=0xcf463f9c) at fs/sysfs/file.c:243
#4  0xc10b3fcf in vfs_write (file=file@entry=0xcf461900, buf=buf@entry=0x8245208 "disk\n\346#\b",
    count=<optimized out>, count@entry=5, pos=0xcf463f9c) at fs/read_write.c:377
#5  0xc10b42c3 in sys_write (fd=1, buf=0x8245208 "disk\n\346#\b", count=5) at fs/read_write.c:429
#6  <signal handler called>
#7  0xffffe424 in ?? ()
#8  0x0811c04d in ?? ()
Backtrace stopped: previous frame inner to this frame (corrupt stack?)
(gdb)



Breakpoint 3, hibernate () at kernel/power/hibernate.c:611
611	{
(gdb) bt
#0  hibernate () at kernel/power/hibernate.c:611
#1  0xc1056bef in state_store (kobj=<optimized out>, attr=<optimized out>, buf=0xce428000 "disk\n", n=5)
    at kernel/power/main.c:185
#2  0xc1183f94 in kobj_attr_store (kobj=<optimized out>, attr=<optimized out>, buf=<optimized out>,
    count=5) at lib/kobject.c:699
#3  0xc10f5d13 in flush_write_buffer (dentry=<optimized out>, count=<optimized out>, buffer=0xce40ad90)
    at fs/sysfs/file.c:209
#4  sysfs_write_file (file=file@entry=0xcf461900, buf=buf@entry=0x8245208 "disk\n\346#\b",
    count=<optimized out>, ppos=0xcf463f9c) at fs/sysfs/file.c:243
#5  0xc10b3fcf in vfs_write (file=file@entry=0xcf461900, buf=buf@entry=0x8245208 "disk\n\346#\b",
    count=<optimized out>, count@entry=5, pos=0xcf463f9c) at fs/read_write.c:377
#6  0xc10b42c3 in sys_write (fd=1, buf=0x8245208 "disk\n\346#\b", count=5) at fs/read_write.c:429
#7  <signal handler called>
#8  0xffffe424 in ?? ()
#9  0x0811c04d in ?? ()
Backtrace stopped: previous frame inner to this frame (corrupt stack?)
(gdb)



(gdb) bt
#0  pm_notifier_call_chain (val=val@entry=1) at kernel/power/main.c:39
#1  0xc1057e81 in hibernate () at kernel/power/hibernate.c:622
#2  0xc1056bef in state_store (kobj=<optimized out>, attr=<optimized out>, buf=0xce428000 "disk\n", n=5)
    at kernel/power/main.c:185
#3  0xc1183f94 in kobj_attr_store (kobj=<optimized out>, attr=<optimized out>, buf=<optimized out>,
    count=5) at lib/kobject.c:699
#4  0xc10f5d13 in flush_write_buffer (dentry=<optimized out>, count=<optimized out>, buffer=0xce40ad90)
    at fs/sysfs/file.c:209
#5  sysfs_write_file (file=file@entry=0xcf461900, buf=buf@entry=0x8245208 "disk\n\346#\b",
    count=<optimized out>, ppos=0xcf463f9c) at fs/sysfs/file.c:243
#6  0xc10b3fcf in vfs_write (file=file@entry=0xcf461900, buf=buf@entry=0x8245208 "disk\n\346#\b",
    count=<optimized out>, count@entry=5, pos=0xcf463f9c) at fs/read_write.c:377
#7  0xc10b42c3 in sys_write (fd=1, buf=0x8245208 "disk\n\346#\b", count=5) at fs/read_write.c:429
#8  <signal handler called>
#9  0xffffe424 in ?? ()
#10 0x0811c04d in ?? ()
Backtrace stopped: previous frame inner to this frame (corrupt stack?)
(gdb)





(gdb) bt
#0  create_basic_memory_bitmaps () at kernel/power/snapshot.c:732
#1  0xc1057e97 in hibernate () at kernel/power/hibernate.c:631
#2  0xc1056bef in state_store (kobj=<optimized out>, attr=<optimized out>, buf=0xce428000 "disk\n", n=5)
    at kernel/power/main.c:185
#3  0xc1183f94 in kobj_attr_store (kobj=<optimized out>, attr=<optimized out>, buf=<optimized out>,
    count=5) at lib/kobject.c:699
#4  0xc10f5d13 in flush_write_buffer (dentry=<optimized out>, count=<optimized out>, buffer=0xce40ad90)
    at fs/sysfs/file.c:209
#5  sysfs_write_file (file=file@entry=0xcf461900, buf=buf@entry=0x8245208 "disk\n\346#\b",
    count=<optimized out>, ppos=0xcf463f9c) at fs/sysfs/file.c:243
#6  0xc10b3fcf in vfs_write (file=file@entry=0xcf461900, buf=buf@entry=0x8245208 "disk\n\346#\b",
    count=<optimized out>, count@entry=5, pos=0xcf463f9c) at fs/read_write.c:377
#7  0xc10b42c3 in sys_write (fd=1, buf=0x8245208 "disk\n\346#\b", count=5) at fs/read_write.c:429
#8  <signal handler called>
#9  0xffffe424 in ?? ()
#10 0x0811c04d in ?? ()
Backtrace stopped: previous frame inner to this frame (corrupt stack?)
(gdb)














Step #2. restoring hybernate state after the boot:
Breakpoint 5, software_resume () at kernel/power/hibernate.c:707
707			return 0;
(gdb) bt
#0  software_resume () at kernel/power/hibernate.c:707
#1  0xc100112a in do_one_initcall_debug (fn=0xc1057b27 <software_resume>) at init/main.c:653
#2  do_one_initcall (fn=0xc1057b27 <software_resume>) at init/main.c:669
#3  0xc17227df in do_initcalls () at init/main.c:701
#4  do_basic_setup () at init/main.c:719
#5  kernel_init (unused=<optimized out>) at init/main.c:802
#6  0xc14a8996 in kernel_thread_helper () at arch/x86/kernel/entry_32.S:1011
#7  0x00000000 in ?? ()
(gdb)


