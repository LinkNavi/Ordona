project('ordona', 'cpp',
        version : '0.1.0',
        default_options : ['warning_level=3', 'cpp_std=c++17']
)

cc = meson.get_compiler('cpp')

is_windows = host_machine.system() == 'windows'
is_linux   = host_machine.system() == 'linux'
is_macos   = host_machine.system() == 'darwin'

# replxx sources
replxx_src = files(
    'replxx/src/ConvertUTF.cpp',
    'replxx/src/conversion.cxx',
    'replxx/src/escape.cxx',
    'replxx/src/history.cxx',
    'replxx/src/prompt.cxx',
    'replxx/src/replxx.cxx',
    'replxx/src/replxx_impl.cxx',
    'replxx/src/terminal.cxx',
    'replxx/src/util.cxx',
    'replxx/src/wcwidth.cpp',
)

if is_windows
    replxx_src += files('replxx/src/windows.cxx')
endif

sources = files(
    'src/main.cpp',
    'src/ordona.cpp',
    'src/predictor.cpp',
)

if is_windows
    sources += files('src/consoleWin.cpp')
else
    sources += files('src/consoleUnix.cpp')
endif

inc = include_directories('include', 'replxx/include')

deps = []
if is_windows
    deps += cc.find_library('kernel32')
endif

extra_args = []
if is_windows
    extra_args += ['-D_WIN32_WINNT=0x0601', '-DUNICODE']
else
    extra_args += ['-D_POSIX_C_SOURCE=200809L']
endif

executable('ordona',
           sources + replxx_src,
           dependencies : deps,
           include_directories : inc,
           cpp_args : extra_args,
           install : true
)
