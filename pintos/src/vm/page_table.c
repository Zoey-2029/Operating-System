#include "vm/frame_table.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"

bool
install_page_supplemental (void *upage)
{
  struct sup_page_table_entry *page_table_entry = find_in_table(upage);
  if (page_table_entry == NULL){
    page_table_entry = calloc (1, sizeof *page_table_entry);
    page_table_entry->user_vaddr = upage;
    list_push_back (&thread_current ()->sup_page_table, &page_table_entry->elem);
    return true;
  }
  else return false;
}

struct sup_page_table_entry *
find_in_table (void *upage)
{

  struct list_elem *e;
  struct list *l = &thread_current ()->sup_page_table;

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


struct sup_page_table_entry * allocate_page_from_file(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
  {
  ASSERT(pg_ofs(upage) == 0);

	struct sup_page_table_entry *spte = (struct sub_page_table_entry *) malloc(sizeof(struct sup_page_table_entry));
	if (spte == NULL)
		return NULL;

	spte->user_vaddr = upage;
	spte->source = FILE;
	//spte->map_id = -1;
	spte->read_only = !writable;
	spte->loaded = false;
	spte->file = file;
	spte->offset = ofs;
	spte->read_bytes = read_bytes;
	spte->zero_bytes = zero_bytes;
	return spte;
  }


bool load_page_from_file(struct sup_page_table_entry* spte){
  ASSERT(spte!=NULL && spte->source==FILE);

  if (spte->loaded) return true;

  struct frame_table_entry *fte = allocate_frame();
  if (fte->frame==NULL) return false;
  fte->spte = spte;
  
  if (spte->read_bytes > 0)
  {
    lock_acquire(&filesys_lock);;
    off_t bytes_read = file_read_at(spte->file, fte->frame, spte->read_bytes, spte->offset);
    lock_release(&filesys_lock);
    //printf("bytes_read and read_bytes: %d and %d\n", bytes_read, spte->read_bytes);
    if (bytes_read != spte->read_bytes){
      free_frame_by_fte(fte);
      printf("Cannot read as much as expected\n");
      return false;
    }
    //memset(frame + spte->read_bytes, 0, spte->zero_bytes);
  }
  if (!(pagedir_get_page (thread_current()->pagedir, spte->user_vaddr) == NULL
        && pagedir_set_page (thread_current()->pagedir, spte->user_vaddr, fte->frame, !spte->read_only))
        && install_page_supplemental(spte->user_vaddr)){
    free_frame_by_fte(fte);
    
    return false;
  }
  
  spte->loaded = true;
  return true;
}

bool load_page_from_stack(struct sup_page_table_entry* spte);
bool load_page_from_swap(struct sup_page_table_entry* spte);
bool load_page_from_mmap(struct sup_page_table_entry* spte);