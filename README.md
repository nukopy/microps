# microps

[![MIT license badge][mit-badge]][mit-url]

[mit-badge]: https://img.shields.io/badge/license-MIT-blue.svg
[mit-url]: https://github.com/nukopy/microps/blob/main/LICENSE

microps is an implementation of a small TCP/IP protocol stack written in C for learning.

This project is based on the development guide, [KLab Expert Camp 6](https://drive.google.com/drive/folders/1k2vymbC3vUk5CTJbay4LLEdZ9HemIpZe) (only in Japanese).

Original implementation is [pandax381/microps](https://github.com/pandax381/microps).

## Features

TODO

ref: https://github.com/pandax381/microps#features

## Environment

Host

- OS: macOS 14.7
- UTM 4.6.5
- Ruby 3.3.0
- Vagrant 2.4.9
- vagrant_utm 0.1.5 (Version Constraint: > 0)
- [vagrant-bindfs](https://github.com/gael-ian/vagrant-bindfs) 1.3.1

VM for development

- OS: Ubuntu 24.04
  - Box definition in HashiCorp: [utm/ubuntu-24.04](https://portal.cloud.hashicorp.com/vagrant/discover/utm/ubuntu-24.04)
  - Official doc of UTM: [UTM Vagrant Box Gallery](https://naveenrajm7.github.io/vagrant_utm/boxes/utm_box_gallery.html)

## Setup

### Install UTM

See [UTM homepage](https://mac.getutm.app/).

For macOS users, this article is helpful (only in Japanese): [【macOS】UTM で Ubuntu Desktop 24.04 LTS の仮想マシン環境を構築する](https://zenn.dev/pyteyon/scraps/0c8cec3c56812b)

### Install Vagrant

```sh
brew install vagrant

# install vagrant plugins for UTM
vagrant plugin install vagrant_utm --plugin-version 0.1.5
vagrant plugin install vagrant-bindfs --plugin-version 1.3.1
```

### Setup VM

```sh
cd microps

# start VM
vagrant up

# start VM with provision (rerun bootstrap script)
vagrant up --provision

# reload VM
vagrant reload

# realod VM with provision
vagrant reload --provision

# status VM
vagrant status

# ssh to VM
vagrant ssh

# get SSH config (e.g. for VSCode Remote-SSH)
vagrant ssh-config

# stop VM
vagrant halt
```

## Development

Working directory in VM is `/home/me/workspace/microps`, so following commands should be run in this directory.

### Build

```sh
cd /home/me/workspace/microps

# build
make

# clean
make clean
```

### Test

```sh
# build
make

# run tests: step0.exe, step1.exe, ...
./test/step0.exe
```

### Format

```sh
make format
```

## References

- [github.com/pandax381/microps](https://github.com/pandax381/microps)
  - Implementation of TCP/IP protocol stack in C. This is a reference implementation of this project.
- [KLab Expert Camp 6](https://drive.google.com/drive/folders/1k2vymbC3vUk5CTJbay4LLEdZ9HemIpZe)
  - A series of lectures on TCP/IP protocol stack, [microps](https://github.com/pandax381/microps). This project is based on the contents of this lecture.
- [Available Vagrant boxes for UTM](https://portal.cloud.hashicorp.com/vagrant/discover?query=utm)
  - Vagrant plugin for UTM.

## License

Copyright (c) 2012-2021 YAMAMOTO Masaya

microps is licensed under the MIT License. For more details, check out the [LICENSE](./LICENSE) file.

# microps
