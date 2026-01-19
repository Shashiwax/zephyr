/* src/main.c */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define I2S_NODE DT_ALIAS(i2s_tx)

#if !DT_NODE_HAS_STATUS(I2S_NODE, okay)
#error "i2s_tx alias is missing or disabled in devicetree"
#endif

static const struct device *const i2s_dev = DEVICE_DT_GET(I2S_NODE);

#define SAMPLE_RATE_HZ  48000
#define WORD_SIZE_BITS  16
#define CHANNELS        2

#define FRAMES_PER_BLOCK 256
#define BLOCK_BYTES (FRAMES_PER_BLOCK * CHANNELS * (WORD_SIZE_BITS / 8))
#define NUM_BLOCKS  4

K_MEM_SLAB_DEFINE(tx_slab, BLOCK_BYTES, NUM_BLOCKS, 4);

static void fill_square(int16_t *buf, size_t frames, int16_t amp)
{
    const int toggle = 48; /* ~1kHz @ 48kHz */
    int state = 1;

    for (size_t i = 0; i < frames; i++) {
        if ((i % toggle) == 0) {
            state = -state;
        }

        int16_t s = (int16_t)(state * amp);
        buf[2 * i + 0] = s;
        buf[2 * i + 1] = s;
    }
}

int main(void)
{
    if (!device_is_ready(i2s_dev)) {
        printk("I2S device not ready\n");
        return 0;
    }

    struct i2s_config cfg = {0};

    cfg.word_size = WORD_SIZE_BITS;
    cfg.channels = CHANNELS;
    cfg.format = I2S_FMT_DATA_FORMAT_I2S;
    cfg.options = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER;
    cfg.frame_clk_freq = SAMPLE_RATE_HZ;
    cfg.mem_slab = &tx_slab;
    cfg.block_size = BLOCK_BYTES;
    cfg.timeout = 2000;

    int ret = i2s_configure(i2s_dev, I2S_DIR_TX, &cfg);
    if (ret) {
        printk("i2s_configure failed: %d\n", ret);
        return 0;
    }

    /* Queue 2 blocks before START */
    for (int i = 0; i < 2; i++) {
        void *block = NULL;
        ret = k_mem_slab_alloc(&tx_slab, &block, K_MSEC(1000));
        if (ret) {
            printk("slab alloc failed: %d\n", ret);
            return 0;
        }
        memset(block, 0, BLOCK_BYTES);
        fill_square((int16_t *)block, FRAMES_PER_BLOCK, 12000);
        ret = i2s_write(i2s_dev, block, BLOCK_BYTES);
        if (ret) {
            printk("i2s_write failed: %d\n", ret);
            return 0;
        }
    }

    ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
    if (ret) {
        printk("i2s_trigger START failed: %d\n", ret);
        return 0;
    }

    /* Keep feeding blocks */
    for (int n = 0; n < 50; n++) {
        void *block = NULL;

        ret = k_mem_slab_alloc(&tx_slab, &block, K_MSEC(1000));
        if (ret) {
            printk("alloc failed: %d\n", ret);
            break;
        }

        memset(block, 0, BLOCK_BYTES);
        fill_square((int16_t *)block, FRAMES_PER_BLOCK, 12000);

        ret = i2s_write(i2s_dev, block, BLOCK_BYTES);
        if (ret) {
            printk("write failed: %d\n", ret);
            break;
        }
    }

    (void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_STOP);
    printk("Done\n");
    return 0;
}
