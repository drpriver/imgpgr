project('imgpgr', 'c', version: '1.0.0')

if host_machine.system() == 'windows'
  ignore_bogus_deprecations = ['-D_CRT_NONSTDC_NO_WARNINGS', '-D_CRT_SECURE_NO_WARNINGS']
else
  ignore_bogus_deprecations = []
endif

if host_machine.system() == 'darwin'
  arches = ['-arch', 'x86_64', '-arch', 'arm64']
else
  arches = []
endif

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

executable('imgpgr', 'imgpgr.c', install:true, c_args:ignore_bogus_deprecations)
