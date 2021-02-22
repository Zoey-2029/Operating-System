#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/page_table.h"
#include "userprog/process.h"
#include <list.h>

struct frame_table_entry
{
  void *frame;
  struct thread *owner;
  struct sup_page_table_entry *spte;
  struct list_elem elem;
  // Maybe store information for memory mapped files here too?
};

struct frame_table_entry *find_in_frame_table (void *kpage);
void *allocate_frame (void);
void free_frame (void *);
void frame_table_init (void);

void *evict_frame (void);

bool load_page_from_file (struct sup_page_table_entry *entry);
bool load_page_from_stack (struct sup_page_table_entry *entry);
bool load_page_from_swap (struct sup_page_table_entry *entry);
bool load_page_from_mmap (struct sup_page_table_entry *spte);
void free_page_table (void);
void free_single_page (struct sup_page_table_entry *spte);