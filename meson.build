project('ordona', 'cpp',
        version : '0.1.0',
        default_options : ['warning_level=3', 'cpp_std=c++17']
)

cc = meson.get_compiler('cpp')

# Platform detection
is_windows = host_machine.system() == 'windows'
is_linux   = host_machine.system() == 'linux'
is_macos   = host_machine.system() == 'darwin'

# Source files
sources = files(
    'src/main.cpp',
'src/ordona.cpp',
)

inc = include_directories('include')

# Platform-specific sources
if is_windows
    sources += files( 'src/consoleWin.cpp')
else
    sources += files( 'src/consoleUnix.cpp')
endif

# Dependencies
readline_dep = dependency('readline', required : false)  # placeholder

if not is_windows
    readline_dep = dependency('readline', required : false)
    if not readline_dep.found()
        readline_dep = cc.find_library('readline', required : false)
    endif
endif

deps = []
if readline_dep.found()
    deps += readline_dep
    add_project_arguments('-DHAVE_READLINE', language : 'cpp')
endif

# Windows-specific libs
if is_windows
    deps += cc.find_library('kernel32')
endif

# Compile flags
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
