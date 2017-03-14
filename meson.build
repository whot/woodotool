project('woodotool', 'c', 'cpp',
        version : '0.0.0',
        licence : 'MIT/Expat')

cc = meson.get_compiler('c')
cppflags = ['-Wall', '-Wextra', '-Wno-unused-parameter', '-g', '-fvisibility=hidden']
cflags = cppflags + ['-Wmissing-prototypes', '-Wstrict-prototypes']
add_global_arguments(cflags,
		     language: 'c')
add_global_arguments(cppflags,
		     language: 'cpp')

config_h = configuration_data()
config_h.set('_GNU_SOURCE', '1')

dep_udev = dependency('libsystemd')
dep_xkbcommon = dependency('xkbcommon')

executable('woodotool-server',
	   'woodotool-server.c',
	   dependencies : [dep_udev, dep_xkbcommon],
	   install : false)

configure_file(output: 'config.h', install: false, configuration: config_h)
