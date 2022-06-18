# Maintainer: Precise
pkgname=rtl-sdr-exp-git
pkgrel=1
pkgver=v2022.6.r0.g71aafcc
epoch=1
pkgdesc="Turns your Realtek RTL2832 based DVB dongle into a SDR receiver. Experimental R820T and R820T2 rtl-sdr driver that tunes down to 13 mhz or lower. (From my experiments up to 3.4 Mhz on R820T2)."
arch=('i686' 'x86_64' 'aarch64')
url="https://github.com/Mr-Precise/rtl-sdr"
license=('GPL2')
depends=('libusb')
makedepends=('git' 'cmake')
provides=('rtl-sdr')
source=('git+https://github.com/Mr-Precise/rtl-sdr.git')

pkgver() {
  cd "${srcdir}/rtl-sdr"
  git describe --long --tags | sed 's/\([^-]*-g\)/r\1/;s/-/./g'
}

build() {
  cd "${srcdir}/rtl-sdr"
  mkdir -p build
  cd build
  cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr \
    -DDETACH_KERNEL_DRIVER=ON -DENABLE_ZEROCOPY=ON -Wno-dev ../
  make
}

package() {
  cd "${srcdir}/rtl-sdr/build/"
  make DESTDIR="${pkgdir}" install
  install -D -m644 "${srcdir}/rtl-sdr/rtl-sdr.rules" \
    "${pkgdir}/usr/lib/udev/rules.d/10-rtl-sdr.rules"
}

md5sums=('SKIP')
