#ifndef _PROCINFO_H_
#define _PROCINFO_H_

#define PROC_NAME_LEN 16

struct procinfo
{
    int pid;
    char name[PROC_NAME_LEN];
    int state;
    int ppid;
    char pname[PROC_NAME_LEN];
};

#endif // _PROCINFO_H_