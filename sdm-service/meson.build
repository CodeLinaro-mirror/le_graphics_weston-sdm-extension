dep_libsdmcore = cxx.find_library('sdmcore')
dep_libsdmutils = cxx.find_library('sdmutils')
#dep_libhdr = cxx.find_library('hdr_tm')
dep_libdisplaydebug =  cxx.find_library('displaydebug')

srcs_sdm = [
	'sdm_display.cpp',
	'sdm_display_debugger.cpp',
	'sdm_display_buffer_allocator.cpp',
	'sdm_display_buffer_sync_handler.cpp',
	'sdm_display_socket_handler.cpp',
	'sdm_display_connect.cpp',
	'sdm_layer.cpp',
	'uevent.c',
	linux_dmabuf_unstable_v1_server_protocol_h,
]

deps_sdm = [
	dep_libsdmcore,
	dep_libsdmutils,
	dep_libdrmutils,
	#	dep_libhdr,
	dep_libdrm,
	dep_gbm,
	dep_libdisplaydebug,
	dep_libweston_private,
	#	dep_libsync,
	dep_threads,
	dep_gbm_buffer_backend,
]



sdm_service = shared_library(
	'sdm-service',
	srcs_sdm,
	include_directories: common_inc,
	dependencies: deps_sdm,
	name_prefix: '',
	install: true,
	install_dir: dir_module_libweston
)
