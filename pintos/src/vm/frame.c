#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "lib/kernel/list.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"


static void * evict_page_from_frame(void);
static void add_frame_table_entry(void * new_frame_ptr);
static struct frame_table_entry * next_frame_table_entry_to_clear(void);
static void save_evicted_page(struct frame_table_entry * next_fte_to_clear);

static struct lock frame_table_lock;

void
vm_frame_table_init()
{
  lock_init(&frame_table_lock);
  list_init(&frame_table);
}

void vm_free_frame(void *frame)
{
  struct frame_table_entry * temp_frame_table_entry;
  struct list_elem * fte_list_elem;

  /* remove the entry from the frame table and free allocated memory */
  lock_acquire(&frame_table_lock);
  fte_list_elem = list_head(&frame_table);
  while((fte_list_elem = list_next(fte_list_elem)) != list_tail(&frame_table))
  {
    temp_frame_table_entry = list_entry(fte_list_elem, struct
                                        frame_table_entry, elem);
    if(temp_frame_table_entry->frame_ptr == frame)
    {
      list_remove(fte_list_elem);
      /* free the memory for frame table entry struct */
      free(temp_frame_table_entry);
      break;
    }
  }
  lock_release(&frame_table_lock);

  /* free the actual frame */
  palloc_free_page(frame);
}


/* allocate a page from USER_POOL and add the new entry to the frame table.
Pintos Doc: The frames used for user pages should be obtained from the
“user pool,” by calling palloc_get_page(PAL_USER).
Note that this is method is call vm_get_frame since Pintos maps kernel virtual
memory directly to physical memory, i.e. the kernel page adress is the same as
the physical frame address. */
void *
vm_get_frame(enum palloc_flags flags)
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
    add_frame_table_entry(new_frame_ptr);
  } else
  {
    /* Evict a page from a frame */
    new_frame_ptr = evict_page_from_frame();
    /* frame is already in frame table and its contents were updated in
    evict_page_from_frame()*/
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

  /* acquire lock to modify frame table */
  lock_acquire(&frame_table_lock);
  list_push_back(&frame_table, &new_frame_table_entry->elem);
  lock_release(&frame_table_lock);
}

/* Evict a page from a frame to make room for a new page */
static void *
evict_page_from_frame()
{
  struct frame_table_entry * cleared_frame_table_entry;

  lock_acquire(&frame_table_lock);

  cleared_frame_table_entry = next_frame_table_entry_to_clear();
  save_evicted_page(cleared_frame_table_entry);

  cleared_frame_table_entry->owner_thread_tid = thread_current()->tid;
  cleared_frame_table_entry->page_ptr = NULL;

  lock_release(&frame_table_lock);

  return cleared_frame_table_entry;
}

/* return the next frame table entry to clear */
static struct frame_table_entry *
next_frame_table_entry_to_clear()
{
  struct frame_table_entry * next_fte_to_clear = NULL;
  struct frame_table_entry * temp_frame_table_entry;
  struct list_elem * fte_list_elem;
  struct thread *temp_thread;
  /* search through frame table till a page with accessed bit is found to be 0 */
  fte_list_elem = list_head(&frame_table);
  while((fte_list_elem = list_next(fte_list_elem)) != list_tail (&frame_table))
  {
    temp_frame_table_entry = list_entry(fte_list_elem,
      struct frame_table_entry, elem);
    temp_thread = thread_get_by_id(temp_frame_table_entry->owner_thread_tid);
    if(!pagedir_is_accessed(temp_thread->pagedir,
      temp_frame_table_entry->page_ptr))
    {
      /* found a page with access bit 0 */
      next_fte_to_clear = temp_frame_table_entry;
      /* maintain that the youngest frame is in the back of the list */
      list_remove(fte_list_elem);
      list_push_back(&frame_table, fte_list_elem);
      break;
    } else
    {
      /* also clear accessed bits while we are here */
      pagedir_set_accessed(temp_thread->pagedir,
        temp_frame_table_entry->page_ptr, false);
    }
  }

  if(next_fte_to_clear == NULL)
  {
    /* no bits had a 0 access bit, just clear the oldest one */
    next_fte_to_clear = list_entry(list_begin(&frame_table),
                                    struct frame_table_entry, elem);
  }

  ASSERT(next_fte_to_clear != NULL);

  return next_fte_to_clear;
}

/* save page that is being evicted and add to the swap table */
void
save_evicted_page(struct frame_table_entry * next_fte_to_clear)
{
  // TODO
}
