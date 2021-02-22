#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/page_table.h"
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