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
  STACK, 
  SWAP, 
  MMAP
};

struct sup_page_table_entry
{
  void *user_vaddr;
  enum page_status source;
  // uint64_t access_time;
  // bool dirty;
  // bool accessed;
  bool pinned;
  bool read_only;
  struct list_elem elem;
  struct frame_table_entry *fte;
  size_t swap_index;

  /* mmap file */
  struct file *file;
  int file_offset;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;
};

struct sup_page_table_entry *install_page_supplemental (void *upage);
struct sup_page_table_entry *find_in_table (void *upage);
