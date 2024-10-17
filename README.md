# llram-kernel-driver-for-Cortex-R

This repository contains an example Linux kernel driver for the _Low-latency RAM_ (LLRAM) port that is present on Arm :registered: Cortex :registered:-based processors that implement the 64-bit R-profile architecture. The LLRAM port is optimized to give a more deterministic and lower-latency access to memory for real-time applications. In order for software to access the memory connected to the LLRAM port from Linux a driver is required and this repository contains an example that can be adapted for the required application. This is required because it is not possible to place pagetables in memory conncted to the LLRAM port, and so the LLRAM cannot be used as general purpose memory by Linux. By using a driver it is still possible for user space applications to communicate with real-time software via the LLRAM port.

## Usage

The driver should be built along with the kernel for the target device. The driver supports the following arguments:

- `enable_llram` - `0/1` - If set, the driver should attempt to enable the LLRAM port, otherwise it is expected it is enabled by bootcode or the hypervisor.
- `allow_simulated` - `0/1` - If set, support a simulated driver as described below, otherwise will error on attempting to load the driver when an LLRAM port is not present.

## Supported Processors

This driver supports the Arm :registered: Cortex :registered:-R82 processor and the Arm :registered: Cortex :registered:-R82AE processor.

For Cortex :registered:-R82 the following revisions are supported:

- All revisions from r1p0 onwards (r0p0, r0p1 and r0p2 do not support VMSA)

For Cortex :registered:-R82AE the following revisions are supported:

- All revisions

## Limitations

This driver provides a minimal example to be able to access memory connected to the LLRAM port from within Linux, for example to allow sharing of data with a real-time operating system that is also using LLRAM. This example has some limitations which include:

- The driver only supports simple read/write accesses, a mmap interface would give more performance but is not currently implemented in this example.
- There is no synchronization of accesses to LLRAM, so only a single application can access the LLRAM at once.
- No access controls or permissions are implemented to restrict access to LLRAM.
- The driver includes debug statements to log accesses to the LLRAM, these could impact performance if kernel debug logging is enabled. These are included as this example is expected to be used for development.
- There is no support for systems where the CFGLLRAMSHARED pin is set to 1.

The implementation of some of these features will be dependent on the eventual system and application and so outside the scope of a generic example driver.

## Simulated mode

For development of applications that need to interact with this driver, when developing on a processor that does not contain an LLRAM port the driver will allocate 256MB of memory which supports simple read/write accesses.
This does not simulate any of the functionality of an LLRAM port and so will not provide any determinism or latency guarantees but is just intended to provide a memory that uses the same API for application development.

## Changelog

### Version 0.3
Updating supported revisions

### Version 0.2
Fix out-by-one issue when checking bounds of a write

### Version 0.1
Initial release

## License

llram-kernel-driver-for-Cortex-R is licensed under GPL-2.0.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
