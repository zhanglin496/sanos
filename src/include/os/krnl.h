//
// krnl.h
//
// Copyright (c) 2001 Michael Ringgaard. All rights reserved.
//

#ifndef KRNL_H
#define KRNL_H

#include <os.h>

#include <types.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <bitops.h>
#include <rmap.h>
#include <inifile.h>
#include <moddb.h>

#include <os/tss.h>
#include <os/seg.h>
#include <os/fpu.h>
#include <os/cpu.h>

#include <os/pdir.h>
#include <os/pframe.h>
#include <os/kmem.h>
#include <os/kmalloc.h>
#include <os/vmm.h>

#include <os/syspage.h>

#include <os/pe.h>

#include <os/buf.h>

#include <os/timer.h>
#include <os/object.h>
#include <os/queue.h>
#include <os/sched.h>
#include <os/trap.h>
#include <os/dbg.h>

#include <os/pic.h>
#include <os/pit.h>

#include <os/dev.h>
#include <os/pci.h>
#include <os/pnpbios.h>

#include <os/video.h>
#include <os/kbd.h>

#include <os/vfs.h>
#include <os/dfs.h>
#include <os/devfs.h>
#include <os/procfs.h>

#include <os/mbr.h>

#include <os/pe.h>
#include <os/ldr.h>

#include <os/syscall.h>

#include <net/net.h>

// start.c

extern devno_t bootdev;
extern struct section *krnlcfg;
extern struct peb *peb;

void panic(char *msg);
void exit();
void stop(int restart);

// syscall.c

void init_syscall();

// cons.c

extern devno_t consdev;

krnlapi void kprintf(const char *fmt, ...);

// hd.c

void init_hd();

// fd.c

void init_fd();

// iop.c

void insw(int port, void *buf, int count);
void outsw(int port, void *buf, int count);
void insd(int port, void *buf, int count);
void outsd(int port, void *buf, int count);

// Intrinsic i/o functions

int __cdecl _inp(port_t);
unsigned short __cdecl _inpw(port_t);
unsigned long __cdecl _inpd(port_t);

int __cdecl _outp(port_t, int);
unsigned short __cdecl _outpw(port_t, unsigned short);
unsigned long __cdecl _outpd(port_t, unsigned long);

#endif
