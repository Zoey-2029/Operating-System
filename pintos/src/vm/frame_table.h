#ifndef FRAME_TABLE_H
#define FRAME_TABLE_H

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

//void *allocate_frame (void);
//void free_frame (void *);
void frame_table_init(void);

struct frame_table_entry *allocate_frame (void);
void free_frame_by_fte (struct frame_table_entry *);
void free_frame (void *);

struct frame_table_entry *evict_frame(void);

#endif /* vm/frame_table.h */