#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

// copied from lib/user/syscall.h because #incuding it caused issues
typedef int pid_t;

tid_t process_execute (const char *);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

mapid_t mmfiles_insert (void *, struct file*, int32_t);
void mmfiles_remove (mapid_t);

#endif /* userprog/process.h */
