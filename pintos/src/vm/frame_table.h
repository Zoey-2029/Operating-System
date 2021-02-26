#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/page_table.h"
#include <list.h>
#include <string.h>

struct frame_table_entry
{
  void *frame;                        /* kernel address */
  struct thread *owner;               /* owner thread of frame*/
  struct sup_page_table_entry *spte;  /* corresponding page */
  struct list_elem elem;              /* list element for frame table */ 
};

void frame_table_init (void);
struct frame_table_entry *find_in_frame_table (void *kpage);
void *allocate_frame (void);
void free_frame (void *);
void *evict_frame (void);

bool load_page_from_file (struct sup_page_table_entry *spte, void *kpage);
bool load_page_from_swap (struct sup_page_table_entry *spte, void *kpage);
bool load_page (struct sup_page_table_entry *spte);

void free_single_page (struct hash_elem *e, void *aux);
void free_page_table (void);

bool install_page (void *upage, void *kpage, bool writable);

void lock_acquire_vm (void);
void lock_release_vm (void);