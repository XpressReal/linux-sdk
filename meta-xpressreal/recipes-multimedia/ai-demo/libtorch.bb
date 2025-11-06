SUMMARY = "LibTorch C++ Library"
DESCRIPTION = "Standalone LibTorch C++ API (PyTorch C++ frontend) \
(source : https://github.com/pytorch/pytorch;branch=main;revc=28796f71d04302029290f473a286efc2aba339c2)"
LICENSE = "BSD-3-Clause"
LICENSE_LOCATION ?= "${S}/LICENSE"
LIC_FILES_CHKSUM = "file://${LICENSE_LOCATION};md5=d07280c7432af485bb23b7b316c5f3e5"

require pytorch.inc
S = "${WORKDIR}/pytorch"
B = "${WORKDIR}/build"

inherit cmake pkgconfig python3native

DEPENDS += "coreutils-native gcc \
            zlib-native \
            python3 \
            python3-numpy-native \
            python3-pyyaml-native \
            python3-setuptools-native \
            python3-typing-extensions-native \
            python3-wheel-native \
            autoconf-native \
            automake-native \
            libtool-native \
"

EXTRA_OECMAKE += "\
    -DCMAKE_INSTALL_PREFIX="${prefix}/install" \
    -DCAFFE2_CUSTOM_PROTOC_EXECUTABLE="${S}/build_host_protoc/bin/protoc" \
    -DPYTHON_EXECUTABLE="$(which python3)" \
    -DBUILDING_WITH_TORCH_LIBS=ON \
    -DUSE_CAFFE2=OFF \
    -DUSE_XNNPACK=OFF \
    -DUSE_KINETO=OFF \
    -DUSE_ONNX=OFF \
    -DUSE_SYSTEM_ONNX=OFF \
    -DUSE_DISTRIBUTED=OFF \
    -DBUILD_LITE_INTERPRETER=OFF \
    -DINTERN_BUILD_MOBILE=OFF \
    -DBUILD_BINARY=ON \
    -DBUILD_CAFFE2_MOBILE=OFF \
    -DBUILD_CAFFE2_OPS=OFF \
    -DBUILD_CUSTOM_PROTOBUF=ON \
    -DBUILD_DFT=OFF \
    -DBUILD_DOCS=OFF \
    -DBUILD_GMOCK=ON \
    -DBUILD_GNUABI_LIBS=OFF \
    -DBUILD_LIBM=ON \
    -DBUILD_ONNX_PYTHON=OFF \
    -DBUILD_PYTHON=OFF \
    -DBUILD_QUAD=OFF \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TEST=OFF \
    -DBUILD_TESTS=OFF \
    -DUSE_ASAN=OFF \
    -DUSE_CUDA=OFF \
    -DUSE_DISTRIBUTED=OFF \
    -DUSE_FBGEMM=OFF \
    -DUSE_FFMPEG=ON \
    -DUSE_GFLAGS=ON \
    -DUSE_GLOG=ON \
    -DUSE_GLOO=OFF \
    -DUSE_LEVELDB=ON \
    -DUSE_LITE_PROTO=OFF \
    -DUSE_LMDB=ON \
    -DUSE_METAL=OFF \
    -DUSE_MKLDNN=OFF \
    -DUSE_MPI=OFF \
    -DUSE_NATIVE_ARCH=OFF \
    -DUSE_NNAPI=OFF \
    -DUSE_NNPACK=OFF \
    -DUSE_NUMA=OFF \
    -DUSE_NUMPY=ON \
    -DUSE_OBSERVERS=OFF \
    -DUSE_OPENCL=OFF \
    -DUSE_OPENCV=ON \
    -DUSE_OPENMP=OFF \
    -DUSE_PROF=OFF \
    -DUSE_PYTORCH_QNNPACK=OFF \
    -DUSE_QNNPACK=OFF \
    -DUSE_REDIS=OFF \
    -DUSE_ROCKSDB=ON \
    -DUSE_ROCM=OFF \
    -DUSE_SNPE=OFF \
    -DUSE_SYSTEM_EIGEN_INSTALL=OFF \
    -DUSE_TBB=OFF \
    -DUSE_TENSORRT=OFF \
    -DUSE_ZMQ=ON \
    -DUSE_ZSTD=OFF \
    -DHAVE_STD_REGEX=0 \
    -DHAVE_POSIX_REGEX=0 \
    -DHAVE_STEADY_CLOCK=0 \
    -DATEN_THREADING=NATIVE \
    -DCMAKE_CROSSCOMPILING=ON \
    -DNATIVE_BUILD_DIR="${S}/third_party/sleef/build-host/" \
    -D_GLIBCXX_USE_CXX11_ABI=1 \
"

addtask do_host_protoc_and_sleef before do_configure after do_patch

do_host_protoc_and_sleef() {
    unset CC
    unset CXX
    unset CFLAGS
    unset CXXFLAGS
    unset LDFLAGS
    unset LD_LIBRARY_PATH
    unset MAKE
    unset NINJA
    unset PKG_CONFIG_SYSROOT_DIR
    unset PKG_CONFIG_PATH
    export PKG_CONFIG_SYSROOT_DIR=${STAGING_DIR_NATIVE}
    export PKG_CONFIG_LIBDIR=${STAGING_DIR_NATIVE}/usr/lib/pkgconfig
    export PATH=/usr/local/bin:/usr/bin:/bin
    export CC="/usr/bin/x86_64-linux-gnu-gcc"
    export CXX="/usr/bin/x86_64-linux-gnu-g++"

    rm -rf ${S}/third_party/sleef/build-host
    mkdir -p ${S}/third_party/sleef/build-host
    cd ${S}/third_party/sleef/build-host
    cmake .. -G"Unix Makefiles" -DCMAKE_INSTALL_PREFIX=_install -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
    cmake --build . --config Release --target install -- -j$CPU_NUM VERBOSE=1

    rm -rf "${S}/build_host_protoc"
    chmod +x ${S}/scripts/build_host_protoc.sh
    cd ${S}
    env -i PATH=/usr/local/bin:/usr/bin:/bin HOME=$HOME
    bash scripts/build_host_protoc.sh
    cd -
}



do_install() {
    DESTDIR=${B}/libtorch ninja -C ${B} install
    install -d ${D}${libdir}
    install -d ${D}${includedir}
    install -d ${D}/usr/share
    install -m 0755 ${B}/libtorch/usr/install/lib/*.so ${D}${libdir}
    cp -r  ${B}/libtorch/usr/install/include/* ${D}${includedir}
    cp -r  ${B}/libtorch/usr/install/share/* ${D}/usr/share

    chown -R root:root ${D}
    chmod -R u=rwX,go=rX ${D}
}

do_install:append() {
    cd ${D}${libdir}

    for lib in libtorch libc10 libtorch_cpu libtorch_global_deps; do
        if [ -f ${lib}.so ]; then
            mv ${lib}.so ${lib}.so.1.0
            ln -sf ${lib}.so.1.0 ${lib}.so.1
            ln -sf ${lib}.so.1 ${lib}.so
        fi
    done
}

FILES:${PN} += "${libdir}/lib*.so \
                ${libdir}/lib*.so.* \
                /usr/share/cmake \
                /usr/share/ATen \
                /usr/share/cpuinfo \"
FILES:${PN}-dev:remove = "${libdir}/lib*.so"
INSANE_SKIP:${PN} += "dev-so"
