#include "threads/malloc.h"
#include "threads/thread.h"
#include <list.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


struct sup_page_table_entry
{
  void *user_vaddr;
  uint64_t access_time;
  bool dirty;
  bool accessed;
  bool read_only;
  struct list_elem elem;
};

struct sup_page_table_entry *install_page_supplemental (void *upage);
struct sup_page_table_entry *find_in_table (void *upage);