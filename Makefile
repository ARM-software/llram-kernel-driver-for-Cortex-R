#-----------------------------------------------------------------------------
# LLRAM Kernel Driver
# Copyright (C) 2022  Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0-only
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software  Foundation; version 2.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#-----------------------------------------------------------------------------

# User configuration
# The KERNEL_PATH variable should point towards the build directory
#  of the target kernel.
# KERNEL_PATH = ""
KERNEL_PATH = /lib/modules/$(shell uname -r)/build

# Makefile
obj-m += llram_driver.o

all:
	make -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean
