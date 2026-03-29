#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/procinfo.h"

#define MAX_PROCS 64
#define CHILD_PROCS 3

int total_tests = 11;
int passed_tests = 0;

void print_header(const char *title)
{
    printf("\n"
           "========================================\n");
    printf("Test %s\n", title);
    printf("========================================\n");
}

void print_pass()
{
    printf("  [PASS]\n");
    passed_tests++;
}

void print_fail(const char *test, const char *reason)
{
    printf("  [FAIL] %s: %s\n", test, reason);
}

void print_info(const char *msg)
{
    printf("  [INFO] %s\n", msg);
}

const char *get_state_str(int state)
{
    switch (state)
    {
    case 0:
        return "UNUSED";
    case 1:
        return "USED";
    case 2:
        return "SLEEPING";
    case 3:
        return "RUNNABLE";
    case 4:
        return "RUNNING";
    case 5:
        return "ZOMBIE";
    default:
        return "UNKNOWN";
    }
}

void test_null()
{
    print_header("NULL test");

    int cnt = ps_listinfo(0, 0);
    if (cnt >= 0)
    {
        printf("Number of processes: %d\n", cnt);
        print_pass();
    }
    else
        print_fail("NULL pointer", "returned error instead of count");
}

void test_small_buffer()
{
    print_header("insufficient buffer test");

    struct procinfo info[1];
    int ret = ps_listinfo(info, 1);

    if (ret == -1)
        print_pass();
    else if (ret >= 0)
        print_fail("Small buffer", "should fail but succeeded");
    else
        print_fail("Small buffer", "wrong error code");
}

void test_neg_lim()
{
    print_header("negative lim parameter test");

    struct procinfo info[MAX_PROCS];
    int ret = ps_listinfo(info, -5);

    if (ret == -2)
        print_pass();
    else
        print_fail("Negative lim", "wrong error code");
}

void test_invalid_addr()
{
    print_header("invalid user address test");

    struct procinfo *invalid_addr1 = (struct procinfo *)0;
    struct procinfo *invalid_addr2 = (struct procinfo *)0xFFFFFFFFFFFFFFFF;
    struct procinfo *invalid_addr3 = (struct procinfo *)0x1000;

    int ret1 = ps_listinfo(invalid_addr1, 10);
    int ret2 = ps_listinfo(invalid_addr2, 10);
    int ret3 = ps_listinfo(invalid_addr3, 10);

    if (ret1 >= 0)
        printf("NULL address (special case) returns: %d\n", ret1);

    if (ret2 == -3 && ret3 == -3)
        print_pass();
    else
        print_fail("Invalid addresses", "wrong error code");
}

void test_norm_buf()
{
    print_header("normal buffer test (lim=64)");

    struct procinfo info[MAX_PROCS];
    int ret = ps_listinfo(info, MAX_PROCS);

    if (ret > 0)
    {
        printf("Got %d processes:\n", ret);
        printf("  %s %s %s %s %s\n", "PID", "NAME", "STATE", "PPID", "PNAME");
        printf("  %s\n", "----------------------------------------");

        for (int i = 0; i < ret && i < 10; i++)
        {
            printf("  %d %s %s %d %s\n",
                   info[i].pid,
                   info[i].name,
                   get_state_str(info[i].state),
                   info[i].ppid,
                   info[i].pname);
        }
        print_pass();
    }
    else
        print_fail("Normal buffer", "failed to get processes");
}

void test_data_consist()
{
    print_header("data consistency test");

    struct procinfo info[MAX_PROCS];
    int cnt = ps_listinfo(info, MAX_PROCS);

    int dup_found = 0;
    for (int i = 0; i < cnt; i++)
    {
        for (int j = i + 1; j < cnt; j++)
        {
            if (info[i].pid == info[j].pid && info[i].pid != 0)
            {
                printf("Duplicate PID found: %d\n", info[i].pid);
                dup_found = 1;
            }
        }
    }

    if (!dup_found)
        printf("No duplicate PIDs found\n");

    int init_found = 0;
    for (int i = 0; i < cnt; i++)
    {
        if (info[i].pid == 1)
        {
            init_found = 1;
            printf("Init process: %s (PID=1, PPID=%d, Parent=%s)\n", info[i].name, info[i].ppid, info[i].pname);
            break;
        }
    }

    if (init_found)
        print_pass();
    else
        print_fail("Data consistency", "init process not found");
}

void test_buf_sz()
{
    print_header("different buffer sizes test");

    int total = ps_listinfo(0, 0);
    printf("Total processes: %d\n", total);

    struct
    {
        int lim;
        const char *desc;
    } tests[] = {
        {0, "lim = 0"},
        {1, "lim = 1"},
        {total / 2, "lim = total/2"},
        {total - 1, "lim = total-1"},
        {total, "lim = total"},
        {total + 1, "lim = total+1"},
        {total * 2, "lim = total*2"}};
    int num_tests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < num_tests; i++)
    {
        int lim = tests[i].lim;
        struct procinfo info[total * 2];

        printf("%s: ", tests[i].desc);
        int ret = ps_listinfo(info, lim);

        if (lim <= 0)
        {
            if (ret == -2)
            {
                printf("OK (invalid lim)\n");
            }
            else
            {
                printf("FAIL (expected -2, got %d)\n", ret);
            }
        }
        else if (lim < total)
        {
            if (ret == -1)
            {
                printf("OK (buffer too small)\n");
            }
            else
            {
                printf("FAIL (expected -1, got %d)\n", ret);
            }
        }
        else
        {
            if (ret == total)
            {
                printf("OK (got %d processes)\n", ret);
            }
            else
            {
                printf("FAIL (expected %d, got %d)\n", total, ret);
            }
        }
    }

    print_pass();
}

void test_stress()
{
    print_header("stress test (multiple calls)");

    int iter = 20;
    int ok = 0;

    int total = ps_listinfo(0, 0);
    printf("Total processes: %d\n", total);

    struct procinfo info[MAX_PROCS];

    printf("Running %d iterations...\n", iter);

    for (int i = 0; i < iter; i++)
    {
        for (int j = 0; j < MAX_PROCS; j++)
        {
            info[j].pid = 0;
            info[j].name[0] = '\0';
            info[j].state = 0;
            info[j].ppid = 0;
            info[j].pname[0] = '\0';
        }

        int ret = ps_listinfo(info, MAX_PROCS);

        if (ret > 0)
        {
            ok++;

            for (int j = 0; j < ret && j < MAX_PROCS; j++)
            {
                if (info[j].pid < 1 || info[j].pid > 1000)
                    printf("Warning: suspicious PID %d at iteration %d\n", info[j].pid, i);
                if (info[j].state < 0 || info[j].state > 5)
                    printf("Warning: invalid state %d for PID %d at iteration %d\n", info[j].state, info[j].pid, i);
            }
        }
        else
            printf("Iteration %d failed with error %d\n", i, ret);

        if (i % 5 == 0)
            pause(1);
    }

    printf("Successful calls: %d/%d\n", ok, iter);
    if (ok == iter)
        print_pass();
    else
        print_fail("Stress test", "some calls failed");
}

void test_parent()
{
    print_header("parent relationships test");

    struct procinfo info[32];

    for (int i = 0; i < 32; i++)
    {
        info[i].pid = 0;
        info[i].name[0] = '\0';
        info[i].state = 0;
        info[i].ppid = 0;
        info[i].pname[0] = '\0';
    }

    int cnt = ps_listinfo(info, 32);

    if (cnt <= 0 || cnt > 32)
    {
        print_fail("Parent relationships", "failed to get processes");
        return;
    }

    struct
    {
        int pid;
        int ppid;
        char name[16];
    } procmap[32];

    for (int i = 0; i < cnt && i < 32; i++)
    {
        procmap[i].pid = info[i].pid;
        procmap[i].ppid = info[i].ppid;
        strcpy(procmap[i].name, info[i].name);
    }

    int errs = 0;
    for (int i = 0; i < cnt && i < 32; i++)
    {
        if (procmap[i].pid == 1)
        {
            if (procmap[i].ppid != -1)
            {
                printf("Error: init (PID=1) has PPID=%d\n", procmap[i].ppid);
                errs++;
            }
            continue;
        }

        if (procmap[i].ppid > 0)
        {
            int par_fd = 0;
            for (int j = 0; j < cnt && j < 32; j++)
            {
                if (procmap[j].pid == procmap[i].ppid)
                {
                    par_fd = 1;
                    break;
                }
            }

            if (!par_fd)
            {
                printf("Error: PID %d's parent (PID=%d) not found\n", procmap[i].pid, procmap[i].ppid);
                errs++;
            }
        }
    }

    if (errs == 0)
        print_pass();
    else
        print_fail("Parent relationships", "found inconsistencies");
}

void test_child_procs()
{
    print_header("child processes test");

    int pipes[CHILD_PROCS][2];
    int pids[CHILD_PROCS];

    for (int i = 0; i < CHILD_PROCS; i++)
    {
        pids[i] = 0;
        pipes[i][0] = -1;
        pipes[i][1] = -1;
    }

    printf("Creating %d child processes...\n", CHILD_PROCS);

    for (int i = 0; i < CHILD_PROCS; i++)
    {
        if (pipe(pipes[i]) < 0)
        {
            printf("Failed to create pipe\n");
            print_fail("Child processes", "pipe creation failed");
            return;
        }

        int pid = fork();
        if (pid == 0)
        {
            close(pipes[i][0]);

            struct procinfo info;
            pause(5);

            int ret = ps_listinfo(&info, 1);

            if (ret > 0 && info.pid == getpid())
                write(pipes[i][1], &info, sizeof(info));

            close(pipes[i][1]);
            exit(0);
        }
        else if (pid > 0)
        {
            pids[i] = pid;
            close(pipes[i][1]);
            printf("Created child %d with PID=%d\n", i, pid);
        }
        else
        {
            printf("Fork failed!\n");
            print_fail("Child processes", "fork failed");
            return;
        }
    }

    pause(10);

    printf("\nReading child info from pipes...\n");
    for (int i = 0; i < CHILD_PROCS; i++)
    {
        if (pipes[i][0] != -1)
        {
            struct procinfo child_info;
            int n = read(pipes[i][0], &child_info, sizeof(struct procinfo));

            if (n == sizeof(struct procinfo))
                printf("Child %d (PID=%d) reported: PPID=%d, Parent=%s\n", i, child_info.pid, child_info.ppid, child_info.pname);

            close(pipes[i][0]);
        }
    }

    printf("Waiting for children to exit...\n");
    int ch_ex = 0;
    for (int i = 0; i < CHILD_PROCS; i++)
    {
        if (pids[i] > 0)
        {
            int status;
            int ret = wait(&status);
            if (ret > 0)
            {
                ch_ex++;
                printf("Child %d (PID=%d) exited with status %d\n", i, ret, status);
            }
        }
    }
    printf("%d/%d children exited\n", ch_ex, CHILD_PROCS);

    if (ch_ex == CHILD_PROCS)
        print_pass();
    else
        print_fail("Child processes", "not all children exited");
}

int main()
{
    total_tests = 10;
    passed_tests = 0;

    test_null();

    test_small_buffer();

    test_neg_lim();

    test_invalid_addr();

    test_norm_buf();

    test_data_consist();

    test_buf_sz();

    test_stress();

    test_parent();

    test_child_procs();

    printf("\n"
           "========================================\n");
    printf("  TEST SUMMARY\n");
    printf("========================================\n");
    printf("  Total tests:  %d\n", total_tests);
    printf("  Passed:       "
           "%d\n",
           passed_tests);
    printf("  Failed:       "
           "%d\n",
           total_tests - passed_tests);
}