#
# PetaLinux recipe for FPGA Walkie-Talkie with Opus codec
#

SUMMARY = "FPGA Multi-Board Walkie-Talkie with Opus Compression"
SECTION = "apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://walkietalkie.c \
           file://opus_helper.c \
           file://opus_helper.h \
           file://network.c \
           file://network.h \
           file://audio_dma.c \
           file://audio_dma.h \
           file://gpio_ptt.c \
           file://gpio_ptt.h \
           file://Makefile \
          "

S = "${WORKDIR}"

# Dependencies
DEPENDS = "opus"
RDEPENDS_${PN} = "opus kernel-module-uio-pdrv-genirq"

# Compiler flags
TARGET_CC_ARCH += "${LDFLAGS}"

inherit pkgconfig

do_compile() {
    oe_runmake all
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/walkietalkie ${D}${bindir}/
}

FILES_${PN} = "${bindir}/walkietalkie"
FILES_${PN}-dbg += "${bindir}/.debug"

INSANE_SKIP_${PN} = "ldflags"
