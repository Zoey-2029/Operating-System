#include "vm/frame_table.h"

/* A list of frame_table_entry as the frame table. */
static struct list frame_table;
static struct lock f_lock;

void
frame_table_init ()
{
  lock_init (&f_lock);
  list_init (&frame_table);
}

void *
allocate_frame ()
{
  lock_acquire (&f_lock);
  struct thread *cur = thread_current ();
  void *kpage = (void *)palloc_get_page (PAL_USER | PAL_ZERO);

  /*If cannot get a page, evict one*/
  if (kpage == NULL){
    struct frame_table_entry *victim = evict_frame();
    //size_t index = swap_out(victim->frame);
		//victim->spte->loaded = false;
		//victim->spte->source = SWAP;
		//victim->spte->index = index;
		pagedir_clear_page(victim->owner->pagedir, victim->spte->user_vaddr);

    victim->owner = cur;
    free(victim->spte);
    //victim->spte = NULL;
    list_push_back (&frame_table, &victim->elem);
  }
  else{
    struct frame_table_entry *frame_table_entry
        = calloc (1, sizeof *frame_table_entry);
    frame_table_entry->owner = cur;
    frame_table_entry->frame = kpage;
    //frame_table_entry->spte = NULL;
    list_push_back (&frame_table, &frame_table_entry->elem);
  }
  lock_release (&f_lock);
  return kpage;
}

void
free_frame (void *kpage)
{
  lock_acquire (&f_lock);
  struct list_elem *e;

  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
    {
      struct frame_table_entry *entry
          = list_entry (e, struct frame_table_entry, elem);
      if (entry->frame == kpage)
        {
          palloc_free_page (kpage);
          list_remove (e);
          free (entry);
          break;
        }
    }

  lock_release (&f_lock);
}

struct frame_table_entry *evict_frame(void){
  /*
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
    {
      struct frame_table_entry *entry
      = list_entry (e, struct frame_table_entry, elem);
    }
  */
  struct thread *curr = thread_current();
  while (!list_empty (&frame_table))
  {
    struct list_elem *e = list_pop_front (&frame_table);
    struct frame_table_entry *fte = list_entry (e, struct frame_table_entry, elem);
    if (pagedir_is_accessed(curr->pagedir, fte->spte->user_vaddr)){
      pagedir_set_accessed(curr->pagedir, fte->spte->user_vaddr, false);
      list_push_back (&frame_table, &fte->elem);
    }
    else return fte;
  }
}