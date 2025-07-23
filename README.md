# XpressReal Linux SDK

Welcome to the official Linux SDK repository for the XpressReal board!

This repository contains `meta-xpressreal`, a dedicated [Yocto Project](https://www.yoctoproject.org/) layer for the XpressReal Single-Board Computer (SBC). By using this layer, you can build a feature-rich, customizable embedded Linux distribution for your XpressReal board.

## 🚀 About the XpressReal Board

XpressReal is a high-performance, compact Single-Board Computer based on the Realtek **RTD1619B** SoC (codename: **Stark**). It is specifically designed for embedded applications that demand powerful media processing capabilities and rich connectivity options.

### Key Features

* **Core Processor**: Realtek RTD1619B, featuring a multi-core ARM Cortex-A55 CPU and an ARM Mali-G57 GPU for efficient computing and graphics performance.
* **Powerful Video Capabilities**: Supports 4K@30fps video decoding, making it an ideal choice for multimedia applications.
* **Rich Interfaces**: Despite its small size, it integrates common interfaces like HDMI 2.1, USB 3.0, Gigabit Ethernet, GPIO, I2C, and SPI, offering excellent extensibility.
* **Compact Form Factor**: The small board design allows for easy integration into a wide variety of space-constrained projects.

## 🛠️ SDK Quick Start Guide

This SDK is built upon the Yocto Project. You will need a Linux host machine (Ubuntu 20.04/22.04 is recommended) for the build process.

### System requirement

#### Operating system
* Ubuntu 22.04/20.04/18.04 LTS
* Fedora 39/40/41
* Debian GNU/Linux 11/12

#### Minimum Free Disk Space
To build an image such as core-image-sato for the qemux86-64 machine, you need a system with at least 90 Gbytes of free disk space. However, much more disk space will be necessary to build more complex images, to run multiple builds and to cache build artifacts, improving build efficiency.

#### Minimum System RAM
You will manage to build an image such as core-image-minimal with as little as 8 Gbytes of RAM on an old system with 4 CPU cores, but your builds will be much faster on a system with as much RAM and as many CPU cores as possible.

### 📚 Full Documentation
For detailed hardware specifications, SDK building guide, firmware flashing instructions, and more, please refer to our official Wiki:

➡️ Visit the Official [XpressReal Wiki](https://wiki.xpressreal.io/guides/building-yocto/)

### 🤝 Contributing
Contributions of all kinds are welcome! If you find a bug or have a feature suggestion, please feel free to open an Issue. If you would like to contribute code, please create a Pull Request.
