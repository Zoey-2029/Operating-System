#include "threads/malloc.h"
#include "threads/thread.h"
#include <list.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum page_status
{
  DEFAULT,
  FILE, 
  SWAP, 
  MMAP
};

struct sup_page_table_entry
{
  void *user_vaddr;
  enum page_status source;
  bool writable;
  struct frame_table_entry *fte;
  struct list_elem elem;

  /* locate swap */
  bool pinned;
  size_t swap_index;

  /* locate mmap file */
  struct file *file;
  int file_offset;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  
};

struct sup_page_table_entry *install_page_supplemental (void *upage);
struct sup_page_table_entry *find_in_table (void *upage);
