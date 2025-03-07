# Overlaybd

## Accelerated Container Image

[Accelerated Container Image](https://github.com/alibaba/accelerated-container-image) is an open-source implementation of paper ["DADI: Block-Level Image Service for Agile and Elastic Application Deployment. USENIX ATC'20"](https://www.usenix.org/conference/atc20/presentation/li-huiba).
It is a solution of remote container image by supporting fetching image data on-demand without downloading and unpacking the whole image before a container running.

At the heart of the acceleration is overlaybd, which provides a merged view of a sequence of block-based layers as an block device.
This repository is a component of Accelerated Container Image, provides an implementation of overlaybd by [TCMU](https://www.kernel.org/doc/Documentation/target/tcmu-design.txt).

## Setup

### System Requirements

Overlaybd provides virtual block devices through TCMU, so the TCMU kernel module is required. TCMU is implemented in the Linux kernel and supported by most Linux distributions.

Check and load the target_core_user module.

```bash
modprobe target_core_user
```

### Install From RPM/DEB

You may download our RPM/DEB packages form [Release](https://github.com/alibaba/overlaybd/releases) and install.

The binaries are install to `/opt/overlaybd/bin/`.

Run `/opt/overlaybd/bin/overlaybd-tcmu` and the log is stored in `/var/log/overlaybd.log`.

It is better to run `overlaybd-tcmu` as a service so that it can be restarted after unexpected crashes.

### Build From Source

#### Requirements

To build overlaybd from source code, the following dependencies are required:

* CMake >= 3.11

* gcc/g++ >= 7

* Libaio, libcurl, libnl3, glib2 and openssl runtime and development libraries.
  * CentOS/Fedora: `sudo yum install libaio-devel libcurl-devel openssl-devel libnl3-devel glib2-devel`
  * Debian/Ubuntu: `sudo apt install pkg-config libcurl4-openssl-dev libssl-dev libaio-dev libnl-3-dev libnl-genl-3-dev libglib2.0-dev`

#### Build

You need git to checkout the source code:

```bash
git clone https://github.com/alibaba/overlaybd.git
cd overlaybd
```

The whole project is managed by CMake. Binaries and resource files will be installed to `/opt/overlaybd/`.

```bash
mkdir build
cd build
cmake ..
make -j
sudo make install
```

Finally, setup a systemd service for overlaybd-tcmu backstore.

```bash
sudo systemctl enable /opt/overlaybd/overlaybd-tcmu.service
sudo systemctl start overlaybd-tcmu
```

## Configuration

### overlaybd config
Default configure file `overlaybd.json` is installed to `/etc/overlaybd/`.

```json
{
    "logLevel": 1,
    "logPath": "/var/log/overlaybd.log",
    "registryCacheDir": "/opt/overlaybd/registry_cache",
    "registryCacheSizeGB": 1,
    "credentialFilePath": "/opt/overlaybd/cred.json",
    "ioEngine": 1,
    "download": {
        "enable": true,
        "delay": 120,
        "delayExtra": 30,
        "maxMBps": 100
    },
    "enableAudit": true,
    "auditPath": "/var/log/overlaybd-audit.log"
}
```

| Field               | Description                                                                                           |
| ---                 | ---                                                                                                   |
| logLevel            | DEBUG 0, INFO  1, WARN  2, ERROR 3                                                                    |
| ioEngine            | IO engine used to open local files: psync 0, libaio 1, posix aio 2.                                   |
| logPath             | The path for log file, `/var/log/overlaybd.log` is the default value.                                 |
| registryCacheDir    | The cache directory for remote image data.                                                            |
| registryCacheSizeGB | The max size of cache, in GB.                                                                         |
| credentialFilePath  | The credential used for fetching images on registry. `/opt/overlaybd/cred.json` is the default value. |
| download.enable     | Whether background downloading is enabled or not.                                                     |
| download.delay      | The seconds waiting to start downloading task after the overlaybd device launched.                    |
| download.delayExtra | A random extra delay is attached to delay, avoiding too many tasks started at the same time.          |
| download.maxMBps    | The speed limit in MB/s for a downloading task.                                                       |
| enableAudit         | Enable audit or not.                                                                                  |
| auditPath           | The path for audit file, `/var/log/overlaybd-audit.log` is the default value.                         |

> NOTE: `download` is the config for background downloading. After an overlaybd device is lauched, a background task will be running to fetch the whole blobs into local directories. After downloading, I/O requests are directed to local files. Unlike other options, download config is reloaded when a device launching.

### credential config

Here is an example of credential file described by `credentialFilePath` field.

```json
{
  "auths": {
    "hub.docker.com": {
      "username": "username",
      "password": "password"
    },
    "hub.docker.com/hello/world": {
      "auth": "dXNlcm5hbWU6cGFzc3dvcmQK"
    }
  }
}
```

Credentials are reloaded when authentication is required.
Credentials have to be updated before expiration if temporary credential is used, otherwise overlaybd keeps reloading until a valid credential is set.
For the convenience of testing, we provided a public registry on Aliyun ACR, see later examples.

> **Important**: The corresponding credential has to be set before launching devices, if the registry is not public.

## What's next?

Now we have finished the setup of overlaybd, let's go back to [Accelerated Container Image](https://github.com/alibaba/accelerated-container-image) repo and start to run our first accelerated container.


## Kernel module

[DADI_kmod](https://github.com/data-accelerator/dadi-kernel-mod) is a kernel module of overlaybd. It can make local overlaybd-format files as a loop device or device-mapper.

## Contributing

Welcome to contribute! [CONTRIBUTING](CONTRIBUTING.md)

## Licenses

Overlaybd is released under the Apache License, Version 2.0.
