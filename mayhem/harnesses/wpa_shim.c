// Host stand-ins for the wpa_supplicant OS layer (port/os_xtensa.c needs the esp_random/mbedtls drivers).
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "os.h"

const wifi_osi_funcs_t *wifi_funcs = NULL;

int os_get_time(struct os_time *t)
{
    struct timeval tv;
    int ret = gettimeofday(&tv, NULL);
    t->sec = (os_time_t)tv.tv_sec;
    t->usec = tv.tv_usec;
    return ret;
}

unsigned long os_random(void)
{
    return (unsigned long)random();
}

int os_get_random(unsigned char *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        buf[i] = (unsigned char)random();
    }
    return 0;
}

void os_sleep(os_time_t sec, os_time_t usec)
{
    (void)sec;
    (void)usec;
}
