# 双接口型PCAN-USB驱动 | Dual-interface PCAN-USB driver

## 简介 | Introduction

* 若想了解什么是`PCAN-USB`，可自行搜索或访问其[官网](https://www.peak-system.com/)。
    > To know what `PCAN-USB` is, Google it, or visit the [official website](https://www.peak-system.com/).

* **官方驱动**支持`网络`和`字符设备`两种接口，但**同一时间内只能使用其中一种接口**，
且要**重新编译、加载**驱动。**本项目**的目标是**可同时使用两种接口**，
或最低限度不必重新编译和切换驱动。
    > The **official driver** supports two types of interface: `network` and `chardev`,
    but **only one is available at a time**, and the user has to **re-compile and re-load** the driver.
    The goal of **this project** is to **make both interfaces available simultaneously**,
    or at least remove the need of re-compiling and switching driver.

* 仅支持`PCAN-USB`，不支持`PCAN-USB FD`、`PCAN-USB Pro`、`PCAN-PCI`及其他类型的硬件产品。
    > For `PCAN-USB` only, not for `PCAN-USB FD`, `PCAN-USB Pro`, `PCAN-PCI` and other hardware products.

* 本项目**仅供学习和测试**！**强烈不建议在生产环境使用**！
    > This project is **for STUDYINIG and TESTING ONLY**!
    **Use in PRODUCTION environment is STRONGLY DISCOURAGED**!

## 安装 | Installation

````
$ make prepare # Only needed at the first time
$ sudo -E make install
````

## 卸载 | Uninstallation

````
$ sudo -E make uninstall
````

## 更新 | Update

````
$ git pull && git checkout vX.Y.Z # For example: v0.8.0
$ sudo -E make update
````

## 使用 | Use

* `网络`接口：使用`candump`，或自行编写应用程序（推荐使用`SocketCAN`应用编程接口）。
    > `Network` interface: Use `candump`, or write an application program yourself (`SocketCAN` API is recommended).

* `字符设备`接口：使用`pcanview`。
    > `Chardev` interface: Use `pcanview`.

## 许可证 | License

GPL-2.0

