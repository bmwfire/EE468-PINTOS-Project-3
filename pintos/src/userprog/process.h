#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

// copied from lib/user/syscall.h because #incuding it caused issues
typedef int pid_t;

/* States of a file that has been loaded to be run by a process. */
enum load_status
  {
    NOT_LOADED,         /* Initial loading state. */
    LOAD_SUCCESS,       /* The file was loaded successfully with no issues. */
    LOAD_FAILED         /* The file failed to load. */
  };

/* A structure storing infomation about a child process. */
struct process
  {
    struct list_elem elem;        /* List element for child processes list. */
    pid_t pid;                    /* The process/thread id. */
    bool is_alive;                /* Whether the process has exited or not. */
    bool is_waited;               /* Whether the process has been or is waited upon. */
    int exit_status;              /* The exit status of the process. */
    enum load_status load_status; /* The load status of the file being executed by the process. */
    struct semaphore wait;        /* Used to wait for a process to exit before returning it's exit status. */
    struct semaphore load;        /* Used to wait for the file being executed to load (or fail to load). */
  };

tid_t process_execute (const char *);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
