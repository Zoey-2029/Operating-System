#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#define MAX_FILE_SIZE 14

typedef int pid_t;

void syscall_init (void);

/* file_info struct keeps track of the opened file and its fd. */
struct file_info
{
  int fd;
  struct list_elem elem;
  struct file *file;
};

/* System call handlers. */
void sys_halt (void);
void sys_exit (int status);
pid_t sys_exec (const char *cmd_line);
int sys_wait (pid_t pid);
bool sys_create (const char *file, unsigned initial_size);
bool sys_remove (const char *file);
int sys_open (const char *file);
int sys_filesize (int fd);
int sys_read (int fd, void *buffer, unsigned size);
int sys_write (int fd, const void *buffer, unsigned size);
void sys_seek (int fd, unsigned position);
unsigned sys_tell (int fd);
void sys_close (int fd);
mapid_t sys_mmap (int fd, void *addr);
void sys_munmap (mapid_t mapid);

bool grow_stack (const void *fault_addr);

#endif /* userprog/syscall.h */
