#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define PAGE_SIZE 4096
#define GLOBAL_VAR_VALUE 42
#define HEAP_ARRAY_SIZE (3 * PAGE_SIZE)

int global_var = GLOBAL_VAR_VALUE;
int mask_a = 1 << 6;
int mask_d = 1 << 7;

void print_separator(const char *title)
{
    printf("\n========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

void show_flags_status(const char *label, void *buf, int len, const char *flag_name, int mask)
{
    int result = checkflags(buf, len, mask);
    printf("[%s] %s flag: %s\n", label, flag_name, result ? "1 (SET)" : "0 (NOT SET)");
}

int main(void)
{
    print_separator("Initial state");
    printf("Global variable address: %p\n", (void *)&global_var);
    printf("\nPage table at startup:\n\n");
    printpgtable();

    print_separator("Access global variable");
    printf("BEFORE: Global variable = %d\n", global_var);
    show_flags_status("BEFORE", (void *)&global_var, sizeof(int), "A", mask_a);
    show_flags_status("BEFORE", (void *)&global_var, sizeof(int), "D", mask_d);

    int val = global_var;
    printf("Read value: %d\n", val);

    show_flags_status("AFTER READ", (void *)&global_var, sizeof(int), "A", mask_a);
    show_flags_status("AFTER READ", (void *)&global_var, sizeof(int), "D", mask_d);

    global_var = 100;
    printf("Modified global variable: %d\n", global_var);

    show_flags_status("AFTER WRITE", (void *)&global_var, sizeof(int), "A", mask_a);
    show_flags_status("AFTER WRITE", (void *)&global_var, sizeof(int), "D", mask_d);

    printf("\nPage table after global variable access:\n\n");
    printpgtable();

    print_separator("Stack variables");
    int stack_var = 55;
    int stack_arr[16];

    printf("Stack variable address: %p (value: %d)\n", (void *)&stack_var, stack_var);
    printf("Stack array address: %p\n", (void *)stack_arr);

    for (int i = 0; i < 16; i++)
        stack_arr[i] = i * 10;

    printf("\nPage table after stack access:\n\n");
    printpgtable();

    print_separator("Allocate heap memory");

    int *heap_arr = (int *)sbrklazy(HEAP_ARRAY_SIZE);
    if (heap_arr == (int *)SBRK_ERROR)
    {
        printf("Error: sbrklazy failed\n");
        exit(1);
    }

    printf("Heap array allocated at: %p\n", (void *)heap_arr);
    printf("Heap array end: %p\n", (void *)(heap_arr + HEAP_ARRAY_SIZE / sizeof(int)));

    show_flags_status("AFTER ALLOCATION", (void *)heap_arr, PAGE_SIZE, "A", mask_a);
    show_flags_status("AFTER ALLOCATION", (void *)heap_arr, PAGE_SIZE, "D", mask_d);

    printf("\nPage table after allocation:\n\n");
    printpgtable();

    print_separator("First access to heap (read)");

    show_flags_status("BEFORE", (void *)heap_arr, PAGE_SIZE, "A", mask_a);
    show_flags_status("BEFORE", (void *)heap_arr, PAGE_SIZE, "D", mask_d);

    int val1 = heap_arr[0];
    int val2 = heap_arr[PAGE_SIZE / sizeof(int)];
    int val3 = heap_arr[(2 * PAGE_SIZE) / sizeof(int)];

    printf("Read values: %d, %d, %d\n", val1, val2, val3);

    show_flags_status("AFTER READ (page 1)", (void *)heap_arr, PAGE_SIZE, "A", mask_a);
    show_flags_status("AFTER READ (page 1)", (void *)heap_arr, PAGE_SIZE, "D", mask_d);
    show_flags_status("AFTER READ (all heap)", (void *)heap_arr, HEAP_ARRAY_SIZE, "A", mask_a);
    show_flags_status("AFTER READ (all heap)", (void *)heap_arr, HEAP_ARRAY_SIZE, "D", mask_d);

    printf("\nPage table after reading from heap:\n\n");
    printpgtable();

    print_separator("Write to heap");

    show_flags_status("BEFORE", (void *)heap_arr, HEAP_ARRAY_SIZE, "A", mask_a);
    show_flags_status("BEFORE", (void *)heap_arr, HEAP_ARRAY_SIZE, "D", mask_d);

    heap_arr[0] = 999;
    heap_arr[PAGE_SIZE / sizeof(int)] = 777;
    heap_arr[(2 * PAGE_SIZE) / sizeof(int)] = 555;
    printf("Values written\n");

    show_flags_status("AFTER WRITE", (void *)heap_arr, HEAP_ARRAY_SIZE, "A", mask_a);
    show_flags_status("AFTER WRITE", (void *)heap_arr, HEAP_ARRAY_SIZE, "D", mask_d);

    printf("\nPage table after writing to heap:\n\n");
    printpgtable();

    print_separator("Clear A and D flags");

    show_flags_status("BEFORE", (void *)heap_arr, HEAP_ARRAY_SIZE, "A", mask_a);
    show_flags_status("BEFORE", (void *)heap_arr, HEAP_ARRAY_SIZE, "D", mask_d);

    int mask_ad = mask_a | mask_d;
    int result = clearflags((void *)heap_arr, HEAP_ARRAY_SIZE, mask_ad);

    if (result != 0)
        printf("Error clearing flags\n");

    show_flags_status("AFTER CLEAR", (void *)heap_arr, HEAP_ARRAY_SIZE, "A", mask_a);
    show_flags_status("AFTER CLEAR", (void *)heap_arr, HEAP_ARRAY_SIZE, "D", mask_d);

    printf("\nPage table after clearing flags:\n\n");
    printpgtable();

    print_separator("Access after flag clearing");

    show_flags_status("BEFORE", (void *)heap_arr, HEAP_ARRAY_SIZE, "A", mask_a);
    show_flags_status("BEFORE", (void *)heap_arr, HEAP_ARRAY_SIZE, "D", mask_d);

    int read_val = heap_arr[PAGE_SIZE / sizeof(int) + 10];
    printf("Read value: %d\n", read_val);

    heap_arr[PAGE_SIZE / sizeof(int) + 20] = 333;
    printf("Written to page 2\n");

    show_flags_status("AFTER ACCESS (page 1)", (void *)heap_arr, PAGE_SIZE, "A", mask_a);
    show_flags_status("AFTER ACCESS (page 1)", (void *)heap_arr, PAGE_SIZE, "D", mask_d);
    show_flags_status("AFTER ACCESS (page 2 only)", (void *)(heap_arr + PAGE_SIZE / sizeof(int)), PAGE_SIZE, "A", mask_a);
    show_flags_status("AFTER ACCESS (page 2 only)", (void *)(heap_arr + PAGE_SIZE / sizeof(int)), PAGE_SIZE, "D", mask_d);
    show_flags_status("AFTER ACCESS (all heap)", (void *)heap_arr, HEAP_ARRAY_SIZE, "A", mask_a);
    show_flags_status("AFTER ACCESS (all heap)", (void *)heap_arr, HEAP_ARRAY_SIZE, "D", mask_d);

    printf("\nPage table after access:\n\n");
    printpgtable();

    print_separator("Demonstrate clear/check on specific pages");

    show_flags_status("BEFORE CLEARING A FLAG (page 2)", (void *)(heap_arr + PAGE_SIZE / sizeof(int)), PAGE_SIZE, "A", mask_a);

    clearflags((void *)(heap_arr + PAGE_SIZE / sizeof(int)), PAGE_SIZE, mask_a);

    show_flags_status("AFTER CLEARING A FLAG (page 2)", (void *)(heap_arr + PAGE_SIZE / sizeof(int)), PAGE_SIZE, "A", mask_a);
    show_flags_status("AFTER (all heap)", (void *)heap_arr, HEAP_ARRAY_SIZE, "A", mask_a);

    printf("\nPage table after clearing:\n\n");
    printpgtable();

    print_separator("Free heap memory");

    char *addr = sbrk(-(HEAP_ARRAY_SIZE));
    if (addr == SBRK_ERROR)
    {
        printf("Error: sbrk shrink failed\n");
        exit(1);
    }

    printf("Page table after memory deallocation:\n\n");
    printpgtable();

    exit(0);
}
