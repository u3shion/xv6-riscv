#include "types.h"
#include "memlayout.h"

static uint32
rtc_read_low(void)
{
    return *(volatile uint32 *)RTC_LOW;
}

static uint32
rtc_read_high(void)
{
    return *(volatile uint32 *)RTC_HIGH;
}

uint64
rtctime(void)
{
    uint32 low, high;
    low = rtc_read_low();
    high = rtc_read_high();

    return ((uint64)high << 32) | low;
}
