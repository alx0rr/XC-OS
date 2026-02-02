<div align="center">
<h1>XC Operating System</h1>

[![GitHub Release](https://img.shields.io/github/v/release/alx0rr/XC-OS?style=flat-square&logo=github)](https://github.com/alx0rr/XC-OS/releases)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](https://github.com/alx0rr/XC-OS/blob/main/LICENSE)
[![Documentation](https://img.shields.io/website?down_message=failing&label=web&up_color=green&up_message=passing&url=https://xc-os-website.onrender.com/&style=flat-square)](https://xc-os-website.onrender.com/)
[![Issues](https://img.shields.io/github/issues-raw/alx0rr/XC-OS.svg?maxAge=25000&style=flat-square)](https://github.com/alx0rr/XC-OS/issues)
[![Pull requests](https://img.shields.io/github/issues-pr/alx0rr/XC-OS.svg?style=flat-square)](https://github.com/alx0rr/XC-OS/pulls)


XC-OS — Operating System maked by alx0rr and forker-25.
This Operating System is simple and example project, which contains Custom FS(XCFS), simple VBE graphics, MMU, Kernel and Bootloader.
The OS was created for hobby purposes, so it may contain errors, bugs and other shortcomings.
Please think carefully before installing it on real hardware.

---

building the img file with build.sh:
</div>

```shell
cd project && sh build.sh
```

<div align="center">
usage with qemu:
</div>
<br>

```shell
qemu-system-x86_64   -drive file=build/xcos.img
```


<h2 align="center">Preview</h2>

<p align="center">
  <img src="res/main.png" width="600"/>
  <br><br>
  <img src="res/dirs.png" width="600"/>
  <br><br>
  <img src="res/help.png" width="600"/>
</p>
