#include "vm/page.h"
#include "threads/pte.h"
#include "vm/frame.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "string.h"
#include "userprog/syscall.h"
#include "vm/swap.h"

void vm_page_init (void);
struct sup_page_entry * get_spe(struct hash *ht, void * user_vaddr);
bool load_page(struct sup_page_entry *spe);
bool load_file_page(struct sup_page_entry *spe);
static bool load_page_swap (struct sup_page_entry *spte);
static bool load_page_mmf (struct sup_page_entry *spte);

void free_sp(struct hash *spe);
static void free_sp_entry(struct hash_elem *he, void *aux UNUSED);

unsigned suppl_pt_hash (const struct hash_elem *he, void *aux UNUSED);
bool suppl_pt_less (const struct hash_elem *hea, const struct hash_elem *heb, void *aux UNUSED);

void vm_page_init (void){
  return;
}

//find hashelement corresponding to a certain user virtual address in a hash table. return success
struct sup_page_entry * get_spe(struct hash *h, void * user_vaddr){
  struct sup_page_entry spe;
  struct hash_elem *he;

  spe.user_vaddr = user_vaddr;
  he = hash_find(h, &spe.elem);
  if(he != NULL){
    return hash_entry(he, struct sup_page_entry, elem);
  }
  return NULL;
}

//load page NOTE this may need to be changed to account for mmf and swapping
bool load_page(struct sup_page_entry *spe){
//  if(spe->type == FILE){
//    return load_file_page(spe);
//  }
//  return false;
  bool success = false;
  switch (spe->type)
  {
  case FILE:
    success = load_file_page (spe);
    break;
  case MMF:
  case MMF | SWAP:
    success = load_page_mmf (spe);
    break;
  case FILE | SWAP:
  case SWAP:
    success = load_page_swap (spe);
    break;
  default:
    break;
  }
  return success;
}

//load a page that is type file. return success
bool load_file_page(struct sup_page_entry *spe){
  struct thread *curr = thread_current ();

  file_seek (spe->data.file_page.file, spe->data.file_page.offset);//look in
  // file at
  // offset

  uint8_t *page = vm_get_frame(PAL_USER);//allocate a page
  if(page == NULL){//allocation must have failed, load unsuccessful
    return false;
  }

  if(file_read(spe->data.file_page.file, page, spe->data.file_page
      .read_bytes) != (int)
      spe->data.file_page.read_bytes){//check if file is at certain offset.
    // clear frame and return fail if it is
    vm_free_frame(page);
    return false;
  }

  memset (page + spe->data.file_page.read_bytes, 0, spe->data.file_page
      .zero_bytes);
  //set the zero bytes of the page to 0

  if(!pagedir_set_page(curr->pagedir, spe->user_vaddr, page, spe->data
      .file_page.writable)){//attempt to map page to physical
    vm_free_frame(page);//if failed to map, then free frame and return unsuccessful
    return false;
  }

//load was succesful, indicate as such
  spe->loaded = true;
  return true;


}

//free supplemntal page table
void free_sp(struct hash *spe){
  hash_destroy(spe, free_sp_entry);
}

static void free_sp_entry(struct hash_elem *he, void *aux UNUSED){
  struct sup_page_entry *spe;
  spe = hash_entry (he, struct sup_page_entry, elem);

  //NOTE: might need swap stuff here
  free(spe);
}


/* needed since sp is a hash*/
unsigned suppl_pt_hash (const struct hash_elem *he, void *aux UNUSED)
{
  const struct sup_page_entry *vspte;
  vspte = hash_entry (he, struct sup_page_entry, elem);
  return hash_bytes (&vspte->user_vaddr, sizeof vspte->user_vaddr);
}

/* needed since sp is a hash*/
bool suppl_pt_less (const struct hash_elem *hea, const struct hash_elem *heb, void *aux UNUSED)
{
  const struct sup_page_entry *vsptea;
  const struct sup_page_entry *vspteb;

  vsptea = hash_entry (hea, struct sup_page_entry, elem);
  vspteb = hash_entry (heb, struct sup_page_entry, elem);

  return (vsptea->user_vaddr - vspteb->user_vaddr) < 0;
}

void grow_stack (void *uvaddr)
{
  void *spage;
  struct thread *t = thread_current ();
  spage = vm_get_frame (PAL_USER | PAL_ZERO);
  if (spage == NULL)
    return;
  else
  {
    /* Add the page to the process's address space. */
    if (!pagedir_set_page (t->pagedir, pg_round_down (uvaddr), spage, true))
    {
      vm_free_frame (spage);
    }
  }
}

/* Load a mmf page whose details are defined in struct suppl_pte */
static bool
load_page_mmf (struct sup_page_entry *spte)
{
  struct thread *cur = thread_current ();

  file_seek (spte->data.mmf_page.file, spte->data.mmf_page.offset);

  /* Get a page of memory. */
  uint8_t *kpage = vm_get_frame (PAL_USER);
  if (kpage == NULL)
    return false;

  /* Load this page. */
  if (file_read (spte->data.mmf_page.file, kpage,
                 spte->data.mmf_page.read_bytes)
      != (int) spte->data.mmf_page.read_bytes)
  {
    vm_free_frame (kpage);
    return false;
  }
  memset (kpage + spte->data.mmf_page.read_bytes, 0,
          PGSIZE - spte->data.mmf_page.read_bytes);

  /* Add the page to the process's address space. */
  if (!pagedir_set_page (cur->pagedir, spte->user_vaddr, kpage, true))
  {
    vm_free_frame (kpage);
    return false;
  }

  spte->loaded = true;
  if (spte->type & SWAP)
    spte->type = MMF;

  return true;
}

/* Load a zero page whose details are defined in struct suppl_pte */
static bool
load_page_swap (struct sup_page_entry *spte)
{
  /* Get a page of memory. */
  uint8_t *kpage = vm_get_frame (PAL_USER);
  if (kpage == NULL)
    return false;

  /* Map the user page to given frame */
  if (!pagedir_set_page (thread_current ()->pagedir, spte->user_vaddr, kpage,
                         spte->swap_writable))
  {
    vm_free_frame (kpage);
    return false;
  }

  /* Swap data from disk into memory page */
  vm_swap_in (spte->swap_slot_idx, spte->user_vaddr);

  if (spte->type == SWAP)
  {
    /* After swap in, remove the corresponding entry in suppl page table */
    hash_delete (&thread_current ()->suppl_page_table, &spte->elem);
  }
  if (spte->type == (FILE | SWAP))
  {
    spte->type = FILE;
    spte->loaded = true;
  }

  return true;
}

/* Add an file suplemental page entry to supplemental page table */
bool
suppl_pt_insert_mmf (struct file *file, off_t ofs, uint8_t *upage,
                     uint32_t read_bytes)
{
  struct sup_page_entry *spte;
  struct hash_elem *result;
  struct thread *cur = thread_current ();

  spte = calloc (1, sizeof *spte);

  if (spte == NULL)
    return false;

  spte->user_vaddr = upage;
  spte->type = MMF;
  spte->data.mmf_page.file = file;
  spte->data.mmf_page.offset = ofs;
  spte->data.mmf_page.read_bytes = read_bytes;
  spte->loaded = false;

  result = hash_insert (&cur->suppl_page_table, &spte->elem);
  if (result != NULL)
    return false;

  return true;
}

/* Given a suppl_pte struct spte, write data at address spte->uvaddr to
  * file. It is required if a page is dirty */
//void write_page_back_to_file_wo_lock (struct sup_page_entry *spte)
//{
//  if (spte->type == MMF)
//  {
//    file_seek (spte->data.mmf_page.file, spte->data.mmf_page.offset);
//    file_write (spte->data.mmf_page.file,
//                spte->user_vaddr,
//                spte->data.mmf_page.read_bytes);
//  }
//}
