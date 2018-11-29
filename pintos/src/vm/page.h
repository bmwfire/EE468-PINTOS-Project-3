#ifndef VM_PAGEH
#define VM_PAGEH

#include <stdio.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "lib/kernel/hash.h"
#include "filesys/file.h"

enum spe_type{
  SWAP = 1,
  FILE = 2,
  //MMF = 3;
};

struct spe_data{
  struct file * file;
  off_t offset; //file offset
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;
  //!TODO complete this
};

struct sup_page_entry{
  void *user_vaddr;
  enum spe_type type;
  struct spe_data data;
  bool loaded;

  struct hash_elem elem;
};

void vm_page_init(void);

struct sup_page_entry * get_spe(struct hash *, void * );

unsigned suppl_pt_hash (const struct hash_elem *, void * UNUSED);
bool suppl_pt_less (const struct hash_elem *, const struct hash_elem *, void * UNUSED);

void free_sp(struct hash *);
bool load_page(struct sup_page_entry *);

#endif
