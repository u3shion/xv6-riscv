#include "kernel/types.h"
#include "user/user.h"

static int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static int
is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static void
print_date(int val, int width)
{
    if (width == 2)
    {
        if (val < 10)
            printf("0");
    }
    else if (width == 4)
    {
        if (val < 10)
            printf("000");
        else if (val < 100)
            printf("00");
        else if (val < 1000)
            printf("0");
    }
    else if (width == 9)
    {
        for (int i = 100000000; i >= 1; i /= 10)
            printf("%d", (val / i) % 10);

        return;
    }
    printf("%d", val);
}

void ns_to_datetime(uint64 ns, int *year, int *mon, int *day, int *hour, int *min, int *sec, int *nans)
{
    uint64 secs = ns / 1000000000UL;
    *nans = ns % 1000000000UL;

    *year = 1970;
    uint64 days = secs / 86400;
    uint64 secs_today = secs % 86400;

    *hour = (secs_today / 3600);
    *min = (secs_today % 3600) / 60;
    *sec = secs_today % 60;

    while (days >= 365)
    {
        int days_this_year = is_leap_year(*year) ? 366 : 365;

        if (days >= days_this_year)
        {
            days -= days_this_year;
            (*year)++;
        }
        else
            break;
    }

    *mon = 0;
    *day = (int)days;

    while (*day >= days_in_month[*mon] + ((*mon == 1 && is_leap_year(*year)) ? 1 : 0))
    {
        *day -= days_in_month[*mon] + ((*mon == 1 && is_leap_year(*year)) ? 1 : 0);
        (*mon)++;
    }

    *day = *day + 1;
}

int main(void)
{
    uint64 ns;
    int year, mon, day, hour, min, sec, nans;

    ns = rtctime();

    ns_to_datetime(ns, &year, &mon, &day, &hour, &min, &sec, &nans);

    print_date(year, 4);
    printf("-");
    print_date(mon + 1, 2);
    printf("-");
    print_date(day, 2);
    printf(" ");
    print_date(hour, 2);
    printf(":");
    print_date(min, 2);
    printf(":");
    print_date(sec, 2);
    printf(".");
    print_date(nans, 9);
    printf("\n");

    return 0;
}
