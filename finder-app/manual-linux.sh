#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.
# Modified: Vidhya. PL
# Date: 09/17/2023

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}
#Checking if directory creation failed
if [ ! -d ${OUTDIR}]; then
   	echo Output directory does not exist
   	exit 1      #Failure Operation
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    #from lecture slides    
     make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
     make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
     make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
     make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
     make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}
echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
#from lecture slides
mkdir -p ${OUTDIR}/rootfs && cd ${OUTDIR}/rootfs
echo Creating the neccessary base directories
mkdir -p {bin,dev,etc,home,lib,lib64,proc,sbin,sys,tmp,usr,var} usr/{bin,lib,sbin} var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    
    # TODO:  Configure busybox
    #from lecture slides
    echo Configuration of Busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
#from lecture slides
echo Making and installing Busybox
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
#obtained from  ChatGPT at https://chat.openai.com/ with prompts including "Add library dependencies to rootfs"
echo Adding library dependencies to rootfs
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
echo "${SYSROOT}"

cp -f $SYSROOT/lib/ld-linux-aarch64.so.1 lib
cp -f $SYSROOT/lib64/libm.so.6 lib64
cp -f $SYSROOT/lib64/libresolv.so.2 lib64
cp -f $SYSROOT/lib64/libc.so.6 lib64

# TODO: Make device nodes
#from lecture slides
echo Making Device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
echo Cleaning and building the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo Copying scripts and executables to the /home directory
cp -f ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home
cp -f ${FINDER_APP_DIR}/conf/username.txt ${OUTDIR}/rootfs/home
cp -f ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home
cp -f ${FINDER_APP_DIR}/conf/ -r ${OUTDIR}/rootfs/home
cp -f ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home
cp -f ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home
cp -f ${FINDER_APP_DIR}/writer.c ${OUTDIR}/rootfs/home
cp -f ${FINDER_APP_DIR}/conf/assignment.txt ${OUTDIR}/rootfs/home

# TODO: Chown the root directory
#from lecture slides
echo Chowning the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
#from lecture slides
echo Creating initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ..
echo "$(pwd)"
gzip -f initramfs.cpio
