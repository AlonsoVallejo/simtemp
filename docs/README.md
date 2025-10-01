# NXP Simulated Temperature Sensor

This project implements a virtual temperature sensor system for Linux, consisting of a kernel module that simulates hardware temperature readings and exposes them to user space applications.

## Project Overview

This is a response to the NXP Systems Software Engineer Challenge, implementing:
- **Kernel module (`nxp_simtemp`)**: Platform driver producing periodic temperature samples
- **Device Tree overlay**: Proper device binding and configuration
- **User space applications**: CLI and optional GUI for sensor interaction
- **Build system**: Automated build and test scripts

## Repository Structure

```
simtemp/
├── kernel/                     # Kernel module source
│   ├── Kbuild                  # Kernel build configuration
│   ├── Makefile                # Build targets for driver and DT overlay
│   ├── nxp_simtemp.c          # Main kernel driver implementation
│   ├── nxp_simtemp.h          # Kernel driver header
│   ├── nxp_simtemp_ioctl.h    # IOCTL definitions (placeholder)
│   └── dts/
│       └── nxp-simtemp.dtsi   # Device Tree overlay source
├── user/                      # User space applications
│   ├── cli/                   # Command-line interface
│   └── gui/                   # Optional GUI (future)
├── scripts/                   # Build and test scripts
│   ├── build.sh              # Build script (future)
│   └── run_demo.sh           # Demo script (future)
├── docs/                     # Documentation
│   ├── README.md             # This file
│   ├── DESIGN.md             # Architecture documentation (future)
│   ├── TESTPLAN.md           # Test plan (future)
│   └── AI_NOTES.md           # AI assistance notes (future)
└── challenge_reqs/           # Challenge requirements
    └── systems_sw_engineer_challenge_stage1.md
```

## Prerequisites

### System Requirements
- **OS**: Ubuntu 24.04 LTS (or compatible Linux distribution)
- **Kernel**: Linux kernel with loadable module support
- **Architecture**: x86_64 (tested), ARM64 (should work)

### Required Packages
```bash
# Essential build tools
sudo apt update
sudo apt install gcc make build-essential

# Kernel development headers
sudo apt install linux-headers-$(uname -r)

# Device Tree Compiler
sudo apt install device-tree-compiler

# Optional: Git for version control
sudo apt install git
```

## Build Instructions

### Step 1: Navigate to Project Directory
```bash
cd /path/to/simtemp/kernel
```

### Step 2: Build the Kernel Module and Device Tree Overlay
```bash
make
```

This command will:
1. Compile the kernel module (`nxp_simtemp.ko`)
2. Compile the device tree overlay (`nxp-simtemp.dtbo`)

### Step 3: Verify Build Artifacts
```bash
ls -la *.ko *.dtbo
```

Expected output:
```
-rw-r--r-- 1 user user  XXXX nxp_simtemp.ko
-rw-r--r-- 1 user user  XXXX nxp-simtemp.dtbo
```

## Running the Module

### Loading the Kernel Module
```bash
# Load the module
sudo insmod nxp_simtemp.ko

# Verify it loaded successfully
lsmod | grep nxp_simtemp

# Check kernel messages
dmesg | tail -10
```

Expected kernel messages:
```
[timestamp] nxp_simtemp: init
[timestamp] nxp_simtemp: probe
[timestamp] nxp_simtemp: char device created successfully
```

### Verifying Device Creation
```bash
# Check if character device was created
ls -la /dev/nxp_simtemp

# Check sysfs entries (when implemented)
ls -la /sys/class/simtemp_class/
```

### Testing Basic Functionality
```bash
# Test device opening (basic test)
cat /dev/nxp_simtemp

# Check kernel logs for device activity
dmesg | tail -5
```

### Unloading the Module
```bash
# Remove the module
sudo rmmod nxp_simtemp

# Verify clean removal
lsmod | grep nxp_simtemp
dmesg | tail -5
```

## Current Implementation Status

### ✅ Completed Features
- [x] Basic kernel module framework
- [x] Platform driver registration
- [x] Character device creation (`/dev/nxp_simtemp`)
- [x] Device Tree overlay support
- [x] Clean module loading/unloading
- [x] Build system (Makefile) for kernel module and DT overlay
- [x] Temperature simulation logic

### 🚧 In Progress / TODO
- [ ] Timer/workqueue for periodic sampling
- [ ] Ring buffer for sample storage
- [ ] Sysfs attributes for configuration
- [ ] Poll/epoll support for user space
- [ ] Binary sample record format
- [ ] User space CLI application
- [ ] Build and demo scripts
- [ ] Comprehensive testing

## Troubleshooting

### Build Issues

**Problem**: `gcc-13: not found`
```bash
# Solution: Install GCC compiler
sudo apt install gcc-13 gcc
```

**Problem**: `make: dtc: No such file or directory`
```bash
# Solution: Install Device Tree Compiler
sudo apt install device-tree-compiler
```

**Problem**: Kernel headers not found
```bash
# Solution: Install kernel headers for your kernel version
sudo apt install linux-headers-$(uname -r)
```

### Runtime Issues

**Problem**: Module fails to load
```bash
# Check kernel logs for error details
dmesg | tail -20

# Verify kernel version compatibility
uname -r
modinfo nxp_simtemp.ko
```

**Problem**: Device file not created
```bash
# Check if module loaded successfully
lsmod | grep nxp_simtemp

# Check for any error messages
dmesg | grep nxp_simtemp
```

## Development Environment

### Tested Configurations
- **Ubuntu 24.04 LTS** with kernel 6.14.0-32-generic
- **GCC 13.3.0** (Ubuntu 13.3.0-6ubuntu2~24.04)
- **Device Tree Compiler 1.7.0**

### Build Warnings (Expected)
The following warnings are expected and can be ignored:
- `warning: the compiler differs from the one used to build the kernel` - Minor version differences
- `Clock skew detected` - File timestamp differences
- `unit_address_vs_reg` in DT overlay - Normal for virtual devices

## API Reference (Planned)

### Device Tree Properties
```dts
simtemp0: simtemp@0 {
    compatible = "nxp,simtemp";
    sampling-ms = <100>;        /* Sampling period in milliseconds */
    threshold-mC = <45000>;     /* Alert threshold in milli-Celsius */
    status = "okay";
};
```

### Sysfs Interface (Future)
```
/sys/class/simtemp_class/simtemp/
├── sampling_ms      # RW: Sampling period
├── threshold_mC     # RW: Alert threshold  
├── mode            # RW: Operation mode
└── stats           # RO: Statistics
```

### Binary Record Format (Future)
```c
struct simtemp_sample {
    __u64 timestamp_ns;   // Monotonic timestamp
    __s32 temp_mC;        // Temperature in milli-Celsius
    __u32 flags;          // Event flags
} __attribute__((packed));
```

## Links

**Git Repository**: [To be added]  
**Demo Video**: [To be added]

## Contributing

This project follows the NXP Systems Software Engineer Challenge requirements. See `challenge_reqs/systems_sw_engineer_challenge_stage1.md` for detailed specifications.

## License

GPL-2.0 - See kernel module source for license details.

## Author

AlonsoVallejo <jvallejorios@hotmail.com>