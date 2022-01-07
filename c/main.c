#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>

#include <rtl-sdr.h>


#define MS_DEF_RATE 2000000
#define MS_MAX_GAIN 999999
#define MS_AUTO_GAIN -100
#define MS_FREQ 1090000000
#define MS_WIDTH 1000
#define MS_HEIGHT 700
#define MS_ASYNC_BUF_NUM 12
#define MS_DATA_LEN (16*16384)

#define MS_PREAMBLE_US 8
#define MS_LONG_MSG_BITS 112
#define MS_SHORT_MSG_BITS 56
#define MS_FULL_LEN (MS_PREAMBLE_US + MS_LONG_MSG_BITS)
#define MS_LONG_MSG_BYTES (MS_LONG_MSG_BITS / 8)
#define MS_SHORT_MSG_BYTES (MS_SHORT_MSG_BYTES / 8)
#define MS_FULL_DATA_LEN MS_DATA_LEN + (MS_FULL_LEN - 1) * 4

#define UNUSED(v) ((void) v)

struct {
    pthread_t reader_thread;
    pthread_mutex_t data_mutex;
    pthread_cond_t data_cond;
    unsigned char *data;
    int data_ready;
    uint16_t *magnitude;
    uint32_t data_len;
    uint16_t *maglut;
    
    int dev_index;
    int gain;
    int enable_agc;
    rtlsdr_dev_t *dev;
    int freq;

} rtl;

void msinit(void) {
    pthread_mutex_init(&rtl.data_mutex, NULL);
    pthread_cond_init(&rtl.data_cond, NULL);

    rtl.dev_index = 0;
    rtl.gain = MS_MAX_GAIN;
    rtl.enable_agc = 0;
    rtl.freq = MS_FREQ;

    rtl.data_len = MS_FULL_DATA_LEN;
    rtl.data_ready = 0;

    if((rtl.data = malloc(rtl.data_len)) == NULL ||
       (rtl.magnitude = malloc(rtl.data_len * 2)) == NULL) {
        printf("Out of memory allocating buffers.\n");
        exit(1);
    }
    memset(rtl.data, 127, rtl.data_len);

    rtl.maglut = malloc(129 * 129 * 2);
    for(int i = 0; i <= 128; i++) {
        for(int q = 0; q <= 128; q++) {
            rtl.maglut[i * 129 + q] = round(sqrt(i * i + q * q) * 360);
        }
    }
}

void msinitrtlsdr(void) {
    int j;
    int devcount;
    int ppmerr = 0;
    char vendor[256], product[256], serial[256];

    devcount = rtlsdr_get_device_count();
    if(!devcount) {
        printf("No devices found.\n");
        exit(1);
    }

    printf("Found %d device(s):\n", devcount);
    for(j = 0; j < devcount; j++) {
        rtlsdr_get_usb_strings(rtl.dev, vendor, product, serial);
        printf("%d: %s, %s, SN: %s\n", j, vendor, product, serial);
    }

    if(rtlsdr_open(&rtl.dev, rtl.dev_index) < 0) {
        printf("Failed to open device.\n");
        exit(1);
    }

    rtlsdr_set_tuner_gain_mode(rtl.dev, (rtl.gain == MS_AUTO_GAIN) ? 0 : 1);
    if(rtl.gain != MS_AUTO_GAIN) {
        if(rtl.gain == MS_MAX_GAIN) {
            int numgains;
            int gains[100];

            numgains = rtlsdr_get_tuner_gains(rtl.dev, gains);
            rtl.gain = gains[numgains - 1];
            printf("Max available gain is %.2f\n", rtl.gain/10.0);
        }
        rtlsdr_set_tuner_gain(rtl.dev, rtl.gain);
    } else {
        printf("Using automatic gain control\n");
    }

    rtlsdr_set_freq_correction(rtl.dev, ppmerr);
    if(rtl.enable_agc) rtlsdr_set_agc_mode(rtl.dev, 1);
    rtlsdr_set_center_freq(rtl.dev, rtl.freq);
    rtlsdr_set_sample_rate(rtl.dev, MS_DEF_RATE);
    rtlsdr_reset_buffer(rtl.dev);
    printf("Gain reported by device: %.2f\n", rtlsdr_get_tuner_gain(rtl.dev)/10.0);
}

void rtlsdrcb(unsigned char *buf, uint32_t len, void *ctx) {
    UNUSED(ctx);

    pthread_mutex_lock(&rtl.data_mutex);
    if(len > MS_DATA_LEN) len = MS_DATA_LEN;

    memcpy(rtl.data, rtl.data + MS_DATA_LEN, (MS_FULL_LEN - 1) * 4);

    memcpy(rtl.data + (MS_FULL_LEN - 1) * 4, buf, len);
    rtl.data_ready = 1;

    pthread_cond_signal(&rtl.data_cond);
    pthread_mutex_unlock(&rtl.data_mutex);
}

void *rtentryp(void *arg) {
    UNUSED(arg);

    rtlsdr_read_async(rtl.dev, rtlsdrcb, NULL, MS_ASYNC_BUF_NUM, MS_DATA_LEN);

    return NULL;
}

void cmpmv(void) {
    uint16_t *m = rtl.magnitude;
    unsigned char *p = rtl.data;

    for(uint32_t j = 0; j < rtl.data_len; j += 2) {
        int i = p[j] - 127;
        int q = p[j + 1] - 127;

        if(i < 0) i = -i;
        if(q < 0) q = -q;
        m[j / 2] = rtl.maglut[i * 129 + q];
    }
}

void start(void) {
    msinit();
    msinitrtlsdr();

    pthread_create(&rtl.reader_thread, NULL, rtentryp, NULL);
    pthread_mutex_lock(&rtl.data_mutex);
}

int threadready(void) {
    if(!rtl.data_ready) {
        pthread_cond_wait(&rtl.data_cond, &rtl.data_mutex);
        return 0;
    }

    return 1;
}

void premsprocess() {
    cmpmv();

    rtl.data_ready = 0;
    pthread_cond_signal(&rtl.data_cond);

    pthread_mutex_unlock(&rtl.data_mutex);
}

uint16_t *getmagd() {
    return rtl.magnitude;
}

void postmsprocess() {
    pthread_mutex_lock(&rtl.data_mutex);
}