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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/cdev.h>
#include <linux/io.h>

#include <linux/uaccess.h>

#include "llram_driver.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arm Ltd");
MODULE_DESCRIPTION("Example driver for the LLRAM port");
MODULE_VERSION("0.3");

// Module config
static bool enable_llram = 0;
static bool allow_simulated = 1;
module_param(enable_llram, bool, 0644);
MODULE_PARM_DESC(enable_llram,
                 "Driver should attempt to enable the LLRAM port via IMP_LLRAMREGIONR_EL1");
module_param(allow_simulated, bool, 0644);
MODULE_PARM_DESC(allow_simulated,
                 "Driver should allocate memory if the port is not present");

// LLRAM is 256MB
#define LLRAM_SIZE (256*1024*1024)
// The ID mask does not compare Variant and Revision, as all revisions that can
// run Linux are supported.
#define ID_MASK  0xff0fffe0
#define ID_VALUE 0x410fd140
#define LLRAM_IMPL_MASK 0x80
#define LLRAM_BASE_MASK 0xFFF0000000
#define LLRAM_EN_MASK   0x1

// LLRAM device information
static int llram_open_count = 0;
static unsigned int llram_device_major = 0;
static struct cdev llram_cdev;
static struct class *llram_class = NULL;

// For testing, an simulated memory can be provided, pointing to a 256MB
// buffer. Otherwise this will point to a mapping to the LLRAM.
static unsigned char *llram_pointer = NULL;
static int pointer_is_to_llram_port = 1;

int llram_addr_in_range(int ptr) {
  if ((ptr >= 0) && (ptr < LLRAM_SIZE)) {
    return 1;
  } else {
    printk(KERN_WARNING "[llram] Out of range pointer %x\n", ptr);
    return 0;
  }
}

//---------------------------
// Access functions
//---------------------------
ssize_t llram_read(struct file *f, char *buf, size_t len, loff_t *offset) {
  ssize_t size_read = len;
  ssize_t i;
  int zero_count = 0;
  loff_t zero_addr;
  printk(KERN_DEBUG "[llram] Requested read of %ld byte(s)", len);

  // If out of range, no bytes to read
  if (!llram_addr_in_range(*offset)) {
    printk(KERN_WARNING "[llram] Address 0x%07llx out of range", *offset);
    return 0;
  }
  if (!llram_addr_in_range(*offset + len)) {
    size_read = LLRAM_SIZE - *offset;
    printk(KERN_DEBUG "[llram] Past end of LLRAM region so reading %ld byte(s)", size_read);
  }
  if (copy_to_user(buf, llram_pointer+*offset, size_read)) {
    printk(KERN_WARNING "[llram] Fault on copying data");
    return -EFAULT;
  }

  for (i = 0; i < size_read; i++) {
    loff_t addr = i + *offset;
    unsigned char data = llram_pointer[addr];
    if (data == 0) {
      if (zero_count == 0) zero_addr = addr;
      zero_count++;
    } else {
      if (zero_count != 0) {
        printk(KERN_DEBUG "[llram] RD [0x%07llx] = 0x00 (x%d)", zero_addr, zero_count);
        zero_count = 0;
      }
      printk(KERN_DEBUG "[llram] RD [0x%07llx] = %02x", addr, data);
    }
  }
  if (zero_count != 0) {
    printk(KERN_DEBUG "[llram] RD [0x%07llx] = 0x00 (x%d)", zero_addr, zero_count);
    zero_count = 0;
  }
  *offset += size_read;
  return size_read;
}

ssize_t llram_write(struct file *f, const char *buf, size_t len, loff_t *offset) {
  ssize_t i;
  printk(KERN_DEBUG "[llram] Requested write of %ld byte(s)", len);

  // If out of range, fault as cannot write
  if (len) {
    if (!llram_addr_in_range(*offset) || !llram_addr_in_range(*offset+len-1)) {
      printk(KERN_WARNING "[llram] Address 0x%07llx -> 0x%07llx out of range", *offset, *offset+len);
      return -EFAULT;
    }
  } else {
    if (!llram_addr_in_range(*offset)) {
      printk(KERN_WARNING "[llram] Address 0x%07llx out of range", *offset);
      return -EFAULT;
    }
  }

  if (copy_from_user(llram_pointer+*offset, buf, len)) {
    printk(KERN_WARNING "[llram] Fault on copying data");
    return -EFAULT;
  }

  for (i = 0; i < len; i++) {
    loff_t addr = i + *offset;
    printk(KERN_DEBUG "[llram] WR [0x%07llx] = %02x", addr, llram_pointer[addr]);
  }

  *offset += len;
  return len;
}

int llram_open(struct inode *inode, struct file *f) {
  if (llram_open_count) {
    printk(KERN_WARNING "[llram] LLRAM device already opened\n");
    return -EBUSY;
  }
  llram_open_count++;
  // Register that this module is in use
  try_module_get(THIS_MODULE);
  return 0;
}

int llram_release(struct inode *inode, struct file *f) {
  // Decrease current usage count and indicate module no longer in use
  llram_open_count--;
  module_put(THIS_MODULE);
  return 0;
}

loff_t llram_llseek(struct file *f, loff_t off, int whence) {
  loff_t new_offset;

  switch (whence) {
    case 0: // SEEK_SET
      new_offset = off;
      break;
    case 1: // SEEK_CUR
      new_offset = f->f_pos + off;
      break;
    case 2: // SEEK_END
      new_offset = LLRAM_SIZE + off;
      break;
    default:
      return -EINVAL;
  }
  if (!llram_addr_in_range(new_offset)) {
    return -EINVAL;
  }
  printk(KERN_INFO "[llram] Set offset to 0x%llx\n", new_offset);
  f->f_pos = new_offset;
  return new_offset;
}

// Structure to point to file operation functions
static struct file_operations llram_file_ops = {
 .owner = THIS_MODULE,
 .read = llram_read,
 .write = llram_write,
 .open = llram_open,
 .release = llram_release,
 .llseek = llram_llseek
};

//---------------------------
// Assembly support code
//---------------------------
#ifdef CONFIG_ARM64
uint64_t get_midr_el1() {
  uint64_t midr_el1 = 0;
  __asm volatile("mrs %[result], midr_el1": [result] "=r" (midr_el1));
  return midr_el1;
}

uint64_t get_imp_clustercfr_el1() {
  uint64_t imp_clustercfr_el1 = 0;
  __asm volatile("mrs %[result], s3_0_c15_c3_0": [result] "=r" (imp_clustercfr_el1));
  return imp_clustercfr_el1;
}

uint64_t get_imp_llramregionr_el1() {
  uint64_t imp_llramregionr_el1 = 0;
  __asm volatile("mrs %[result], s3_0_c15_c0_4": [result] "=r" (imp_llramregionr_el1));
  return imp_llramregionr_el1;
}

void enable_llram() {
  uint64_t enable = 1;
  __asm volatile("msr s3_0_c15_c0_4, %[wr]": [wr] "+r" (enable));
}
#endif

//---------------------------
// Init/Deinit code
//---------------------------
int allocate_memory(void) {
  llram_pointer = (unsigned char*)vmalloc(LLRAM_SIZE);
  if (llram_pointer == NULL) {
    printk(KERN_WARNING "[llram] Cannot allocate memory\n");
    return -ENOMEM;
  }
  pointer_is_to_llram_port = 0;
  return 0;
}

static int __init llram_driver_init(void) {
  int ret = 0;
  printk(KERN_INFO "[llram] Initializing LLRAM driver\n");
  printk(KERN_INFO "[llram]   enable_llram=%d, allow_simulated=%d\n", enable_llram, allow_simulated);
#ifdef CONFIG_ARM64
  if ((get_midr_el1() & ID_MASK) == ID_VALUE) {
    if (get_imp_clustercfr_el1() & LLRAM_IMPL_MASK) {
      if (enable_llram) {
        enable_llram();
      }
      // LLRAM only supports write-through but specifiying WB here means it is treated as cacheable
      llram_pointer = (unsigned char*)memremap(get_imp_llramregionr_el1() & LLRAM_BASE_MASK, LLRAM_SIZE, MEMREMAP_WB);
    } else {
      printk(KERN_WARNING "[llram] LLRAM not implemented\n");
      ret = -1;
    }
  } else {
    printk(KERN_WARNING "[llram] Unexpected ID value\n");
    ret = -1;
  }
  if (ret == -1 & allow_simulated) {
    printk(KERN_WARNING "[llram] LLRAM not present, allocating memory instead\n");
    ret = allocate_memory();
  }
#else
  if (allow_simulated) {
    printk(KERN_WARNING "[llram] Not on ARM64, allocating memory instead\n");
    ret = allocate_memory();
  } else {
    printk(KERN_WARNING "[llram] Not on ARM64, and not allocating memory\n");
    ret = -1;
  }
#endif
  if (ret < 0) {
    return ret;
  }
  ret = llram_create_device();
  if (ret < 0) {
    return ret;
  }
  return ret;
}

static void __exit llram_driver_exit(void) {
  printk(KERN_INFO "[llram] Unloading LLRAM driver\n");
  if (llram_device_major) {
    device_destroy(llram_class, MKDEV(llram_device_major, 0));
    cdev_del(&llram_cdev);
    unregister_chrdev_region(MKDEV(llram_device_major, 0), 1);
  }
  if (llram_class != NULL) {
    class_destroy(llram_class);
  }
  if (llram_pointer != NULL) {
    if (!pointer_is_to_llram_port) {
      vfree(llram_pointer);
    } else {
      memunmap(llram_pointer);
    }
  }
}

//---------------------------
// Device management
//---------------------------
static char *llram_devnode(struct device *dev, umode_t *mode) {
  if (mode && (dev->devt == MKDEV(llram_device_major, 0))) {
    *mode = 0666;
  }

  return NULL;
}

int llram_create_device() {
  int ret = 0;
  dev_t dev = 0;
  struct device *device = NULL;

  ret = alloc_chrdev_region(&dev, 0, 1, "llram");
  if (ret < 0) {
    printk(KERN_WARNING "[llram] Failed to allocate a character device\n");
    return ret;
  }
  llram_device_major = MAJOR(dev);
  printk(KERN_INFO "[llram] Allocated character device %d\n", llram_device_major);

  llram_class = class_create(THIS_MODULE, "llram");
  if (IS_ERR(llram_class)) {
    ret = PTR_ERR(llram_class);
    return ret;
  }

  llram_class->devnode = llram_devnode;

  cdev_init(&llram_cdev, &llram_file_ops);

  ret = cdev_add(&llram_cdev, dev, 1);
  if (ret) {
    printk(KERN_WARNING "[llram] Error %d on adding character device\n", ret);
    return ret;
  }

  device = device_create(llram_class, NULL, dev, NULL, "llram");
  if (IS_ERR(device)) {
    ret = PTR_ERR(device);
    printk(KERN_WARNING "[llram] Error %d on creating device\n", ret);
    return ret;
  }

  return ret;
}

module_init(llram_driver_init);
module_exit(llram_driver_exit);
