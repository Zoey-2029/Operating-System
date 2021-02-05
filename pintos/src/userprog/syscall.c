#include "userprog/syscall.h"
#include <syscall-nr.h>
#include <console.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "devices/input.h"
#define MAX_WRITE_CHUNK 200

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  if (!check_memory_validity(f->esp, 1)) 
    sys_exit(-1);

  // number of args to check validity
  unsigned args = 0;
  switch(*(int *) f->esp) 
  {
    case SYS_HALT:
    {
      args = 0;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);

      sys_halt();
      break;
    }
      
    case SYS_EXIT: 
    {
      args = 1;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);

      int status = *((int *)f->esp + 1);
      sys_exit(status);
      break;
    } 

    case SYS_EXEC:
    {
      break;
    }

    case SYS_WAIT:
    {
      break;
    }
      
    case SYS_CREATE:
    {
      args = 2;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      const char *file = (void *)(*((int *)f->esp + 1));
      unsigned initial_size = *((unsigned *)f->esp + 2);
      f->eax = sys_create(file, initial_size);
      break;
    }
      
    case SYS_REMOVE:
    {
      args = 1;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      const char* file = (void *)(*((int *)f->esp + 1));
      f->eax = sys_remove(file);
      break;
    }

    case SYS_OPEN:
    {
      args = 1;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      const char* file = (void *)(*((int *)f->esp + 1));
      f->eax = sys_open(file);
      break;
    }

    case SYS_FILESIZE:
    {
      args = 1;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      int fd = *((int *)f->esp + 1);
      f->eax = sys_filesize(fd);
      break;
    }
      
    case SYS_READ:
    {
      args = 3;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      int fd = *((int *)f->esp + 1);
      void *buffer = (void *)(*((int *)f->esp + 2));
      unsigned size = *((unsigned *)f->esp + 3);
      f->eax = sys_read(fd, buffer, size);
      break;
    }

    case SYS_WRITE:
    {
      args = 3;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      int fd = *((int *)f->esp + 1);
      const void *buffer = (void *)(*((int *)f->esp + 2));
      unsigned size = *((unsigned *)f->esp + 3);
      f->eax = sys_write(fd, buffer, size);
      break;
    }

    case SYS_SEEK:
    {
      args = 2;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      int fd = *((int *)f->esp + 1);
      unsigned position = *((unsigned *)f->esp + 2);
      sys_seek(fd, position);
      break;
    }
      

    case SYS_TELL:
    {
      args = 1;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      int fd = *((int *)f->esp + 1);
      f->eax = sys_tell(fd);
      break;
    }
      
      
    case SYS_CLOSE:
    {
      args = 1;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      int fd = *((int *)f->esp + 1);
      sys_close(fd);
      break;
    }
      
    default:
    {
      printf("unimplemented system call \n");
      sys_exit(-1);
    }
  }
}

void 
sys_halt() {
  shutdown_power_off();
}

void 
sys_exit (int status) 
{
  printf ("%s: exit(%d)\n", thread_current()->name, status);
  thread_current() ->exit_status = status;
  thread_exit();
}

pid_t 
sys_exec (const char *cmd_line) 
{
  return 0;
}

int 
sys_wait (pid_t pid) 
{

}

bool 
sys_create (const char *file, unsigned initial_size) 
{
  if (!check_memory_validity(file, 1)) {
    sys_exit(-1);
  }
  return filesys_create(file, initial_size);
}

bool 
sys_remove (const char *file) 
{
  if (!check_memory_validity(file, 1)) {
    sys_exit(-1);
  }

  return filesys_remove(file);
}

int 
sys_open (const char *file) 
{
  if (!check_memory_validity(file, 1)) {
    sys_exit(-1);
  }
  struct file *f = filesys_open (file);
  if (f == NULL)
    return -1;
  struct file_info *info = calloc (1, sizeof(struct file_info));
  info->fd = fd_count++;
  info->file = f;
  list_push_back (&thread_current()->file_info_list, &info->elem);

  return info->fd;
}
 
int 
sys_filesize (int fd)
{
  struct file_info *info = find_file_info(fd);

  if (!info) {
    return -1;
  }

  return file_length(info->file);
}

int 
sys_read (int fd, void *buffer, unsigned size) 
{
  if (!check_memory_validity(buffer, 1)) {
    sys_exit(-1);
  }

  unsigned bytes_read = 0;

  if (fd == STDIN_FILENO) {
    while(bytes_read < size) {
      ((char*)buffer)[bytes_read++] = input_getc();
    }
  } else {
    struct file_info *info = find_file_info(fd);

    if (!info) {
      return -1;
    }

    bytes_read = file_read(info->file, buffer, size);
  }
  return bytes_read;
}

int 
sys_write(int fd, const void *buffer, unsigned size) {
  if (!check_memory_validity(buffer, 1))
    sys_exit(-1);

  // write to console
  // break the buffer to 200-byte chunks
  if (fd == STDOUT_FILENO) {
    size_t bytes_written = 0;
    while(bytes_written + MAX_WRITE_CHUNK < size) 
    {
      putbuf((char *)(buffer + bytes_written), MAX_WRITE_CHUNK);
      bytes_written += MAX_WRITE_CHUNK;
    }
    putbuf((char *)(buffer + bytes_written), 
            (size_t)(size - bytes_written));
    return size;
  }
  
  struct file_info *info = find_file_info(fd);

  if (!info) {
    return -1;
  }
  
  return file_write(info->file, buffer, size);
}

void 
sys_seek (int fd, unsigned position)
{
  struct file_info *info = find_file_info (fd);

  if (info) 
  {
     file_seek (info->file, position);
  }
}

unsigned sys_tell (int fd) 
{
  struct file_info *info = find_file_info (fd);

  if (!info)
  {
    return -1;
  }

  return file_tell (info->file);
}

void sys_close (int fd) 
{
  struct file_info *info = find_file_info(fd);

  if (info) {
    list_remove(&info->elem);
  }
}


/* check if the user pointer is valid
   virtual_addr: the first pointer
   size: number of pointers to check validity */
bool
check_memory_validity(const void *virtual_addr, unsigned size) 
{
  for (unsigned i = 0; i < size * sizeof(void *); i++) 
  {
    const void *addr = virtual_addr + i;
    // printf("check_memory_validity: %p\n", addr);
    /* invalid user virtual pointer: 
     NULL pointer, 
     a pointer below the code segment
     a pointer to kernel virtual address space,
     a pointer to unmapped virtual memory,  */
    if (addr == NULL ||
        addr < 0x08048000 || 
        !is_user_vaddr(addr) || 
        !pagedir_get_page(thread_current ()->pagedir, addr)) 
          return false; 
  }
  // printf("check_memory_pass\n");
  return true;
}

static struct file_info* find_file_info(int fd) {
  struct list_elem *e;
  struct list *l = &thread_current()->file_info_list;

  for (e = list_begin (l); e != list_end (l); e = list_next (e)) {
    struct file_info *f = list_entry (e, struct file_info, elem);
    if (f->fd == fd) {
      return f;
    }
  }

  return NULL;
}
