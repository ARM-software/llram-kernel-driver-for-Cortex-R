//-----------------------------------------------------------------------------
// llram-kernel-driver-for-Cortex-R
// Copyright (C) 2022  Arm Limited. All rights reserved.
//
// SPDX-License-Identifier: GPL-2.0-only
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software  Foundation; version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//-----------------------------------------------------------------------------

#ifndef _LLRAM_DRIVER_H_
#define _LLRAM_DRIVER_H_

#include <linux/fs.h>

ssize_t llram_read(struct file *f, char *buf, size_t len, loff_t *offset);
ssize_t llram_write(struct file *f, const char *buf, size_t len, loff_t *offset);
int llram_open(struct inode *inode, struct file *f);
int llram_release(struct inode *inode, struct file *f);
int llram_emulated_init(void);
int llram_create_device(void);

#ifdef CONFIG_ARM64
uint64_t get_midr_el1(void);
uint64_t get_imp_clustercfr_el1(void);
uint64_t get_imp_llramregionr_el1(void);
void enable_llram(void);
#endif

#endif /*_LLRAM_DRIVER_H_*/
