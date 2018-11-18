#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"


static void * frame_evict();
static void add_frame_table_entry(void * new_frame_ptr);

void
vm_frame_table_init()
{
  list_init(&frame_table);
}

/* allocate a page from USER_POOL and add the new entry to the frame table.
Pintos Doc: The frames used for user pages should be obtained from the
“user pool,” by calling palloc_get_page(PAL_USER).
Note that this is method is call vm_get_frame since Pintos maps kernel virtual
memory directly to physical memory, i.e. the kernel page adress is the same as
the physical frame address. */
void *
frame vm_get_frame(enum palloc_flags flags)
{
  void *new_frame_ptr = NULL;

  /* check if frame is going to be used for a user page */
  if (flags & PAL_USER)
  {
    new_frame_ptr = palloc_get_page(flags);
  }

  /* on success add frame to frame table */
  if (new_frame_ptr != NULL)
  {

  } else
  {
    /* Evict a page from a frame */
    new_frame_ptr = frame_evict();
    /* frame is already in frame table and its contents were updated in
    frame_evict()*/
  }

  return new_frame_ptr;
}

/* add an entry to the frame table */
static void
add_frame_table_entry(void * new_frame_ptr)
{
  struct frame_table_entry * new_frame_table_entry;
  new_frame_table_entry = malloc(sizeof(* new_frame_table_entry));

  ASSERT(new_frame_table_entry != NULL);

  new_frame_table_entry->frame_ptr = new_frame_ptr;
  new_frame_table_entry->owner_thread_tid = thread_current()->tid;

  list_push_front(&frame_table, &new_frame_table_entry->elem);
}
