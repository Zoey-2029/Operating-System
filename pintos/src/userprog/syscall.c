#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <console.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

#define MAX_WRITE_CHUNK 200


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
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
      break;
    }
      
    case SYS_REMOVE:
    {
      break;
    }

    case SYS_OPEN:
    {
      break;
    }

    case SYS_FILESIZE:
    {
      break;
    }
      
    case SYS_READ:
    {
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
      break;
    }
      

    case SYS_TELL:
    {
      break;
    }
      
      
    case SYS_CLOSE:
    {
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

}

bool 
sys_remove (const char *file) 
{

}

int 
sys_open (const char *file) 
{
  return 0;
}
 
int 
sys_filesize (int fd)
{
  return 0;
}

int 
sys_read (int fd, const void *buffer, unsigned size) 
{
  return 0;
}

int 
sys_write(int fd, const void *buffer, unsigned size) {
  if (!check_memory_validity(buffer, size))
    return 0;

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
  return 0;
}

void 
sys_seek (int fd, unsigned position)
{

}

unsigned sys_tell (int fd) 
{
  return 0;
}

void sys_close (int fd) 
{

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
  return true;
}
