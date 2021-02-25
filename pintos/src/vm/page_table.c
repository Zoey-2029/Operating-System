#include "vm/frame_table.h"

/* allocate a user page with DEFAULT source */
struct sup_page_table_entry *
install_page_supplemental (void *upage)
{
  struct sup_page_table_entry *page_table_entry =
                                       find_in_table (upage);
  if (page_table_entry == NULL)
    {
      page_table_entry = calloc (1, sizeof *page_table_entry);
      if (!page_table_entry)
        return NULL;

      page_table_entry->user_vaddr = upage;
      page_table_entry->source = DEFAULT;
      hash_insert (&thread_current ()->page_table, 
                   &page_table_entry->elem);
      return page_table_entry;
    }
  else
    return page_table_entry;
}

/* find if the sup_page_table_entry with user virtual 
address of upage */
struct sup_page_table_entry *
find_in_table (void *upage)
{
  struct sup_page_table_entry *tmp = calloc (1, sizeof *tmp);
  if (!tmp)
    return NULL;

  tmp->user_vaddr = upage;
  struct hash_elem *e = hash_find (&thread_current ()->page_table, 
                                   &tmp->elem);
  free (tmp);
  if (e == NULL)
    return NULL;

  struct sup_page_table_entry *f
      = hash_entry (e, struct sup_page_table_entry, elem);
  return f;
}


unsigned
page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  struct sup_page_table_entry *f
      = hash_entry (e, struct sup_page_table_entry, elem);
  return (unsigned)f->user_vaddr;
}

bool
page_less_func (const struct hash_elem *a, 
                const struct hash_elem *b, void *aux UNUSED)
{
  struct sup_page_table_entry *f1
      = hash_entry (a, struct sup_page_table_entry, elem);
  struct sup_page_table_entry *f2
      = hash_entry (b, struct sup_page_table_entry, elem);

  return f1->user_vaddr < f2->user_vaddr;
}