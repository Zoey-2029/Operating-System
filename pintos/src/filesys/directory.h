#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include "devices/block.h"
#include "filesys/off_t.h"
#include <stdbool.h>
#include <stddef.h>

/* Maximum length of a file name component.
   This is the traditional UNIX maximum length.
   After directories are implemented, this maximum length may be
   retained, but much longer full path names must be allowed. */
#define NAME_MAX 30

struct inode;

/* Opening and closing directories. */
bool dir_create (block_sector_t sector, size_t entry_cnt);
struct dir *dir_open (struct inode *);
struct dir *dir_open_root (void);
struct dir *dir_reopen (struct dir *);
void dir_close (struct dir *);
struct inode *dir_get_inode (struct dir *);

/* Reading and writing. */
bool dir_lookup (const struct dir *, const char *name, struct inode **);
bool dir_add (struct dir *, const char *name, block_sector_t, bool);
bool dir_remove (struct dir *, const char *name);
bool dir_readdir (struct dir *, char name[NAME_MAX + 1]);
struct dir *dir_open_from_path (const char *name);
struct dir *get_dir_from_path (const char *name);
char *get_file_name_from_path (const char *name);
bool create_entry (const char *name, block_sector_t inode_sector,
                   block_sector_t inumber);
bool check_is_dir (const char *name);
#endif /* filesys/directory.h */
