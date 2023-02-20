project('weston-extension',
	'c', 'cpp',
	version: '1.0.0',
	default_options: [
		'warning_level=3',
		'c_std=gnu99',
		'cpp_std=c++17',
		'b_lundef=true',
	],
	meson_version: '>= 0.52.1',
	license: 'BSD',
)

libweston_major = 10
version_weston_extension = meson.project_version()

dir_prefix = get_option('prefix')
dir_lib = join_paths(dir_prefix, get_option('libdir'))
dir_module_weston = join_paths(dir_lib, 'weston')
dir_module_libweston = join_paths(dir_lib, 'libweston-@0@'.format(libweston_major))

common_inc = include_directories('.')

config_h = configuration_data()

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

global_args = []
global_args_maybe = [
	'-Wmissing-prototypes',
	'-Wno-unused-parameter',
	'-Wno-shift-negative-value', # required due to Pixman
	'-Wno-missing-field-initializers',
	'-Wno-pedantic',
	'-Wundef',
	'-fvisibility=hidden',
]
foreach a : global_args_maybe
	if cc.has_argument(a)
		global_args += a
	endif
endforeach
add_global_arguments(global_args, language: 'c')
add_global_arguments(global_args, language: 'cpp')

if cc.has_header_symbol('sys/sysmacros.h', 'major')
	config_h.set('MAJOR_IN_SYSMACROS', 1)
elif cc.has_header_symbol('sys/mkdev.h', 'major')
	config_h.set('MAJOR_IN_MKDEV', 1)
endif

optional_libc_funcs = [
	'mkostemp', 'strchrnul', 'initgroups', 'posix_fallocate', 'memfd_create'
]
foreach func : optional_libc_funcs
	if cc.has_function(func)
		config_h.set('HAVE_' + func.to_upper(), 1)
	endif
endforeach

optional_system_headers = [
	'linux/sync_file.h'
]
foreach hdr : optional_system_headers
	if cc.has_header(hdr)
		config_h.set('HAVE_' + hdr.underscorify().to_upper(), 1)
	endif
endforeach

config_h.set('_GNU_SOURCE', '1')
config_h.set('_ALL_SOURCE', '1')
config_h.set('EGL_NO_X11', '1')
config_h.set('MESA_EGL_NO_X11_HEADERS', '1')
config_h.set('EGL_NO_PLATFORM_SPECIFIC_TYPES', '1')

config_h.set_quoted('PACKAGE_STRING', 'weston extension')
config_h.set_quoted('PACKAGE_VERSION', version_weston_extension)
config_h.set_quoted('VERSION', version_weston_extension)

config_h.set_quoted('MODULEDIR', dir_module_weston)
config_h.set_quoted('LIBWESTON_MODULEDIR', dir_module_libweston)

dep_xkbcommon = dependency('xkbcommon', version: '>= 0.3.0')
if dep_xkbcommon.version().version_compare('>= 0.5.0')
	config_h.set('HAVE_XKBCOMMON_COMPOSE', '1')
endif

dep_wayland_server = dependency('wayland-server', version: '>= 1.18.0')
dep_wayland_client = dependency('wayland-client', version: '>= 1.18.0')
dep_pixman = dependency('pixman-1', version: '>= 0.25.2')
dep_libinput = dependency('libinput', version: '>= 0.8.0')
dep_libevdev = dependency('libevdev')
dep_libm = cc.find_library('m')
dep_libdl = cc.find_library('dl')
dep_libdrm = dependency('libdrm', version: '>= 2.4.95')
dep_libdrm_headers = dep_libdrm.partial_dependency(compile_args: true)
dep_threads = dependency('threads')
dep_libcutils = cc.find_library('cutils')
dep_gbm = dependency('gbm', required: false)
dep_libdrmutils = cxx.find_library('drmutils')
dep_libsdedrm = cxx.find_library('sdedrm')
dep_libsync = cc.find_library('sync')
# Depends on specific libraries from weston
dep_libweston = dependency('libweston-10')
dep_libinput_backend = cc.find_library('libinput-backend')
dep_session_helper = cc.find_library('session-helper')
dep_backlight = cc.find_library('backlight')

dep_libdrm_version = dep_libdrm.version()
if dep_libdrm_version.version_compare('>=2.4.107')
  message('Found libdrm with human format modifier support.')
  config_h.set('HAVE_HUMAN_FORMAT_MODIFIER', '1')
endif

deps_for_libweston_users = [
	dep_wayland_server,
	dep_pixman,
	dep_xkbcommon,
	dep_libweston,
]

# For internal users, like sdm-backend.
dep_libweston_private = declare_dependency(
	dependencies: deps_for_libweston_users
)

subdir('protocol')
subdir('sdm-backend')
subdir('sdm-service')

configure_file(output: 'config.h', configuration: config_h)
