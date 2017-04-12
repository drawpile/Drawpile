FROM centos:6

RUN yum -y install epel-release wget
RUN wget http://people.centos.org/tru/devtools-2/devtools-2.repo -O /etc/yum.repos.d/devtools-2.repo
RUN yum -y install devtoolset-2-gcc devtoolset-2-gcc-c++ devtoolset-2-binutils zlib-devel qt5-qtbase-devel qt5-qtsvg-devel qt5-qtmultimedia-devel qt5-linguist cmake3 make giflib-devel wget unzip git xz binutils fuse glibc-devel glib2-devel fuse-devel zlib-devel libpng12 libmicrohttpd-devel

ADD Build-* /
ADD appimage.patch /

RUN bash -ex Build-deps

