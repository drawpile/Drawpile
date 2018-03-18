FROM centos:6

RUN yum -y install epel-release wget
RUN yum -y install tar bzip2 git libtool which fuse fuse-devel libpng-devel automake libtool mesa-libEGL cppunit-devel cmake3 glibc-headers libstdc++-devel freetype-devel fontconfig-devel libxml2-devel libstdc++-devel libXrender-devel patch xcb-util-keysyms-devel libXi-devel mesa-libGL-devel libxcb libxcb-devel xcb-util xcb-util-devel openssl-devel xz unzip glibc-devel glib2-devel pulseaudio-libs-devel
RUN yum -y install libxcb libxcb-devel libXrender libXrender-devel xcb-util-wm xcb-util-wm-devel xcb-util xcb-util-devel xcb-util-image xcb-util-image-devel xcb-util-keysyms xcb-util-keysyms-devel

# Newer compiler than what comes with CentOS 6
RUN yum -y install centos-release-scl-rh
RUN yum -y install devtoolset-3-gcc devtoolset-3-gcc-c++

RUN wget http://download.qt.io/archive/qt/5.6/5.6.2/single/qt-everywhere-opensource-src-5.6.2.tar.xz -O /opt/qt.tar.xz

ADD Build-qt /
RUN bash -ex Build-qt

ADD Build-deps /
RUN bash -ex Build-deps
