#include "vm/page_table.h"

struct sup_page_table_entry *
install_page_supplemental (void *upage)
{
  // printf("install_page_supplemental %p\n", upage);
  struct sup_page_table_entry *page_table_entry = find_in_table (upage);
  if (page_table_entry == NULL)
    {
      page_table_entry = calloc (1, sizeof *page_table_entry);
      page_table_entry->user_vaddr = upage;
      list_push_back (&thread_current ()->page_table, &page_table_entry->elem);
      return page_table_entry;
    }
  else {
    // printf("asdasd\n");
    return page_table_entry;
  }
}

struct sup_page_table_entry *
find_in_table (void *upage)
{

  struct list_elem *e;
  struct list *l = &thread_current ()->page_table;

  for (e = list_begin (l); e != list_end (l); e = list_next (e))
    {
      struct sup_page_table_entry *f
          = list_entry (e, struct sup_page_table_entry, elem);
      if (f->user_vaddr == upage)
        {
          return f;
        }
    }

  return NULL;
}
