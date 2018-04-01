#!/bin/bash -x

# Add pkgs needed to build nfs-ganesha
apt-get update && apt-get install -y bison flex doxygen libdbus-1-dev

cd /nfs-ganesha

# Clean up previous blds
[ -e Makefile ] && make clean

rm -f CMakeCache.txt CPackConfig.cmake CPackSourceConfig.cmake cmake_install.cmake

# Reconfigure make
cmake src -DCMAKE_BUILD_TYPE=Release -DUSE_FSAL_VFS=ON -DUSE_DBUS=NON -DUSE_FSAL_PROXY=ON -DLSB_RELEASE=Release -DUSE_FSAL_GPFS=OFF -DUSE_FSAL_LUSTRE=OFF -DUSE_FSAL_MEM=OFF -DUSE_FSAL_NULL=OFF -DUSE_GSS=OFF

# Build
make clean install
[ $? -ne 0 ] && exit 1

# Create install archive of the built binaries and config files.
tar --transform "s|.*/org.ganesha.nfsd.conf|sharedV4/config/org.ganesha.nfsd.conf|" -czvf nfs-ganesha-ubuntu-V2.7.tgz /usr/bin/ganesha.nfsd /usr/lib/ganesha /usr/lib/libntirpc* /var/run/ganesha /usr/lib/pkgconfig/libntirpc.pc /etc/ganesha/ganesha.conf src/scripts/ganeshactl/org.ganesha.nfsd.conf
