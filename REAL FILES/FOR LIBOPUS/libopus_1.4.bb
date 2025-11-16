SUMMARY = "Opus interactive audio codec library"
HOMEPAGE = "https://opus-codec.org"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://COPYING;md5=e304cdf74c2a1b0a33a5084c128a23a3"

SRC_URI = "https://downloads.xiph.org/releases/opus/opus-${PV}.tar.gz"
SRC_URI[sha256sum] = "c9b32b4253be5ae63d1ff16eea06b94b5f0f2951b7a02aceef58e3a3ce49c51f"

inherit autotools pkgconfig

EXTRA_OECONF = "--disable-static --enable-shared"

PACKAGES =+ "${PN}-tools"
FILES:${PN}-tools += "${bindir}/*"

do_install() {
    autotools_do_install

    # Optional: verify headers
    install -d ${D}${includedir}/opus
    install -m 0644 include/opus/*.h ${D}${includedir}/opus
}

BBCLASSEXTEND = "native nativesdk"