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
  void *frame;
  struct thread *owner;
  struct sup_page_table_entry *spte;
  struct list_elem elem;
  // Maybe store information for memory mapped files here too?
};

void frame_table_init (void);
struct frame_table_entry *find_in_frame_table (void *kpage);
void *allocate_frame (void);
void free_frame (void *);

void *evict_frame (void);

bool load_page_from_file (struct sup_page_table_entry *spte, void *kpage);
bool load_page_from_stack (struct sup_page_table_entry *entry);
bool load_page_from_swap (struct sup_page_table_entry *spte, void *kpage);
bool load_page_from_mmap (struct sup_page_table_entry *spte, void *kpage);
bool load_page (struct sup_page_table_entry *spte);

void free_single_page (struct sup_page_table_entry *spte);
void free_page_table (void);