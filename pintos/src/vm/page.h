#include <stdio.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "lib/kernel/hash.h"
#include "filesys/file.h"

enum spe_type{
  SWAP = 1;
  FILE = 2;
  //MMF = 3;
};

struct spe_data{
  struct file * file;
  off_t offset; //file offset
  uint32_t read_bytes;
  uint32_t zero_bytes;
  //!TODO complete this
};

struct sup_page_entry{
  void *user_vaddr;
  enum spe_type type;
  struct spe_data data;
  bool loaded;

  struct hash_elem elem;
};
