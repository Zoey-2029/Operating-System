// #include "vm/page_table.h"
#include "vm/frame_table.h"

struct sup_page_table_entry *
install_page_supplemental (void *upage)
{
  // printf ("%d install_page_supplemental %p\n", thread_current ()->tid,
  // upage);

  struct sup_page_table_entry *page_table_entry = find_in_table (upage);
  lock_acquire_vm ();
  if (page_table_entry == NULL)
    {
      page_table_entry = calloc (1, sizeof *page_table_entry);
      page_table_entry->user_vaddr = upage;
      list_push_back (&thread_current ()->page_table, &page_table_entry->elem);
      lock_release_vm ();
      return page_table_entry;
    }
  else
    {
      // printf("asdasd\n");
      lock_release_vm ();
      return page_table_entry;
    }
}

struct sup_page_table_entry *
find_in_table (void *upage)
{
  lock_acquire_vm ();
  struct list_elem *e;
  struct list *l = &thread_current ()->page_table;

  for (e = list_begin (l); e != list_end (l); e = list_next (e))
    {
      struct sup_page_table_entry *f
          = list_entry (e, struct sup_page_table_entry, elem);
      if (f->user_vaddr == upage)
        {
          lock_release_vm ();
          return f;
        }
    }
  lock_release_vm ();
  return NULL;
}
