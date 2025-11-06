SRC_URI = " \
    git://github.com/pytorch/pytorch;branch=main;protocol=https;name=main;destsuffix=pytorch \
    git://github.com/pybind/pybind11.git;protocol=https;destsuffix=pytorch/third_party/pybind11;name=third_party_pybind11;branch=stable \
    git://gitlab.com/libeigen/eigen.git;protocol=https;destsuffix=pytorch/third_party/eigen;name=third_party_eigen;branch=3.4 \
    git://github.com/google/googletest.git;protocol=https;destsuffix=pytorch/third_party/googletest;name=third_party_googletest;branch=v1.17.x \
    git://github.com/google/benchmark.git;protocol=https;destsuffix=pytorch/third_party/benchmark;name=third_party_benchmark;branch=main \
    git://github.com/protocolbuffers/protobuf.git;protocol=https;destsuffix=pytorch/third_party/protobuf;name=third_party_protobuf;branch=21.x \
    git://github.com/Maratyszcza/NNPACK.git;protocol=https;destsuffix=pytorch/third_party/NNPACK;name=third_party_NNPACK;branch=master \
    git://github.com/pytorch/gloo;protocol=https;destsuffix=pytorch/third_party/gloo;name=third_party_gloo;branch=main \
    git://github.com/Maratyszcza/pthreadpool.git;protocol=https;destsuffix=pytorch/third_party/pthreadpool;name=third_party_pthreadpool;branch=master \
    git://github.com/Maratyszcza/FXdiv.git;protocol=https;destsuffix=pytorch/third_party/FXdiv;name=third_party_FXdiv;branch=master \
    git://github.com/Maratyszcza/FP16.git;protocol=https;destsuffix=pytorch/third_party/FP16;name=third_party_FP16;branch=master \
    git://github.com/Maratyszcza/psimd.git;protocol=https;destsuffix=pytorch/third_party/psimd;name=third_party_psimd;branch=master \
    git://github.com/pytorch/cpuinfo.git;protocol=https;destsuffix=pytorch/third_party/cpuinfo;name=third_party_cpuinfo;branch=main \
    git://github.com/malfet/PeachPy.git;protocol=https;destsuffix=pytorch/third_party/python-peachpy;name=third_party_python-peachpy;branch=master \
    git://github.com/onnx/onnx.git;protocol=https;destsuffix=pytorch/third_party/onnx;name=third_party_onnx;branch=rel-1.18.0 \
    git://github.com/shibatch/sleef;protocol=https;destsuffix=pytorch/third_party/sleef;name=third_party_sleef;branch=master \
    git://github.com/intel/ideep;protocol=https;destsuffix=pytorch/third_party/ideep;name=third_party_ideep;branch=ideep_pytorch \
    git://github.com/google/gemmlowp.git;protocol=https;destsuffix=pytorch/third_party/gemmlowp/gemmlowp;name=third_party_gemmlowp_gemmlowp;branch=master \
    git://github.com/pytorch/fbgemm;protocol=https;destsuffix=pytorch/third_party/fbgemm;name=third_party_fbgemm;branch=main \
    git://github.com/facebookincubator/fbjni.git;protocol=https;destsuffix=pytorch/android/libs/fbjni;name=android_libs_fbjni;branch=main \
    git://github.com/google/XNNPACK.git;protocol=https;destsuffix=pytorch/third_party/XNNPACK;name=third_party_XNNPACK;branch=master \
    git://github.com/fmtlib/fmt.git;protocol=https;destsuffix=pytorch/third_party/fmt;name=third_party_fmt;branch=master \
    git://github.com/pytorch/tensorpipe.git;protocol=https;destsuffix=pytorch/third_party/tensorpipe;name=third_party_tensorpipe;branch=main \
    git://github.com/NVIDIA/cudnn-frontend.git;protocol=https;destsuffix=pytorch/third_party/cudnn_frontend;name=third_party_cudnn_frontend;branch=main \
    git://github.com/pytorch/kineto;protocol=https;destsuffix=pytorch/third_party/kineto;name=third_party_kineto;branch=main \
    git://github.com/mreineck/pocketfft;protocol=https;destsuffix=pytorch/third_party/pocketfft;name=third_party_pocketfft;branch=cpp \
    git://github.com/intel/ittapi.git;protocol=https;destsuffix=pytorch/third_party/ittapi;name=third_party_ittapi;branch=master \
    git://github.com/google/flatbuffers.git;protocol=https;destsuffix=pytorch/third_party/flatbuffers;name=third_party_flatbuffers;branch=master \
    git://github.com/nlohmann/json.git;protocol=https;destsuffix=pytorch/third_party/nlohmann;name=third_party_nlohmann;branch=master \
    git://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git;protocol=https;destsuffix=pytorch/third_party/VulkanMemoryAllocator;name=third_party_VulkanMemoryAllocator;branch=master \
    git://github.com/NVIDIA/cutlass.git;protocol=https;destsuffix=pytorch/third_party/cutlass;name=third_party_cutlass;branch=main \
    git://github.com/microsoft/mimalloc.git;protocol=https;destsuffix=pytorch/third_party/mimalloc;name=third_party_mimalloc;branch=main \
    git://github.com/open-telemetry/opentelemetry-cpp.git;protocol=https;destsuffix=pytorch/third_party/opentelemetry-cpp;name=third_party_opentelemetry-cpp;branch=main \
    git://github.com/yhirose/cpp-httplib.git;protocol=https;destsuffix=pytorch/third_party/cpp-httplib;name=third_party_cpp-httplib;branch=master \
    git://github.com/NVIDIA/NVTX.git;protocol=https;destsuffix=pytorch/third_party/NVTX;name=third_party_NVTX;branch=release-v3 \
    git://github.com/ROCm/composable_kernel.git;protocol=https;destsuffix=pytorch/third_party/composable_kernel;name=third_party_composable_kernel;branch=develop \
    git://github.com/ARM-software/kleidiai.git;protocol=https;destsuffix=pytorch/third_party/kleidiai;name=third_party_kleidiai;branch=main \
    git://github.com/Dao-AILab/flash-attention.git;protocol=https;destsuffix=pytorch/third_party/flash-attention;name=third_party_flash-attention;branch=main \
"

SRCREV_main = "28796f71d04302029290f473a286efc2aba339c2"
SRCREV_third_party_pybind11 = "a2e59f0e7065404b44dfe92a28aca47ba1378dc4"
SRCREV_third_party_eigen = "3147391d946bb4b6c68edd901f2add6ac1f31f8c"
SRCREV_third_party_googletest = "52eb8108c5bdec04579160ae17225d66034bd723"
SRCREV_third_party_benchmark = "299e5928955cc62af9968370293b916f5130916f"
SRCREV_third_party_protobuf = "d1eca4e4b421cd2997495c4b4e65cea6be4e9b8a"
SRCREV_third_party_NNPACK = "c07e3a0400713d546e0dea2d5466dd22ea389c73"
SRCREV_third_party_gloo = "c7b7b022c124d9643957d9bd55f57ac59fce8fa2"
SRCREV_third_party_pthreadpool = "4fe0e1e183925bf8cfa6aae24237e724a96479b8"
SRCREV_third_party_FXdiv = "b408327ac2a15ec3e43352421954f5b1967701d1"
SRCREV_third_party_FP16 = "4dfe081cf6bcd15db339cf2680b9281b8451eeb3"
SRCREV_third_party_psimd = "072586a71b55b7f8c584153d223e95687148a900"
SRCREV_third_party_cpuinfo = "5e3d2445e6a84d9599bee2bf78edbb4d80865e1d"
SRCREV_third_party_python-peachpy = "913d74c35a6b1d330e90bfc055208ce5b06b35a0"
SRCREV_third_party_onnx = "e709452ef2bbc1d113faf678c24e6d3467696e83"
SRCREV_third_party_sleef = "5a1d179df9cf652951b59010a2d2075372d67f68"
SRCREV_third_party_ideep = "719d8e6cd7f7a0e01b155657526d693acf97c2b3"
SRCREV_third_party_gemmlowp_gemmlowp = "3fb5c176c17c765a3492cd2f0321b0dab712f350"
SRCREV_third_party_fbgemm = "157e88b750c452bef2ab4653fe9d1eeb151ce4c3"
SRCREV_android_libs_fbjni = "7e1e1fe3858c63c251c637ae41a20de425dde96f"
SRCREV_third_party_XNNPACK = "51a0103656eff6fc9bfd39a4597923c4b542c883"
SRCREV_third_party_fmt = "40626af88bd7df9a5fb80be7b25ac85b122d6c21"
SRCREV_third_party_tensorpipe = "52791a2fd214b2a9dc5759d36725909c1daa7f2e"
SRCREV_third_party_cudnn_frontend = "666996fe3960f27170d1527e5579ba24c8d3380a"
SRCREV_third_party_kineto = "5e7501833f1021ce6f618572d3baf657b6319658"
SRCREV_third_party_pocketfft = "0fa0ef591e38c2758e3184c6c23e497b9f732ffa"
SRCREV_third_party_ittapi = "dec1d23ca65ab069d225dfe40dea14f455170959"
SRCREV_third_party_flatbuffers = "a2cd1ea3b6d3fee220106b5fed3f7ce8da9eb757"
SRCREV_third_party_nlohmann = "55f93686c01528224f448c19128836e7df245f72"
SRCREV_third_party_VulkanMemoryAllocator = "1d8f600fd424278486eade7ed3e877c99f0846b1"
SRCREV_third_party_cutlass = "ad7b2f5e84fcfa124cb02b91d5bd26d238c0459e"
SRCREV_third_party_mimalloc = "94036de6fe20bfd8a73d4a6d142fcf532ea604d9"
SRCREV_third_party_opentelemetry-cpp = "a799f4aed9c94b765dcdaabaeab7d5e7e2310878"
SRCREV_third_party_cpp-httplib = "3af7f2c16147f3fbc6e4d717032daf505dc1652c"
SRCREV_third_party_NVTX = "2942f167cc30c5e3a44a2aecd5b0d9c07ff61a07"
SRCREV_third_party_composable_kernel = "8086bbe3a78d931eb96fe12fdc014082e18d18d3"
SRCREV_third_party_kleidiai = "cca02c2f69dd18e1f12647c1c0bdc8cf90e680c7"
SRCREV_third_party_flash-attention = "979702c87a8713a8e0a5e9fee122b90d2ef13be5"

SRCREV_FORMAT = "main third_party_pybind11 third_party_eigen third_party_googletest third_party_benchmark third_party_protobuf third_party_NNPACK third_party_gloo third_party_pthreadpool third_party_FXdiv third_party_FP16 third_party_psimd third_party_cpuinfo third_party_python-peachpy third_party_onnx third_party_sleef third_party_ideep third_party_gemmlowp_gemmlowp third_party_fbgemm android_libs_fbjni third_party_XNNPACK third_party_fmt third_party_tensorpipe third_party_cudnn_frontend third_party_kineto third_party_pocketfft third_party_ittapi third_party_flatbuffers third_party_nlohmann third_party_VulkanMemoryAllocator third_party_cutlass third_party_mimalloc third_party_opentelemetry-cpp third_party_cpp-httplib third_party_NVTX third_party_composable_kernel third_party_kleidiai third_party_flash-attention"

SRC_URI += " \
        git://github.com/asmjit/asmjit.git;protocol=https;destsuffix=pytorch/third_party/fbgemm/external/asmjit;name=fbgemm_external_asmjit;branch=master \
        git://github.com/pytorch/cpuinfo;protocol=https;destsuffix=pytorch/third_party/fbgemm/external/cpuinfo;name=fbgemm_external_cpuinfo;branch=main \
        git://github.com/google/googletest;protocol=https;destsuffix=pytorch/third_party/fbgemm/external/googletest;name=fbgemm_external_googletest;branch=main \
        git://github.com/ROCmSoftwarePlatform/hipify_torch.git;protocol=https;destsuffix=pytorch/third_party/fbgemm/external/hipify_torch;name=fbgemm_external_hipify_torch;branch=master \
        git://github.com/jwfromm/cutlass;protocol=https;destsuffix=pytorch/third_party/fbgemm/external/cutlass;name=fbgemm_external_cutlass;branch=main \
        git://github.com/jwfromm/composable_kernel.git;protocol=https;destsuffix=pytorch/third_party/fbgemm/external/composable_kernel;name=fbgemm_external_composable_kernel;branch=develop \
        git://github.com/nlohmann/json.git;protocol=https;destsuffix=pytorch/third_party/fbgemm/external/json;name=fbgemm_external_json;branch=add-ci-step-windows \
"

SRCREV_fbgemm_external_asmjit = "e5d7c0bd5d9aec44d68830187138149e6a8c4e32"
SRCREV_fbgemm_external_cpuinfo = "6543fec09b2f04ac4a666882998b534afc9c1349"
SRCREV_fbgemm_external_googletest = "f8d7d77c06936315286eb55f8de22cd23c188571"
SRCREV_fbgemm_external_hipify_torch = "a4337c69fe0e2552a7b7b0669178926beeed828c"
SRCREV_fbgemm_external_cutlass = "7de6a5978463c2ddf9a79305bbea3a701cb5057f"
SRCREV_fbgemm_external_composable_kernel = "7142d8003c6a99f952a62bbd0b90d5f0261fc807"
SRCREV_fbgemm_external_json = "9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03"

SRCREV_FORMAT += "fbgemm_external_asmjit fbgemm_external_cpuinfo fbgemm_external_googletest fbgemm_external_hipify_torch fbgemm_external_cutlass fbgemm_external_composable_kernel fbgemm_external_json "

SRC_URI += " \
        git://github.com/google/benchmark.git;protocol=https;destsuffix=pytorch/third_party/protobuf/third_party/benchmark;name=protobuf_third_party_benchmark;branch=main \
        git://github.com/google/googletest.git;protocol=https;destsuffix=pytorch/third_party/protobuf/third_party/googletest;name=protobuf_third_party_googletest;branch=main \
"
SRCREV_protobuf_third_party_benchmark = "5b7683f49e1e9223cf9927b24f6fd3d6bd82e3f8"
SRCREV_protobuf_third_party_googletest = "5ec7f0c4a113e2f18ac2c6cc7df51ad6afc24081"

SRCREV_FORMAT += "protobuf_third_party_benchmark protobuf_third_party_googletest"

SRC_URI += " \
        git://github.com/jupp0r/prometheus-cpp;protocol=https;destsuffix=pytorch/third_party/opentelemetry-cpp/third_party/prometheus-cpp;name=opentelemetry-cpp_third_party_prometheus-cpp;branch=master \
        git://github.com/Microsoft/vcpkg;protocol=https;destsuffix=pytorch/third_party/opentelemetry-cpp/tools/vcpkg;name=opentelemetry-cpp_tools_vcpkg;branch=master \
        git://github.com/microsoft/GSL;protocol=https;destsuffix=pytorch/third_party/opentelemetry-cpp/third_party/ms-gsl;name=opentelemetry-cpp_third_party_ms-gsl;branch=main \
        git://github.com/google/googletest;protocol=https;destsuffix=pytorch/third_party/opentelemetry-cpp/third_party/googletest;name=opentelemetry-cpp_third_party_googletest;branch=main \
        git://github.com/google/benchmark;protocol=https;destsuffix=pytorch/third_party/opentelemetry-cpp/third_party/benchmark;name=opentelemetry-cpp_third_party_benchmark;branch=main \
        git://github.com/open-telemetry/opentelemetry-proto;protocol=https;destsuffix=pytorch/third_party/opentelemetry-cpp/third_party/opentelemetry-proto;name=opentelemetry-cpp_third_party_opentelemetry-proto;branch=main \
        git://github.com/nlohmann/json;protocol=https;destsuffix=pytorch/third_party/opentelemetry-cpp/third_party/nlohmann-json;name=opentelemetry-cpp_third_party_nlohmann-json;branch=develop \
        git://github.com/opentracing/opentracing-cpp.git;protocol=https;destsuffix=pytorch/third_party/opentelemetry-cpp/third_party/opentracing-cpp;name=opentelemetry-cpp_third_party_opentracing-cpp;branch=master \
"

SRCREV_opentelemetry-cpp_third_party_prometheus-cpp = "c9ffcdda9086ffd9e1283ea7a0276d831f3c8a8d"
SRCREV_opentelemetry-cpp_tools_vcpkg = "8eb57355a4ffb410a2e94c07b4dca2dffbee8e50"
SRCREV_opentelemetry-cpp_third_party_ms-gsl = "6f4529395c5b7c2d661812257cd6780c67e54afa"
SRCREV_opentelemetry-cpp_third_party_googletest = "b796f7d44681514f58a683a3a71ff17c94edb0c1"
SRCREV_opentelemetry-cpp_third_party_benchmark = "d572f4777349d43653b21d6c2fc63020ab326db2"
SRCREV_opentelemetry-cpp_third_party_opentelemetry-proto = "4ca4f0335c63cda7ab31ea7ed70d6553aee14dce"
SRCREV_opentelemetry-cpp_third_party_nlohmann-json = "bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d"
SRCREV_opentelemetry-cpp_third_party_opentracing-cpp = "06b57f48ded1fa3bdd3d4346f6ef29e40e08eaf5"

SRCREV_FORMAT += "copentelemetry-cpp_third_party_prometheus-cpp opentelemetry-cpp_tools_vcpkg opentelemetry-cpp_third_party_ms-gsl opentelemetry-cpp_third_party_googletest opentelemetry-cpp_third_party_benchmark_opentelemetry-cpp_third_party_opentelemetry-proto opentelemetry-cpp_third_party_nlohmann-json opentelemetry-cpp_third_party_opentracing-cpp "

SRC_URI += " \
        git://github.com/google/googletest.git;protocol=https;destsuffix=pytorch/third_party/opentelemetry-cpp/third_party/prometheus-cpp/3rdparty/googletest;name=opentelemetry-cpp_third_party_prometheus-cpp_3rdparty_googletest;branch=main \
        git://github.com/civetweb/civetweb.git;protocol=https;destsuffix=pytorch/third_party/opentelemetry-cpp/third_party/prometheus-cpp/3rdparty/civetweb;name=opentelemetry-cpp_third_party_prometheus-cpp_3rdparty_civetweb;branch=master \
"

SRCREV_opentelemetry-cpp_third_party_prometheus-cpp_3rdparty_googletest = "e2239ee6043f73722e7aa812a459f54a28552929"
SRCREV_opentelemetry-cpp_third_party_prometheus-cpp_3rdparty_civetweb = "eefb26f82b233268fc98577d265352720d477ba4"

SRCREV_FORMAT += "opentelemetry-cpp_third_party_prometheus-cpp_3rdparty_googletest opentelemetry-cpp_third_party_prometheus-cpp_3rdparty_civetweb "

SRC_URI += " \
        git://github.com/google/googletest.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/googletest;name=opentelemetry-cpp_third_party_prometheus-cpp_libkineto_third_party_googletest;branch=main \
        git://github.com/fmtlib/fmt.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/fmt;name=opentelemetry-cpp_third_party_prometheus-cpp_libkineto_third_party_fmt;branch=master \
        git://github.com/facebookincubator/dynolog.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog;name=opentelemetry-cpp_third_party_prometheus-cpp_libkineto_third_party_dynolog;branch=main \
"

SRCREV_opentelemetry-cpp_third_party_prometheus-cpp_libkineto_third_party_googletest = "7aca84427f224eeed3144123d5230d5871e93347"
SRCREV_opentelemetry-cpp_third_party_prometheus-cpp_libkineto_third_party_fmt = "0041a40c1350ba702d475b9c4ad62da77caea164"
SRCREV_opentelemetry-cpp_third_party_prometheus-cpp_libkineto_third_party_dynolog = "7d04a0053a845370ae06ce317a22a48e9edcc74e"

SRCREV_FORMAT += "opentelemetry-cpp_third_party_prometheus-cpp_libkineto_third_party_googletest opentelemetry-cpp_third_party_prometheus-cpp_libkineto_third_party_fmt opentelemetry-cpp_third_party_prometheus-cpp_libkineto_third_party_dynolog_"

SRC_URI += " \
        git://github.com/google/googletest.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/googletest;name=kineto_libkineto_third_party_googletest;branch=main \
        git://github.com/fmtlib/fmt.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/fmt;name=kineto_libkineto_third_party_fmt;branch=master \
        git://github.com/facebookincubator/dynolog.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog;name=kineto_libkineto_third_party_dynolog;branch=main \
"

SRCREV_kineto_libkineto_third_party_googletest = "7aca84427f224eeed3144123d5230d5871e93347"
SRCREV_kineto_libkineto_third_party_fmt = "0041a40c1350ba702d475b9c4ad62da77caea164"
SRCREV_kineto_libkineto_third_party_dynolog = "7d04a0053a845370ae06ce317a22a48e9edcc74e"

SRCREV_FORMAT += "kineto_libkineto_third_party_googletest kineto_libkineto_third_party_fmt kineto_libkineto_third_party_dynolog_"

SRC_URI += " \
        git://github.com/google/glog.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/glog;name=kineto_libkineto_third_party_dynolog_third_party_glog;branch=master \
        git://github.com/gflags/gflags.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/gflags;name=kineto_libkineto_third_party_dynolog_third_party_gflags;branch=master \
        git://github.com/fmtlib/fmt.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/fmt;name=kineto_libkineto_third_party_dynolog_third_party_fmt;branch=master \
        git://github.com/nlohmann/json.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/json;name=kineto_libkineto_third_party_dynolog_third_party_json;branch=develop \
        git://github.com/dtrugman/pfs.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/pfs;name=kineto_libkineto_third_party_dynolog_third_party_pfs;branch=main \
        git://github.com/google/googletest.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/googletest;name=kineto_libkineto_third_party_dynolog_third_party_googletest;branch=v1.12.x \
        git://github.com/libcpr/cpr.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/cpr;name=kineto_libkineto_third_party_dynolog_third_party_cpr;branch=master \
        git://github.com/NVIDIA/DCGM.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/DCGM;name=kineto_libkineto_third_party_dynolog_third_party_DCGM;branch=master \
"

SRCREV_kineto_libkineto_third_party_dynolog_third_party_glog = "b33e3bad4c46c8a6345525fd822af355e5ef9446"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_gflags = "e171aa2d15ed9eb17054558e0b3a6a413bb01067"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_fmt = "cd4af11efc9c622896a3e4cb599fa28668ca3d05"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_json = "4f8fba14066156b73f1189a2b8bd568bde5284c5"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_pfs = "f68a2fa8ea36c783bdd760371411fcb495aa3150"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_googletest = "58d77fa8070e8cec2dc1ed015d66b454c8d78850"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_cpr = "871ed52d350214a034f6ef8a3b8f51c5ce1bd400"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_DCGM = "ffde4e54bc7249a6039a5e6b45b395141e1217f9"

SRCREV_FORMAT += "kineto_libkineto_third_party_dynolog_third_party_glog kineto_libkineto_third_party_dynolog_third_party_gflags kineto_libkineto_third_party_dynolog_third_party_fmt kineto_libkineto_third_party_dynolog_third_party_json kineto_libkineto_third_party_dynolog_third_party_pfs kineto_libkineto_third_party_dynolog_third_party_googletest kineto_libkineto_third_party_dynolog_third_party_cpr kineto_libkineto_third_party_dynolog_third_party_DCGM "

SRC_URI += " \
        git://github.com/google/glog.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/glog;name=kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_glog;branch=master \
        git://github.com/gflags/gflags.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/gflags;name=kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_gflags;branch=master \
        git://github.com/fmtlib/fmt.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/fmt;name=kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_fmt;branch=master \
        git://github.com/nlohmann/json.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/json;name=kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_json;branch=develop \
        git://github.com/dtrugman/pfs.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/pfs;name=kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_pfs;branch=main \
        git://github.com/google/googletest.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/googletest;name=kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_googletest;branch=v1.12.x \
        git://github.com/libcpr/cpr.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/cpr;name=kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_cpr;branch=master \
        git://github.com/NVIDIA/DCGM.git;protocol=https;destsuffix=pytorch/third_party/kineto/libkineto/third_party/dynolog/third_party/DCGM;name=kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_DCGM;branch=master \
"

SRCREV_kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_glog = "b33e3bad4c46c8a6345525fd822af355e5ef9446"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_gflags = "e171aa2d15ed9eb17054558e0b3a6a413bb01067"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_fmt = "cd4af11efc9c622896a3e4cb599fa28668ca3d05"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_json = "4f8fba14066156b73f1189a2b8bd568bde5284c5"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_pfs = "f68a2fa8ea36c783bdd760371411fcb495aa3150"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_googletest = "58d77fa8070e8cec2dc1ed015d66b454c8d78850"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_cpr = "871ed52d350214a034f6ef8a3b8f51c5ce1bd400"
SRCREV_kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_DCGM = "ffde4e54bc7249a6039a5e6b45b395141e1217f9"

SRCREV_FORMAT += "kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_glog kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_gflags kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_fmt kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_json kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_pfs kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_googletest kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_cpr kineto_libkineto_third_party_dynolog_third_party_gflags_third_party_DCGM "

SRC_URI += " \
        git://github.com/intel/mkl-dnn.git;protocol=https;destsuffix=pytorch/third_party/ideep/mkl-dnn;name=ideep_mkl-dnn;branch=rls-v3.7 \
"

SRCREV_ideep_mkl-dnn = "8d263e693366ef8db40acc569cc7d8edf644556d"
SRCREV_FORMAT += "ideep_mkl-dnn"

SRC_URI += " \
        git://github.com/NVIDIA/cutlass.git;protocol=https;destsuffix=pytorch/third_party/flash-attention/csrc/cutlass;name=flash-attention_csrc_cutlass;branch=main \
        git://github.com/ROCm/composable_kernel.git;protocol=https;destsuffix=pytorch/third_party/flash-attention/csrc/composable_kernel;name=flash-attention_csrc_composable_kernel;branch=develop \
"

SRCREV_flash-attention_csrc_cutlass = "c506e16788cb08416a4a57e11a9067beeee29420"
SRCREV_flash-attention_csrc_composable_kernel = "888317e698e9803c62bd38568abc9e05d7709f33"

SRCREV_FORMAT += "flash-attention_csrc_cutlass flash-attention_csrc_composable_kernel"

SRC_URI += " \
        git://github.com/pybind/pybind11.git;protocol=https;destsuffix=pytorch/third_party/tensorpipe/third_party/pybind11;name=tensorpipe_third_party_pybind11;branch=v2.2 \
        git://github.com/libuv/libuv.git;protocol=https;destsuffix=pytorch/third_party/tensorpipe/third_party/libuv;name=tensorpipe_third_party_libuv;branch=master \
        git://github.com/google/googletest.git;protocol=https;destsuffix=pytorch/third_party/tensorpipe/third_party/googletest;name=tensorpipe_third_party_googletest;branch=main \
        git://github.com/google/libnop.git;protocol=https;destsuffix=pytorch/third_party/tensorpipe/third_party/libnop;name=tensorpipe_third_party_libnop;branch=master \
"

SRCREV_tensorpipe_third_party_pybind11 = "a23996fce38ff6ccfbcdc09f1e63f2c4be5ea2ef"
SRCREV_tensorpipe_third_party_libuv = "1dff88e5161cba5c59276d2070d2e304e4dcb242"
SRCREV_tensorpipe_third_party_googletest = "aee0f9d9b5b87796ee8a0ab26b7587ec30e8858e"
SRCREV_tensorpipe_third_party_libnop = "910b55815be16109f04f4180e9adee14fb4ce281"

SRCREV_FORMAT += "tensorpipe_third_party_pybind11 tensorpipe_third_party_libuv tensorpipe_third_party_googletest tensorpipe_third_party_libnop "

SRC_URI += " \
        git://github.com/wjakob/clang-cindex-python3;protocol=https;destsuffix=pytorch/third_party/tensorpipe/third_party/pybind11/tools/clang;name=tensorpipe_third_party_pybind11_tools_clang;branch=master \
"

SRCREV_tensorpipe_third_party_pybind11_tools_clang = "6a00cbc4a9b8e68b71caf7f774b3f9c753ae84d5"

SRCREV_FORMAT += "tensorpipe_third_party_pybind11_tools_clang"

SRC_URI += " \
        git://github.com/pybind/pybind11.git;protocol=https;destsuffix=pytorch/third_party/onnx/third_party/pybind11;name=onnx_third_party_pybind11;branch=stable \
"

SRCREV_onnx_third_party_pybind11 = "a2e59f0e7065404b44dfe92a28aca47ba1378dc4"

SRCREV_FORMAT += "onnx_third_party_pybind11"
