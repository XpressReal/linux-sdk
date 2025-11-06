obj-$(CONFIG_RTK_GPU_WRAP) += rtk-gpu_wrap.o
obj-$(CONFIG_RTK_GPU_CACHE) += rtk-gpu_cache.o

rtk-gpu_cache-y += rtk-gpu_cache_drv.o  \
		   rtk-gpu_cache_fops.o \
		   rtk-gpu_cache-rtd1319d.o \
		   rtk-gpu_cache-rtd1315e.o \
		   rtk-gpu_cache-rtd1625.o

rtk-gpu_cache-$(CONFIG_RTK_GPU_CACHE_CONTROL) += rtk-gpu_cache_control.o
