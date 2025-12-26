# Shared memory (ONE vring)
┌───────────────────────────────────────────────────────────────────────────┐
│                                                                           │
│  Descriptor Table (desc[])  [ARRAY, indexed]                              │
│                                                                           │
│  index                                                                    │
│   0 ┌───────────────────────────────┐                                     │
│     │ addr = buf0                   │                                     │
│     │ len  = ...                    │                                     │
│     │ flags = NEXT                  │                                     │
│     │ next  = 1                     │                                     │
│     └───────────────────────────────┘                                     │
│                                                                           │
│   1 ┌───────────────────────────────┐                                     │
│     │ addr = buf1                   │                                     │
│     │ len  = ...                    │                                     │
│     │ flags = NEXT                  │                                     │
│     │ next  = 2                     │                                     │
│     └───────────────────────────────┘                                     │
│                                                                           │
│   2 ┌───────────────────────────────┐                                     │
│     │ addr = buf2                   │                                     │
│     │ len  = ...                    │                                     │
│     │ flags = 0   (END)             │                                     │
│     │ next  = X                     │                                     │
│     └───────────────────────────────┘                                     │
│                                                                           │
│   3 ┌───────────────────────────────┐   ◄──── HEAD DESCRIPTOR             │
│     │ addr = buf3                   │                                     │
│     │ len  = ...                    │                                     │
│     │ flags = NEXT                  │                                     │
│     │ next  = 4                     │                                     │
│     └───────────────────────────────┘                                     │
│                                                                           │
│   4 ┌───────────────────────────────┐                                     │
│     │ addr = buf4                   │                                     │
│     │ len  = ...                    │                                     │
│     │ flags = 0   (END)             │                                     │
│     │ next  = X                     │                                     │
│     └───────────────────────────────┘                                     │
│                                                                           │
├───────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  Available Ring (avail)   [CIRCULAR RING, driver → device]                │
│                                                                           │
│    avail.flags = 0                                                        │
│    avail.idx   = 5   (monotonic)                                          │
│                                                                           │
│    slot = idx % num                                                       │
│                                                                           │
│    ring slots                                                             │
│    ┌────┬────┬────┬────┬────┬────┬────┬────┐                              │
│    │ 1  │ 6  │ 3  │ 2  │ 7  │    │    │    │                              │
│    └────┴────┴────┴────┴────┴────┴────┴────┘                              │
│                ▲                                                          │
│                │                                                          │
│                └── descriptor HEAD index (3)                              │
│                                                                           │
├───────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  Used Ring (used)   [CIRCULAR RING, device → driver]                      │
│                                                                           │
│    used.flags = 0                                                         │
│    used.idx   = 2                                                         │
│                                                                           │
│    ring slots                                                             │
│    ┌────────────────┬────────────────┬──────┬──────┬──────┬──────┐        │
│    │ id=1 len=128   │ id=6 len=256   │      │      │      │      │        │
│    └────────────────┴────────────────┴──────┴──────┴──────┴──────┘        │
│                                                                           │
└───────────────────────────────────────────────────────────────────────────┘


# ONE virtqueue → ONE vring → THREE structures
LEGEND
------
F = free descriptor (driver owns)
U = in-flight / available to device
C = consumed, waiting reclaim
_ = unused ring slot
→ = pointer / flow


=========================
PHASE 0 — INITIAL STATE
=========================
Counters
--------
avail.idx = 0
used.idx  = 0


Descriptor Table (desc[])
-------------------------
index:   0   1   2   3   4   5   6   7
        [F] [F] [F] [F] [F] [F] [F] [F]


Available Ring  (driver → device)
---------------------------------
slots:  [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ]
idx = 0


Used Ring (device → driver)
---------------------------
slots:  [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ]
idx = 0


=====================================
PHASE 1 — DRIVER ADDS ONE BUFFER
=====================================
(two descriptors: desc[0] → desc[1])
Driver actions
--------------
alloc desc[0], desc[1]
fill buffers
publish head = 0 to avail ring


Descriptor chaining   <<====]]]]
-------------------
desc[0] ──next──▶ desc[1] ──END


Descriptor Table
----------------
index:   0   1   2   3   4   5   6   7
        [U] [U] [F] [F] [F] [F] [F] [F]
         ↑   ↑
         └───┴── owned by DEVICE (in flight)


Available Ring
--------------
slot = avail.idx % 8 = 0

slots:  [ 0 ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ]
idx = 1


Used Ring
---------
slots:  [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ]
idx = 0


Ownership transfer:
DRIVER ──publish──▶ DEVICE


=====================================
PHASE 2 — DEVICE CONSUMES BUFFER
=====================================
Device actions
--------------
read avail ring
head = avail.ring[0] = 0
walk desc[0] → desc[1]
complete I/O
publish completion to used ring


Descriptor Table
----------------
index:   0   1   2   3   4   5   6   7
        [C] [C] [F] [F] [F] [F] [F] [F]
         ↑   ↑
         └───┴── completed, waiting reclaim


Used Ring
---------
slot = used.idx % 8 = 0

slots:  [ id=0,len=640 ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ]
idx = 1


Available Ring (unchanged)
--------------------------
slots:  [ 0 ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ]
idx = 1


Ownership state:
DEVICE ──done──▶ waiting for DRIVER reclaim


=====================================
PHASE 3 — DRIVER RECLAIMS BUFFER
=====================================
Driver actions
--------------
virtqueue_get_buf()
read used ring
head = used.ring[0].id = 0
walk desc[0] → desc[1]
free descriptors


Descriptor Table (after free)
-----------------------------
index:   0   1   2   3   4   5   6   7
        [F] [F] [F] [F] [F] [F] [F] [F]


Available Ring (unchanged, ignored)
-----------------------------------
slots:  [ 0 ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ]
idx = 1


Used Ring (unchanged, ignored)
------------------------------
slots:  [ 0 ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ] [ _ ]
idx = 1


NOTE:Ring slots still contain old values —
    they are ignored because idx moved forward.


Ownership restored:
    DRIVER ◀──reclaim── DEVICE



=========================
LIFETIME FLOW (ONE LINE)
=========================
FREE (driver)
   ↓ virtqueue_add_buf
AVAILABLE (avail ring)
   ↓ device consumes
CONSUMED (used ring)
   ↓ virtqueue_get_buf
FREE (driver again)


=========================
KEY INVARIANTS (CRITICAL)
=========================
avail.idx  → written only by DRIVER
used.idx   → written only by DEVICE

slot index = idx % num

Outstanding buffers = avail.idx - used.idx

avail.idx + used.idx ≠ num        (this is CORRECT)


=========================
ONE-LINE MENTAL MODEL
=========================
avail ring says WHAT desc[] heads are ready,
desc[].next says HOW MANY buffers belong to it,
used ring says WHICH head is finished.

