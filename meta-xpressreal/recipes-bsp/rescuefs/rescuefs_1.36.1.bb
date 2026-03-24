DESCRIPTION = "Generate minimal rescue rootfs for avengers platform"

require recipes-core/busybox/busybox_${PV}.bb

DEPENDS += "cpio-native xz-native rescuefs-parted rescuefs-pixz rescuefs-pv"

DEPENDS += "virtual/kernel linux-firmware"

do_deploy[depends] = "rescuefs-parted:do_deploy rescuefs-e2fsprogs:do_deploy rescuefs-pixz:do_deploy rescuefs-pv:do_deploy"

do_deploy[depends] += "linux-yocto:do_deploy linux-firmware:do_install"

S = "${WORKDIR}/busybox-${PV}"

BUSYBOXDIR = "${COREBASE}/meta/recipes-core/busybox"
FILESEXTRAPATHS:prepend := "${BUSYBOXDIR}/busybox:"
FILESEXTRAPATHS:prepend := "${BUSYBOXDIR}/files:"

#busybox defconfig for generating rescue rootfs
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

#Add back rcS.default and inittab from original busybox_1.35.0.bb and busybox-inittab_1.35.0.bb
SRC_URI += " \
	file://rescuefs.cfg \
	file://nas-loader_a \
	file://fstab \
	file://rcS.default \
	file://inittab \
	file://rtkotp \
	file://modules \
	file://module_load.sh \
	"

#disable separate SUID and non SUID busybox to reduce binary size
BUSYBOX_SPLIT_SUID = "0"

SYSTEMD_PACKAGES = ""
SYSTEMD_SERVICE:${PN}-syslog = ""

do_install:append() {

	#install start scripts since original busybox would rely on systemv init
	install -D -m 0755 ${WORKDIR}/rcS ${D}${sysconfdir}/init.d/rcS
	install -D -m 0755 ${WORKDIR}/rcK ${D}${sysconfdir}/init.d/rcK
	install -D -m 0755 ${WORKDIR}/rcS.default ${D}${sysconfdir}/default/rcS
	install -D -m 0644 ${WORKDIR}/inittab ${D}${sysconfdir}/inittab
	install -D -m 0755 ${WORKDIR}/nas-loader_a ${D}${sbindir}/nas-loader_a
	install -D -m 0644 ${WORKDIR}/fstab ${D}${sysconfdir}/fstab
	install -D -m 0755 ${WORKDIR}/rtkotp ${D}${sbindir}/rtkotp

	install -D -m 0755 ${WORKDIR}/module_load.sh ${D}${sbindir}/module_load.sh
	install -D -m 0644 ${WORKDIR}/modules ${D}${sysconfdir}/modules

	sed -i '/rebooting/i ${KERNEL_CONSOLE}::respawn:/bin/sh \n' ${D}${sysconfdir}/inittab

	sed -i 's:/bin:/usr/bin:g' ${D}${sysconfdir}/inittab
	sed -i 's:/sbin:/usr/sbin:g' ${D}${sysconfdir}/inittab

	#remove mmc realted auto mount since we will do firmware upgrade
	sed -i '/^mmcblk/d' ${D}${sysconfdir}/mdev.conf

	#keep compatibility of shell path
	echo "/bin/sh" >> ${D}${sysconfdir}/busybox.links

	IFS='
	'
	for NAME in `cat ${D}${sysconfdir}/busybox.links`; do
		if [ ! -f ${D}/${NAME} ]; then
			DIR=`dirname "${NAME}"`
			mkdir -p ${D}/${DIR}
			ln -s -r ${D}${bindir}/busybox ${D}/${NAME}
		fi
	done

	# create symbolic /init
	[ ! -f ${D}/init ] && ln -s -r ${D}${bindir}/busybox ${D}/init

	echo "/usr/sbin/module_load.sh" >> ${D}${sysconfdir}/init.d/rcS
	echo "echo /usr/sbin/mdev > /proc/sys/kernel/hotplug" >> ${D}${sysconfdir}/init.d/rcS
	echo "/usr/sbin/mdev -s" >> ${D}${sysconfdir}/init.d/rcS
	echo "/usr/sbin/nas-loader_a &" >> ${D}${sysconfdir}/init.d/rcS

	install -d ${D}/proc
	install -d ${D}/sys
	install -d ${D}/dev
	install -d ${D}/tmp
	install -d ${D}/run/media
}

inherit deploy nopackages

do_deploy() {

	install -d ${DEPLOYDIR}/${BOOTFILES_DIR}
	install -m 0755 ${DEPLOY_DIR_IMAGE}/staging/resize2fs ${D}${base_sbindir}/resize2fs
	install -m 0755 ${DEPLOY_DIR_IMAGE}/staging/e2fsck ${D}${base_sbindir}/e2fsck
	install -m 0755 ${DEPLOY_DIR_IMAGE}/staging/parted ${D}${base_sbindir}/parted
	install -m 0755 ${DEPLOY_DIR_IMAGE}/staging/pixz ${D}${base_bindir}/pixz
	install -m 0755 ${DEPLOY_DIR_IMAGE}/staging/pv ${D}${base_bindir}/pv

	if [ -d "${DEPLOY_DIR_IMAGE}/selected-firmware" ]; then
		install -d ${D}/lib/firmware
		cp -a ${DEPLOY_DIR_IMAGE}/selected-firmware/. ${D}/lib/firmware || true
	fi

	if [ -d "${DEPLOY_DIR_IMAGE}/selected-kmods" ]; then
		install -d ${D}/lib/modules
		cp -a ${DEPLOY_DIR_IMAGE}/selected-kmods/. ${D}/lib/modules || true
	fi

	# generate rescue initrd
	(cd ${D} && find . | sort | cpio --reproducible -H newc -o > ${DEPLOYDIR}/${BOOTFILES_DIR}/rescue.root.cpio)
	xz --check=crc32 --lzma2 -f ${DEPLOYDIR}/${BOOTFILES_DIR}/rescue.root.cpio
}

addtask deploy after do_install
