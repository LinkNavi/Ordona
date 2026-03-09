project('ordona', 'cpp',
        version : '0.2.0',
        default_options : ['warning_level=3', 'cpp_std=c++17']
)

cc = meson.get_compiler('cpp')

is_windows = host_machine.system() == 'windows'

sources = files(
    'src/main.cpp',
    'src/ordona.cpp',
    'src/predictor.cpp',
    'src/line_editor.cpp',
)

if is_windows
    sources += files('src/consoleWin.cpp', 'src/terminalWin.cpp')
else
    sources += files('src/consoleUnix.cpp', 'src/terminalUnix.cpp')
endif

inc = include_directories('include')

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
           sources,
           dependencies : deps,
           include_directories : inc,
           cpp_args : extra_args,
           install : true
)
