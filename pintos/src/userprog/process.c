#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "lib/user/syscall.h"
#include "vm/frame.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *cmdline)
{
  char *cmd_copy;
  char *file_name;
  char *cmdline_cp;
  char *file_name_ptr;
  struct child_status *child;
  tid_t tid;

  //printf("process.c: process_execute: cmdline= %s\n", cmdline);

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  cmd_copy = palloc_get_page (0);
  if (cmd_copy == NULL)
    return TID_ERROR;
  strlcpy (cmd_copy, cmdline, PGSIZE);

  /* obtain executable file name */
  cmdline_cp = (char *) malloc(strlen(cmdline) + 1);
  strlcpy(cmdline_cp, cmdline, strlen(cmdline) + 1);
  file_name = strtok_r(cmdline_cp, " ", &file_name_ptr);

  /* Create a new thread to execute FILE_NAME. */
  //printf("process.c: process_execute: creating thread:%s\n",file_name);
  tid = thread_create (file_name, PRI_DEFAULT, start_process, cmd_copy);

  /* free allocated memory pointed to by file_name */
  free(file_name);

  if (tid == TID_ERROR)
    {
      //printf("process.c: process_execute: thread_create failed: tid == TID_ERROR\n");
      palloc_free_page (cmd_copy);
    }
  else
   {
     child = calloc(1, sizeof(struct child_status));
     child->child_tid = tid;
     child->exited = false;
     child->has_been_waited = false;
     // add new child thread to parents list of children
     list_push_back(&thread_current()->children, &child->elem_child_status);
   }
  //printf("process.c: process_execute: thread_create and child added to parent list success\n");
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *cmdline_)
{
  char *cmd_line = cmdline_;
  int load_status;
  struct intr_frame if_;
  bool success;
  struct thread *parent_thread;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (cmd_line, &if_.eip, &if_.esp);

  // check whether load failed or not
  if(!success)
    load_status = -1;
  else
    load_status = 1;

  parent_thread = thread_get_by_id(thread_current()->parent_tid);
  if (parent_thread != NULL)
   {
     lock_acquire(&parent_thread->child_lock);
     parent_thread->child_load = load_status;
     cond_signal(&parent_thread->child_condition, &parent_thread->child_lock);
     lock_release(&parent_thread->child_lock);
   }

  palloc_free_page (cmdline_);

  /* If load failed, quit. */
  if (!success)
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{
  int ret;
  struct thread *cur;
  struct child_status *child = NULL;
  int child_found = 0;

  if(child_tid != TID_ERROR){
    cur = thread_current();
    //printf("process_wait: waiting for thread \n");
    //search through children for children
    struct list_elem *elem = list_tail(&cur->children);//set the focus to the child at the tail of children
    do{
      child = list_entry(elem, struct child_status, elem_child_status);
      if(child->child_tid == child_tid){//check if the child is the one we want
        child_found = 1;//flag that the child was found
        //printf("process_wait: child found\n");
        break;//finish while loop so child is not overwritten
      }
      elem = list_prev(elem);//since our child was not found, move onto next child
    }while(elem != list_head(&cur->children));//condition for ensuring a null value is not checked

    if(child_found == 0){//repeat loop for head if child still not found
      child = list_entry(elem, struct child_status, elem_child_status);
      if(child->child_tid == child_tid){
        //printf("process_wait: child found head\n");
        child_found = 1;
      }
    }

    //check if we found the child
    //return -1 if we didnt find the children
    if(child_found == 0){
      //printf("process_wait: your child is missing! \n");
      return -1;
    }else{//code for waiting
        lock_acquire(&cur->child_lock);//acquire lock since editing child
        while(thread_get_by_id(child_tid) != NULL){//loop when child is alive
          //printf("process_wait: child is still alive (...so needy): wait till it dies \n");
          cond_wait(&cur->child_condition, &cur->child_lock);//release lock, reacquire when signaled by child
        }
        //if child hasn't called its exit or has been waited by the same process then return -1
        if(!child->exited || child->has_been_waited){
          //printf("process_wait: either child is not exited or has been waited\n");
          lock_release(&cur->child_lock);//release lock since finished editing child
          return -1;
        }
        else{
          //ready the return variable as the child's exit status
          //printf("process_wait: child died and its last words were: %d \n", child->child_exit_status);
          ret = child->child_exit_status;
          //mark child as waited
          child->has_been_waited = true;
        }
        lock_release(&cur->child_lock);//release lock since finished editing child
    }

  }else{
    //printf("process_wait: TID_ERROR \n");
    return TID_ERROR;//return TID_ERROR since child tid is TID_ERROR
  }

  return ret;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  struct thread *parent_thread;
  struct list_elem *elem;
  struct list_elem *temp;
  struct child_status *child;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

  elem = list_begin(&cur->children);
  while(elem != list_tail(&cur->children))
  {
    temp = list_next(elem);
    child = list_entry(elem, struct child_status, elem_child_status);
    list_remove(elem);
    free(child);
    elem = temp;
  }

  // // don't forget to free the tail
  // child = list_entry(elem, struct child_status, elem_child_status);
  // list_remove(elem);
  // free(child);

  // close the files that are opened by the current thread
  close_thread_files(cur->tid);

  parent_thread = thread_get_by_id(cur->parent_tid);
  if (parent_thread != NULL)
  { // update status and signal parent
    lock_acquire(&parent_thread->child_lock);
    if (parent_thread->child_load == 0)
      parent_thread->child_load = -1; // this may hapen if exitted mid load
    cond_signal(&parent_thread->child_condition, &parent_thread->child_lock);
    lock_release(&parent_thread->child_lock);
  }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char *filename);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *cmdline, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  char * file_name;
  char * cmdline_cp;
  char * cmdline_cp_2;
  char * file_name_ptr;

  /* obtain executable file name */
  cmdline_cp = (char *) malloc(strlen(cmdline) + 1);
  strlcpy(cmdline_cp, cmdline, strlen(cmdline) + 1);
  file_name = strtok_r(cmdline_cp, " ", &file_name_ptr);

  // make another copy of cmdline just in case setup_stack wants to modify it
  cmdline_cp_2 = (char *) malloc(strlen(cmdline) + 1);
  strlcpy(cmdline_cp_2, cmdline, strlen(cmdline) + 1);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);

  if (file == NULL)
    {
      free(cmdline_cp);
      goto done;
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      //printf ("load: %s: error loading executable\n", file_name);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, cmdline_cp_2)) {
    goto done;
  }
  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  free(cmdline_cp);
  free(cmdline_cp_2);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = vm_get_frame (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          vm_free_frame (kpage);
          return false;
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable))
        {
          vm_free_frame (kpage);
          return false;
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, char *bufptr)
{
  uint8_t *kpage;
  bool success = false;
  char *token, *save_ptr, *cmdline_cp, **argv, *cmdline;
  int argc = 0, i;

  /* make copy of cmdline info pointed to by bufptr */
  cmdline = (char *) malloc(strlen(bufptr) + 1);
  strlcpy(cmdline, bufptr, strlen(bufptr) + 1);

  kpage = vm_get_frame (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success){
        *esp = PHYS_BASE;

          /* parse cmd line */
          cmdline_cp = (char *) malloc(strlen(cmdline) + 1);
          strlcpy(cmdline_cp, cmdline, strlen(cmdline) + 1);

          for (token = strtok_r (cmdline_cp, " ", &save_ptr); token != NULL;
               token = strtok_r (NULL, " ", &save_ptr))
               argc++;

          /* allocate enough memory for argv */
          argv = (char **)malloc(argc * 4 + 1);

          /* push args onto stack and last time using cmdline so no new copy */
          i = 0;
          for (token = strtok_r (cmdline, " ", &save_ptr); token != NULL;
               token = strtok_r (NULL, " ", &save_ptr))
            {
              *esp -= strlen(token) + 1;
              memcpy(*esp, token, strlen(token) + 1);
              argv[i] = *esp;
              i++ ;
            }

          /* push argv pointers onto stack with 0 padding */
          argv[argc] = 0;

          /* add necessary padding for word size, that is 4-bytes */
          i = (size_t) *esp % 4;
          if (i > 0)
            {
              *esp -= i;
              memcpy(*esp, &argv[argc], i);
            }

          for (i = argc; i >=0; i--)
            {
              *esp -= 4;
              memcpy(*esp, &argv[i], 4);
            }

          /* push argv itself */
          char ** ptr = *esp;
          *esp -= 4;
          memcpy(*esp, &ptr, sizeof(char**));

          /* push argc */
          *esp -= 4;
          memcpy(*esp, &argc, sizeof(int));

          /* push return address (0s)*/
          *esp -= 4;
          memcpy(*esp, &argv[argc], sizeof(void*));

          /* free argv and cmdline cp*/
          free(argv);
          free(cmdline_cp);

          //printf("process.c: setup_stack: *esp = %x\n", *esp);

          //hex_dump((uintptr_t)*esp, *esp , PHYS_BASE - *esp, true);
      }
      else
        vm_free_frame (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
