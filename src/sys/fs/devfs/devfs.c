//
// devfs.c
//
// Device Filesystem
//
// Copyright (C) 2002 Michael Ringgaard. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
// 1. Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.  
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.  
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
// SUCH DAMAGE.
// 

#include <os/krnl.h>

#define DEVROOT ((struct devfile *) NODEV)

static time_t mounttime;

int devfs_open(struct file *filp, char *name);
int devfs_close(struct file *filp);
int devfs_flush(struct file *filp);

int devfs_read(struct file *filp, void *data, size_t size);
int devfs_write(struct file *filp, void *data, size_t size);
int devfs_ioctl(struct file *filp, int cmd, void *data, size_t size);

off64_t devfs_tell(struct file *filp);
off64_t devfs_lseek(struct file *filp, off64_t offset, int origin);

int devfs_fstat(struct file *filp, struct stat64 *buffer);
int devfs_stat(struct fs *fs, char *name, struct stat64 *buffer);

int devfs_opendir(struct file *filp, char *name);
int devfs_readdir(struct file *filp, struct dirent *dirp, int count);

struct fsops devfsops =
{
  FSOP_OPEN | FSOP_CLOSE | FSOP_FLUSH | FSOP_READ | FSOP_WRITE | FSOP_IOCTL | 
  FSOP_TELL | FSOP_LSEEK | FSOP_STAT | FSOP_FSTAT | FSOP_OPENDIR | FSOP_READDIR,

  NULL,
  NULL,

  NULL,
  NULL,
  NULL,

  NULL,

  devfs_open,
  devfs_close,
  devfs_flush,

  devfs_read,
  devfs_write,
  devfs_ioctl,

  devfs_tell,
  devfs_lseek,
  NULL,

  NULL,
  NULL,

  devfs_fstat,
  devfs_stat,

  NULL,
  NULL,

  NULL,
  NULL,

  NULL,
  NULL,
  NULL,

  devfs_opendir,
  devfs_readdir
};

void init_devfs()
{
  register_filesystem("devfs", &devfsops);
  mounttime = get_time();
}

int devfs_open(struct file *filp, char *name)
{
  dev_t devno;
  struct devfile *df;

  if (*name == PS1 || *name == PS2) name++;
  devno = dev_open(name);
  if (devno == NODEV) return -ENOENT;

  df = (struct devfile *) kmalloc(sizeof(struct devfile));
  if (!df)
  {
    dev_close(devno);
    return -EMFILE;
  }

  df->filp = filp;
  df->devno = devno;
  df->devsize = dev_ioctl(devno, IOCTL_GETDEVSIZE, NULL, 0);
  df->blksize = dev_ioctl(devno, IOCTL_GETBLKSIZE, NULL, 0);
  if (df->blksize <= 0) df->blksize = 1;

  df->next = NULL;
  df->prev = device(devno)->files;
  device(devno)->files = df;

  filp->data = df;
  return 0;
}

int devfs_close(struct file *filp)
{
  struct devfile *df = (struct devfile *) filp->data;

  if (filp->data == DEVROOT) return 0;

  if (df->next) df->next->prev = df->prev;
  if (df->prev) df->prev->next = df->next;
  if (device(df->devno)->files == df) device(df->devno)->files = df->next;

  dev_close(df->devno);

  kfree(df);

  return 0;
}

int devfs_flush(struct file *filp)
{
  return 0;
}

int devfs_read(struct file *filp, void *data, size_t size)
{
  struct devfile *df = (struct devfile *) filp->data;
  int read;
  int flags = 0;

  if (df == DEVROOT) return -EBADF;
  if (filp->flags & O_NONBLOCK) flags |= DEVFLAG_NBIO;
  
  read = dev_read(df->devno, data, size, (blkno_t) (filp->pos / df->blksize), flags);
  if (read > 0) filp->pos += read;
  return read;
}

int devfs_write(struct file *filp, void *data, size_t size)
{
  struct devfile *df = (struct devfile *) filp->data;
  int written;
  int flags = 0;

  if (df == DEVROOT) return -EBADF;
  if (filp->flags & O_NONBLOCK) flags |= DEVFLAG_NBIO;
  
  written = dev_write(df->devno, data, size, (blkno_t) (filp->pos / df->blksize), flags);
  if (written > 0) filp->pos += written;
  return written;
}

int devfs_ioctl(struct file *filp, int cmd, void *data, size_t size)
{
  struct devfile *df = (struct devfile *) filp->data;
  int rc;

  if (df == DEVROOT) return -EBADF;

  rc = dev_ioctl(df->devno, cmd, data, size);
  return rc;
}

off64_t devfs_tell(struct file *filp)
{
  return filp->pos;
}

off64_t devfs_lseek(struct file *filp, off64_t offset, int origin)
{
  struct devfile *df = (struct devfile *) filp->data;
  off64_t size;

  if (df == DEVROOT) return -EBADF;
  size = (off64_t) df->devsize * (off64_t) df->blksize;

  switch (origin)
  {
    case SEEK_END:
      offset += size;
      break;

    case SEEK_CUR:
      offset += filp->pos;
  }

  if (offset < 0 || offset > size) return -EINVAL;

  filp->pos = offset;
  return offset;
}

int devfs_fstat(struct file *filp, struct stat64 *buffer)
{
  struct devfile *df = (struct devfile *) filp->data;
  off64_t size;
  struct dev *dev;

  if (df == DEVROOT) return -EBADF;

  dev = device(df->devno);
  if (!dev) return -ENOENT;
  size = (off64_t) df->devsize * (off64_t) df->blksize;

  if (buffer)
  {
    memset(buffer, 0, sizeof(struct stat64));
    if (dev->driver->type == DEV_TYPE_BLOCK) 
      buffer->st_mode = S_IFBLK | S_IREAD | S_IWRITE;
    else if (dev->driver->type == DEV_TYPE_STREAM) 
      buffer->st_mode = S_IFCHR | S_IREAD | S_IWRITE;
    else
      buffer->st_mode = S_IREAD | S_IWRITE;

    buffer->st_ino = df->devno;
    buffer->st_nlink = 1;
    buffer->st_dev = NODEV;

    buffer->st_atime = mounttime;
    buffer->st_mtime = mounttime;
    buffer->st_ctime = mounttime;
  
    buffer->st_size = size;
  }

  return size > 0x7FFFFFFF ? 0x7FFFFFFF : (int) size;
}

int devfs_stat(struct fs *fs, char *name, struct stat64 *buffer)
{
  dev_t devno;
  int devsize;
  int blksize;
  __int64 size;
  struct dev *dev;

  if (*name == PS1 || *name == PS2) name++;

  if (!*name)
  {
    memset(buffer, 0, sizeof(struct stat64));
    buffer->st_mode = S_IFDIR | S_IREAD | S_IWRITE;

    buffer->st_ino = 0;
    buffer->st_nlink = 1;
    buffer->st_dev = NODEV;

    buffer->st_atime = mounttime;
    buffer->st_mtime = mounttime;
    buffer->st_ctime = mounttime;
    buffer->st_size = 0;

    return 0;
  }

  devno = dev_open(name);
  if (devno == NODEV) return -ENOENT;
  dev = device(devno);
  if (!dev) return -ENOENT;

  devsize = dev_ioctl(devno, IOCTL_GETDEVSIZE, NULL, 0);
  blksize = dev_ioctl(devno, IOCTL_GETBLKSIZE, NULL, 0);
  size = (__int64) devsize * (__int64) blksize;

  if (buffer)
  {
    memset(buffer, 0, sizeof(struct stat64));
    if (dev->driver->type == DEV_TYPE_BLOCK) 
      buffer->st_mode = S_IFBLK | S_IREAD | S_IWRITE;
    else if (dev->driver->type == DEV_TYPE_STREAM) 
      buffer->st_mode = S_IFCHR | S_IREAD | S_IWRITE;
    else
      buffer->st_mode = S_IREAD | S_IWRITE;

    buffer->st_ino = devno;
    buffer->st_nlink = 1;
    buffer->st_dev = NODEV;

    buffer->st_atime = mounttime;
    buffer->st_mtime = mounttime;
    buffer->st_ctime = mounttime;
  
    buffer->st_size = size;
  }

  dev_close(devno);
  return size > 0x7FFFFFFF ? 0x7FFFFFFF : (int) size;
}

int devfs_opendir(struct file *filp, char *name)
{
  if (*name == PS1 || *name == PS2) name++;
  if (*name) return -ENOENT;

  filp->data = DEVROOT;
  return 0;
}

int devfs_readdir(struct file *filp, struct dirent *dirp, int count)
{
  dev_t devno;
  struct dev *dev;

  if (filp->data != DEVROOT) return -EBADF;
  if (filp->pos == num_devs) return 0;
  if (filp->pos < 0 || filp->pos > num_devs) return -EINVAL;

  devno = (dev_t) filp->pos;
  dev = device(devno);
  if (!dev) return -ENOENT;

  dirp->ino = (ino_t) devno;
  dirp->namelen = strlen(dev->name);
  dirp->reclen = sizeof(struct dirent) - MAXPATH + dirp->namelen + 1;
  strcpy(dirp->name, dev->name);

  filp->pos++;
  return 1;
}

void devfs_setevt(struct dev *dev, int events)
{
  struct devfile *df = dev->files;

  while (df)
  {
    set_io_event(&df->filp->iob, events);
    df = df->next;
  }
}

void devfs_clrevt(struct dev *dev, int events)
{
  struct devfile *df = dev->files;

  while (df)
  {
    clear_io_event(&df->filp->iob, events);
    df = df->next;
  }
}
