/*
 * dma_memcpy_ioctl_demo_linux_2_6_39.c
 *
 * Educational sample: a tiny misc driver that exposes an ioctl interface and
 * uses the DMAEngine MEMCPY path to copy data from a source DMA buffer to a
 * destination DMA buffer.
 *
 * IMPORTANT LIMITATIONS
 * ---------------------
 * 1) This is a teaching/demo driver, not production-ready code.
 * 2) It requires a DMA engine channel that supports DMA_MEMCPY.
 * 3) It does NOT DMA directly from arbitrary user-space pointers to arbitrary
 *    user-space pointers. For a simple and safe demo, user data is copied into
 *    a DMA source buffer, hardware DMA copies source -> destination, then the
 *    destination data is copied back to user space.
 * 4) Real zero-copy user-page DMA is more complex: page pinning, SG mapping,
 *    cache coherency, lifetime rules, and security checks.
 * 5) DMAEngine API details vary a bit by kernel/platform. This is written in a
 *    Linux-2.6.39-style spirit and may need small adjustments for your tree.
 *
 * Build idea:
 *   obj-m += dma_memcpy_ioctl_demo.o
 *
 * User flow:
 *   - open /dev/dma_memcpy_demo
 *   - fill struct dma_memcpy_req
 *   - ioctl(fd, DMA_MEMCPY_IOCTL_RUN, &req)
 *   - req.dst[] contains the copied bytes after ioctl returns
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/ioctl.h>
#include <linux/types.h>

#define DRV_NAME            "dma_memcpy_demo"
#define DMA_DEMO_MAX_BYTES  4096

/*
 * For a small demo we embed payload arrays directly in the ioctl structure.
 * Userspace fills src[] and len, kernel DMA-copies src->dst internally, then
 * returns dst[] to userspace.
 */
struct dma_memcpy_req {
    __u32 len;
    __u8  src[DMA_DEMO_MAX_BYTES];
    __u8  dst[DMA_DEMO_MAX_BYTES];
};

#define DMA_MEMCPY_IOCTL_MAGIC   'd'
#define DMA_MEMCPY_IOCTL_RUN     _IOWR(DMA_MEMCPY_IOCTL_MAGIC, 1, struct dma_memcpy_req)

struct dma_demo_dev {
    struct miscdevice miscdev;
    struct dma_chan *chan;
    struct device *dma_dev; /* device used for DMA buffer allocation */

    void *src_cpu;
    dma_addr_t src_dma;

    void *dst_cpu;
    dma_addr_t dst_dma;

    size_t buf_size;
    struct mutex lock;
    struct completion dma_done;
    int dma_status;
};

static struct dma_demo_dev *g_demo;

static void dma_demo_complete_func(void *arg)
{
    struct dma_demo_dev *d = arg;

    d->dma_status = 0;
    complete(&d->dma_done);
}

static int dma_demo_submit_memcpy(struct dma_demo_dev *d, size_t len)
{
    struct dma_async_tx_descriptor *tx;
    dma_cookie_t cookie;
    int ret;

    if (!d || !d->chan)
        return -ENODEV;

    reinit_completion(&d->dma_done);
    d->dma_status = -EINPROGRESS;

    /*
     * DMA_MEMCPY engine operation:
     *   destination DMA address, source DMA address, length, flags
     */
    tx = d->chan->device->device_prep_dma_memcpy(
            d->chan,
            d->dst_dma,
            d->src_dma,
            len,
            DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
    if (!tx)
        return -EIO;

    tx->callback = dma_demo_complete_func;
    tx->callback_param = d;

    cookie = tx->tx_submit(tx);
    ret = dma_submit_error(cookie);
    if (ret)
        return ret;

    dma_async_issue_pending(d->chan);

    /*
     * For a tiny demo, block until completion.
     * Production code may prefer wait_event/timeout/async notification.
     */
    ret = wait_for_completion_interruptible(&d->dma_done);
    if (ret)
        return ret;

    return d->dma_status;
}

static long dma_demo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct dma_demo_dev *d = g_demo;
    struct dma_memcpy_req req;
    int ret = 0;

    if (!d)
        return -ENODEV;

    if (cmd != DMA_MEMCPY_IOCTL_RUN)
        return -ENOTTY;

    if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
        return -EFAULT;

    if (req.len == 0 || req.len > DMA_DEMO_MAX_BYTES)
        return -EINVAL;

    mutex_lock(&d->lock);

    /*
     * Stage source bytes into DMA buffer.
     * This is CPU copy user->kernel, then hardware DMA kernel_src->kernel_dst.
     */
    memset(d->src_cpu, 0, d->buf_size);
    memset(d->dst_cpu, 0, d->buf_size);
    memcpy(d->src_cpu, req.src, req.len);

    /*
     * Buffers were allocated with dma_alloc_coherent(), so CPU/device share a
     * coherent view and no explicit dma_sync_* is needed for this simple x86-ish
     * demo path.
     */
    ret = dma_demo_submit_memcpy(d, req.len);
    if (ret)
        goto out_unlock;

    memcpy(req.dst, d->dst_cpu, req.len);

    if (copy_to_user((void __user *)arg, &req, sizeof(req))) {
        ret = -EFAULT;
        goto out_unlock;
    }

out_unlock:
    mutex_unlock(&d->lock);
    return ret;
}

static int dma_demo_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int dma_demo_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations dma_demo_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = dma_demo_ioctl,
    .open           = dma_demo_open,
    .release        = dma_demo_release,
};

static int dma_demo_alloc_buffers(struct dma_demo_dev *d)
{
    d->src_cpu = dma_alloc_coherent(d->dma_dev,
                                    d->buf_size,
                                    &d->src_dma,
                                    GFP_KERNEL);
    if (!d->src_cpu)
        return -ENOMEM;

    d->dst_cpu = dma_alloc_coherent(d->dma_dev,
                                    d->buf_size,
                                    &d->dst_dma,
                                    GFP_KERNEL);
    if (!d->dst_cpu)
        return -ENOMEM;

    return 0;
}

static void dma_demo_free_buffers(struct dma_demo_dev *d)
{
    if (!d)
        return;

    if (d->src_cpu)
        dma_free_coherent(d->dma_dev, d->buf_size, d->src_cpu, d->src_dma);

    if (d->dst_cpu)
        dma_free_coherent(d->dma_dev, d->buf_size, d->dst_cpu, d->dst_dma);

    d->src_cpu = NULL;
    d->dst_cpu = NULL;
}

static int __init dma_demo_init(void)
{
    struct dma_demo_dev *d;
    dma_cap_mask_t mask;
    int ret;

    d = kzalloc(sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;

    d->buf_size = DMA_DEMO_MAX_BYTES;
    mutex_init(&d->lock);
    init_completion(&d->dma_done);

    /*
     * Request a channel capable of memory-to-memory copy.
     * This only succeeds if the platform has a DMA engine provider exposing
     * DMA_MEMCPY capability.
     */
    dma_cap_zero(mask);
    dma_cap_set(DMA_MEMCPY, mask);

    d->chan = dma_request_channel(mask, NULL, NULL);
    if (!d->chan) {
        pr_err(DRV_NAME ": no DMA_MEMCPY channel available\n");
        ret = -ENODEV;
        goto err_free_dev;
    }

    d->dma_dev = d->chan->device->dev;
    if (!d->dma_dev) {
        pr_err(DRV_NAME ": DMA channel has no backing device\n");
        ret = -ENODEV;
        goto err_release_chan;
    }

    ret = dma_demo_alloc_buffers(d);
    if (ret)
        goto err_release_chan;

    d->miscdev.minor = MISC_DYNAMIC_MINOR;
    d->miscdev.name  = DRV_NAME;
    d->miscdev.fops  = &dma_demo_fops;

    ret = misc_register(&d->miscdev);
    if (ret)
        goto err_free_bufs;

    g_demo = d;

    pr_info(DRV_NAME ": loaded\n");
    pr_info(DRV_NAME ": channel=%s src_dma=%pad dst_dma=%pad\n",
            dma_chan_name(d->chan), &d->src_dma, &d->dst_dma);
    return 0;

err_free_bufs:
    dma_demo_free_buffers(d);
err_release_chan:
    dma_release_channel(d->chan);
err_free_dev:
    kfree(d);
    return ret;
}

static void __exit dma_demo_exit(void)
{
    struct dma_demo_dev *d = g_demo;

    if (!d)
        return;

    misc_deregister(&d->miscdev);
    dma_demo_free_buffers(d);

    if (d->chan)
        dma_release_channel(d->chan);

    kfree(d);
    g_demo = NULL;

    pr_info(DRV_NAME ": unloaded\n");
}

module_init(dma_demo_init);
module_exit(dma_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI");
MODULE_DESCRIPTION("Educational ioctl + DMAEngine memcpy demo driver");

/*
 * -----------------------------------------------------------------------------
 * Optional tiny userspace test program
 * -----------------------------------------------------------------------------
 *
 * Save separately as: test_dma_memcpy_demo.c
 *
 * #include <stdio.h>
 * #include <string.h>
 * #include <fcntl.h>
 * #include <unistd.h>
 * #include <sys/ioctl.h>
 * #include <stdint.h>
 *
 * #define DMA_DEMO_MAX_BYTES 4096
 *
 * struct dma_memcpy_req {
 *     uint32_t len;
 *     uint8_t  src[DMA_DEMO_MAX_BYTES];
 *     uint8_t  dst[DMA_DEMO_MAX_BYTES];
 * };
 *
 * #define DMA_MEMCPY_IOCTL_MAGIC   'd'
 * #define DMA_MEMCPY_IOCTL_RUN     _IOWR(DMA_MEMCPY_IOCTL_MAGIC, 1, struct dma_memcpy_req)
 *
 * int main(void)
 * {
 *     int fd;
 *     struct dma_memcpy_req req;
 *
 *     memset(&req, 0, sizeof(req));
 *     req.len = 24;
 *     memcpy(req.src, "hello-from-dma-demo-123", req.len);
 *
 *     fd = open("/dev/dma_memcpy_demo", O_RDWR);
 *     if (fd < 0) {
 *         perror("open");
 *         return 1;
 *     }
 *
 *     if (ioctl(fd, DMA_MEMCPY_IOCTL_RUN, &req) < 0) {
 *         perror("ioctl");
 *         close(fd);
 *         return 1;
 *     }
 *
 *     printf("src: %.*s\n", req.len, req.src);
 *     printf("dst: %.*s\n", req.len, req.dst);
 *
 *     close(fd);
 *     return 0;
 * }
 *
 * -----------------------------------------------------------------------------
 * Notes for extending this demo
 * -----------------------------------------------------------------------------
 *
 * 1) If you want userspace to provide source and destination addresses rather
 *    than embedded arrays, do NOT directly treat those addresses as DMA-able.
 *    You would need a more advanced design:
 *       - pin user pages
 *       - build scatter-gather lists
 *       - map them for DMA
 *       - handle page lifetime and dirtying
 *       - deal with alignment, cache coherence, and permissions
 *
 * 2) If your machine has no MEMCPY-capable DMA engine, the driver will fail at
 *    module load with "no DMA_MEMCPY channel available".
 *
 * 3) For a pure software fallback, you could add an option to use memcpy() when
 *    no DMA engine exists, but that would defeat the main learning goal here.
 */

