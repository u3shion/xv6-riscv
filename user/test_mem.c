#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define PAGE_SIZE 4096
#define GLOBAL_VAR_VALUE 42
#define HEAP_ARRAY_SIZE (3 * PAGE_SIZE)

int global_var = GLOBAL_VAR_VALUE;

void print_separator(const char *title)
{
    printf("\n========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

int main(void)
{
    print_separator("Initial state");
    printf("Global variable address: %p\n", (void *)&global_var);
    printf("\nPage table at startup:\n\n");
    printpgtable();

    print_separator("Access global variable");
    printf("Global variable value: %d\n", global_var);
    global_var = 100;
    printf("Modified global variable: %d\n", global_var);
    printf("\nPage table after accessing global variable:\n\n");
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

    printf("\nPage table after allocation:\n\n");
    printpgtable();

    print_separator("First access to heap (read)");

    int val1 = heap_arr[0];
    int val2 = heap_arr[PAGE_SIZE / sizeof(int)];
    int val3 = heap_arr[(2 * PAGE_SIZE) / sizeof(int)];

    printf("Read values: %d, %d, %d\n", val1, val2, val3);

    printf("\nPage table after reading from heap:\n\n");
    printpgtable();

    print_separator("Check Accessed flag after read");
    int mask_a = 1 << 6;

    int has_a_flag = checkflags((void *)heap_arr, PAGE_SIZE, mask_a);
    printf("First heap page has Accessed flag: %s\n", has_a_flag ? "YES" : "NO");

    print_separator("Write to heap");

    heap_arr[0] = 999;
    heap_arr[PAGE_SIZE / sizeof(int)] = 777;
    heap_arr[(2 * PAGE_SIZE) / sizeof(int)] = 555;

    printf("Page table after writing to heap:\n\n");
    printpgtable();

    print_separator("Check Dirty flag after write");
    int mask_d = 1 << 7;

    int has_d_flag = checkflags((void *)heap_arr, HEAP_ARRAY_SIZE, mask_d);
    printf("Heap pages have Dirty flag: %s\n", has_d_flag ? "YES" : "NO");

    print_separator("Clear A and D flags");

    int mask_ad = mask_a | mask_d;
    int result = clearflags((void *)heap_arr, HEAP_ARRAY_SIZE, mask_ad);

    if (result != 0)
        printf("Error clearing flags\n");

    printf("Page table after clearing flags:\n\n");
    printpgtable();

    print_separator("Access after flag clearing");
    int read_val = heap_arr[PAGE_SIZE / sizeof(int) + 10];
    printf("Read value: %d\n", read_val);

    heap_arr[PAGE_SIZE / sizeof(int) + 20] = 333;

    printf("\nPage table after access:\n\n");
    printpgtable();

    print_separator("Verify flags reappeared");
    has_a_flag = checkflags((void *)heap_arr, PAGE_SIZE, mask_a);
    has_d_flag = checkflags((void *)heap_arr, HEAP_ARRAY_SIZE, mask_d);
    printf("Accessed flag present: %s\n", has_a_flag ? "YES" : "NO");
    printf("Dirty flag present: %s\n", has_d_flag ? "YES" : "NO");

    print_separator("Global variable access again");
    printf("Current global variable: %d\n", global_var);
    global_var = 200;
    printf("Modified global variable: %d\n", global_var);

    printf("\nPage table after global variable access:\n\n");
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
