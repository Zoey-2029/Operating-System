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
#include "process.h"
#define MAX_WRITE_CHUNK 200

static int FD_COUNT = STDOUT_FILENO + 1;
static void syscall_handler (struct intr_frame *);
static struct file_info* find_file_info(int fd);
static struct lock filesys_lock;
static void free_file_info (void);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&filesys_lock);
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
      args = 1;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);

      const char *cmd_line = (void *)(*((int *)f->esp + 1));
      f->eax = sys_exec(cmd_line);
      break;
    }

    case SYS_WAIT:
    {
      args = 1;
      if (!check_memory_validity((int *)f->esp + 1, args)) 
        sys_exit(-1);

      pid_t pid = *((pid_t *)f->esp + 1);
      f->eax = sys_wait (pid);
      break;
    }
      
    case SYS_CREATE:
    {
      args = 2;
      if (!check_memory_validity ((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      const char *file = (void *)(*((int *)f->esp + 1));
      unsigned initial_size = *((unsigned *)f->esp + 2);
      f->eax = sys_create (file, initial_size);
      break;
    }
      
    case SYS_REMOVE:
    {
      args = 1;
      if (!check_memory_validity ((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      const char* file = (void *)(*((int *)f->esp + 1));
      f->eax = sys_remove (file);
      break;
    }

    case SYS_OPEN:
    {
      args = 1;
      if (!check_memory_validity ((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      const char* file = (void *)(*((int *)f->esp + 1));
      f->eax = sys_open (file);
      break;
    }

    case SYS_FILESIZE:
    {
      args = 1;
      if (!check_memory_validity ((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      int fd = *((int *)f->esp + 1);
      f->eax = sys_filesize (fd);
      break;
    }
      
    case SYS_READ:
    {
      args = 3;
      if (!check_memory_validity ((int *)f->esp + 1, args)) 
        sys_exit(-1);
      
      int fd = *((int *)f->esp + 1);
      void *buffer = (void *)(*((int *)f->esp + 2));
      unsigned size = *((unsigned *)f->esp + 3);
      f->eax = sys_read (fd, buffer, size);
      break;
    }

    case SYS_WRITE:
    {
      args = 3;
      if (!check_memory_validity ((int *)f->esp + 1, args)) 
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
      if (!check_memory_validity ((int *)f->esp + 1, args)) 
        sys_exit (-1);
      
      int fd = *((int *)f->esp + 1);
      unsigned position = *((unsigned *)f->esp + 2);
      sys_seek(fd, position);
      break;
    }
      

    case SYS_TELL:
    {
      args = 1;
      if (!check_memory_validity ((int *)f->esp + 1, args)) 
        sys_exit (-1);
      
      int fd = *((int *)f->esp + 1);
      f->eax = sys_tell (fd);
      break;
    }
      
      
    case SYS_CLOSE:
    {
      args = 1;
      if (!check_memory_validity ((int *)f->esp + 1, args)) 
        sys_exit (-1);
      
      int fd = *((int *)f->esp + 1);
      sys_close (fd);
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
  struct thread_info *info = get_child_process(thread_current()->tid, &thread_current()->parent->child_processes);
  info->exit_status = status;
  // printf("about to thread_exit\n");
  free_file_info();
  struct list_elem *e;
  struct list *l = &thread_current()->child_processes;

  for (e = list_begin(l); e != list_end(l); )
  {
    struct thread_info *f = list_entry(e, struct thread_info, elem);
    e = list_next(e);
    if (f) free(f);
  }
  thread_exit();
}

pid_t 
sys_exec (const char *cmd_line) 
{
  //struct thread* current = thread_current();
  if (!check_memory_validity (cmd_line, 1))
  {
    sys_exit(-1);
  }
  pid_t child_pid = process_execute(cmd_line);
  // printf("before sema down\n");
  sema_down(&thread_current()->sema_exec);
  /* Get child process from pid*/
  // printf("child process %d parent %d\n", child_pid, thread_current()->tid);
  struct thread_info *child_process = get_child_process(child_pid, &thread_current()->child_processes);
  // printf("child process %p\n", child_process);
  /* Wait for child process loading*/

  /* Return -1 if loading faild, otherwise return pid*/
  // printf("get status back %d\n", child_process->load_status);
  if (child_process->load_status) return child_pid;
  else return -1;
}

int 
sys_wait (pid_t pid) 
{
  struct thread_info *child_process = get_child_process(pid, &thread_current()->child_processes);

  if (!child_process) {
    return -1;
  }
  // printf("about to wait %d\n", pid);
  int wait_result = process_wait(pid);
  if (wait_result != -1)
    list_remove(&child_process->elem);

  return wait_result;
}

bool 
sys_create (const char *file, unsigned initial_size) 
{
  if (!check_memory_validity (file, 1))
  {
    sys_exit(-1);
  }
  lock_acquire (&filesys_lock);
  bool success = filesys_create (file, initial_size);
  lock_release (&filesys_lock);

  return success;
}

bool 
sys_remove (const char *file) 
{
  if (!check_memory_validity (file, 1)) 
  {
    sys_exit(-1);
  }

  lock_acquire (&filesys_lock);
  bool success = filesys_remove (file);
  lock_release (&filesys_lock);

  return success;
}

int 
sys_open (const char *file) 
{
  if (!check_memory_validity (file, 1)) {
    sys_exit(-1);
  }
 
  struct file *f = filesys_open (file);
  if (f == NULL)
  {
    return -1;
  }
  lock_acquire (&filesys_lock);
  struct file_info *info = calloc (1, sizeof (struct file_info));
  info->fd = FD_COUNT++;
  info->file = f;
  list_push_back (&thread_current ()->file_info_list, &info->elem);
  lock_release (&filesys_lock);
  return info->fd;
}
 
int 
sys_filesize (int fd)
{
  struct file_info *info = find_file_info(fd);
  lock_acquire (&filesys_lock);
  if (!info) 
  {
    return -1;
  }
  int size = file_length (info->file);
  lock_release (&filesys_lock);
  return size;
}

int 
sys_read (int fd, void *buffer, unsigned size) 
{
  if (!check_memory_validity (buffer, 1)) 
  {
    sys_exit(-1);
  }

  unsigned bytes_read = 0;

  if (fd == STDIN_FILENO) 
  {
    while(bytes_read < size) 
    {
      ((char*)buffer)[bytes_read++] = input_getc ();
    }
  } else 
  {
    
    struct file_info *info = find_file_info (fd);

    if (!info) 
    {
      return -1;
    }
    lock_acquire (&filesys_lock);
    // printf("start read\n");
    bytes_read = file_read (info->file, buffer, size);
    // printf("end read\n");
    lock_release (&filesys_lock);
  }
  return bytes_read;
}

int 
sys_write(int fd, const void *buffer, unsigned size) {
  if (!check_memory_validity(buffer, 1))
    sys_exit(-1);

  // write to console
  // break the buffer to 200-byte chunks
  if (fd == STDOUT_FILENO) 
  {
    size_t bytes_written = 0;
    while (bytes_written + MAX_WRITE_CHUNK < size) 
    {
      putbuf ((char *)(buffer + bytes_written), MAX_WRITE_CHUNK);
      bytes_written += MAX_WRITE_CHUNK;
    }
    putbuf ((char *)(buffer + bytes_written), 
            (size_t)(size - bytes_written));
    return size;
  }
  
  struct file_info *info = find_file_info(fd);

  if (!info) 
  {
    return -1;
  }
  lock_acquire (&filesys_lock);
  off_t bytes_written = file_write (info->file, buffer, size);
  lock_release (&filesys_lock);
  return bytes_written;
}

void 
sys_seek (int fd, unsigned position)
{
  struct file_info *info = find_file_info (fd);
  lock_acquire (&filesys_lock);
  if (info) 
  {
     file_seek (info->file, position);
  }
  lock_release (&filesys_lock);
}

unsigned sys_tell (int fd) 
{

  struct file_info *info = find_file_info (fd);

  if (!info)
  {
    return -1;
  }
  lock_acquire (&filesys_lock);
  off_t position = file_tell (info->file);
  lock_release (&filesys_lock);

  return position;
}

void sys_close (int fd) 
{
  
  struct file_info *info = find_file_info(fd);
  lock_acquire (&filesys_lock);
  if (info) 
  {
    file_close(info->file);
    list_remove(&info->elem);
    free(info);
  }
  lock_release (&filesys_lock);
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
        addr < (void*) 0x08048000 || 
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
      // printf("found\n");
      return f;
    }
  }

  return NULL;
}

static void free_file_info() {
  struct list_elem *e;
  struct list *l = &thread_current()->file_info_list;
  // printf("%p\n", l);
  for (e = list_begin (l); e != list_end (l); ) {
    struct file_info *f = list_entry (e, struct file_info, elem);
    e = list_next (e);
    if (f) free(f);
  }
}
