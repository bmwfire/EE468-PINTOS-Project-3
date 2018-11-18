#include "threads/thread.h"
#include "threads/palloc.h"

/* Frame table entry structure */
struct frame_table_entry {
  void *frame_ptr; // pointer to the frame holding the user page
  void *page_ptr; // pointer to the page that is currently occupies the frame
  tid_t owner_thread_tid; // thread id of the thread which allocated the frame
  struct list_elem elem; // list entry for frame table
};

struct list frame_table; // the frame table

/* functions in frame.c */
void *frame vm_get_frame(enum palloc_flags flags);
void vm_free_frame(void *frame);
void vm_frame_table_init();
