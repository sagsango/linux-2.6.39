# virtio-pci
virtio_device  = WHAT the device is
virtio_pci_device = HOW the device is connected
──────────────────────────────────────────────
                Guest Kernel
──────────────────────────────────────────────

          +---------------------------+
          |   virtio_blk / net / rng  |
          |   (virtio drivers)        |
          +-------------▲-------------+
                        |
                        | uses
                        |
          +-------------+-------------+
          |     struct virtio_device  |
          |  - id                     |
          |  - features               |
          |  - vqs                    |
          |  - config ops             |
          +-------------▲-------------+
                        |
                        | embedded in
                        |
          +-------------+-------------+
          | struct virtio_pci_device  |
          |  - pci_dev                |
          |  - BAR mapping            |
          |  - MSI-X / INTx           |
          |  - IRQ routing            |
          +-------------▲-------------+
                        |
                        | PCI transport
                        |
          +-------------+-------------+
          |     PCI subsystem          |
          +---------------------------+

