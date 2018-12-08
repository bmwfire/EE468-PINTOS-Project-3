#ifndef VM_PAGEH
#define VM_PAGEH

#define STACK_SIZE (8 * (1 << 20))

#include <stdio.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "lib/kernel/hash.h"
#include "filesys/file.h"

enum spe_type{
  SWAP = 1,
  FILE = 2,
  MMF = 4
};

union spe_data{
  struct
  {
    struct file * file;
    off_t offset; //file offset
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
  } file_page;

  struct
  {
    struct file * file;
    off_t offset;
    uint32_t read_bytes;
  } mmf_page;
};

struct sup_page_entry{
  void *user_vaddr;
  enum spe_type type;
  union spe_data data;
  bool loaded;

  size_t swap_slot_idx;
  bool swap_writable;

  struct hash_elem elem;
};

void vm_page_init(void);

struct sup_page_entry * get_spe(struct hash *, void * );

bool suppl_pt_insert_mmf (struct file *file, off_t ofs, uint8_t *upage,
                     uint32_t read_bytes);

unsigned suppl_pt_hash (const struct hash_elem *, void * UNUSED);
bool suppl_pt_less (const struct hash_elem *, const struct hash_elem *, void * UNUSED);

void free_sp(struct hash *);
bool load_page(struct sup_page_entry *);
void grow_stack (void *);
//void write_page_back_to_file_wo_lock (struct suppl_pte *spte);

#endif
