#include "vm/page_table.h"

bool
install_page_supplemental (void *upage)
{
  struct sup_page_table_entry *page_table_entry = find_in_table(upage);
  if (page_table_entry == NULL){
    page_table_entry = calloc (1, sizeof *page_table_entry);
    page_table_entry->user_vaddr = upage;
    list_push_back (&thread_current ()->page_table, &page_table_entry->elem);
    return true;
  }
  else return false;
}

struct sup_page_table_entry *
find_in_table (void *upage)
{

  struct list_elem *e;
  struct list *l = &thread_current ()->page_table;

  for (e = list_begin (l); e != list_end (l); e = list_next (e))
    {
      struct sup_page_table_entry *f = list_entry (e, struct sup_page_table_entry, elem);
      if (f->user_vaddr == upage)
        {
          return f;
        }
    }

  return NULL;
}

bool load_page_from_file(struct sup_page_table_entry* entry){
  ASSERT(entry!=NULL && entry->source==FILE);
  //uint32_t *frame = allocate_frame(spte, spte->zero_bytes == PGSIZE);
}

bool load_page_from_stack(struct sup_page_table_entry* entry);
bool load_page_from_swap(struct sup_page_table_entry* entry);
bool load_page_from_mmap(struct sup_page_table_entry* entry);