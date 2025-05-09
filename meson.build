project('VLC', ['c', 'cpp'],
    version: '4.0.0-dev',
    default_options: ['c_std=gnu17', 'cpp_std=c++17'],
    meson_version: '>=1.1.0')

vlc_copyright_years = '1996-2025'
vlc_version_codename = 'Otto Chriek'

# LibVLC library (ABI) version
# Format must be major.minor.micro
libvlc_abi_version = '12.0.0'

libvlc_abi_version_parts = libvlc_abi_version.split('.')
libvlc_abi_version_major = libvlc_abi_version_parts[0].to_int()
libvlc_abi_version_minor = libvlc_abi_version_parts[1].to_int()
libvlc_abi_version_micro = libvlc_abi_version_parts[2].to_int()

vlc_version_full = meson.project_version()

vlc_version_parts = vlc_version_full.split('-')[0].split('.')
vlc_version_type  = vlc_version_full.split('-').get(1, '')

if (vlc_version_parts.length() < 3 or vlc_version_parts.length() > 4)
    error(f'Unexpected project version "@vlc_version_full@".',
          'Expected a format of major.minor.revision[.extra][-dev]')
endif

vlc_version_major    = vlc_version_parts[0].to_int()
vlc_version_minor    = vlc_version_parts[1].to_int()
vlc_version_revision = vlc_version_parts[2].to_int()
vlc_version_extra    = vlc_version_parts.get(3, '0').to_int()

# Short version (major.minor.revision)
vlc_version_short = f'@vlc_version_major@.@vlc_version_minor@.@vlc_version_revision@'

# Normal VLC version (major.minor.revision[-dev])
vlc_version = vlc_version_short + ((vlc_version_type != '') ? f'-@vlc_version_type@' : '')

vlc_package_name = meson.project_name().to_lower()

vlc_src_root = meson.current_source_dir()
vlc_build_root = meson.current_build_dir()

cdata = configuration_data()

gen_vlc_about = find_program('buildsystem/gen-vlc-about.py')
vlc_about = custom_target('vlc_about.h',
                        input:   ['COPYING', 'THANKS', 'AUTHORS'],
                        output:  ['vlc_about.h'],
                        command: [gen_vlc_about,
                                   '@INPUT0@',
                                   '@INPUT1@',
                                   '@INPUT2@',
                                   '@OUTPUT@'])

cargo_rustc_static_libs = find_program('buildsystem/cargo-rustc-static-libs.py')
cargo_output = find_program('buildsystem/cargo-output.py')

add_project_arguments('-DHAVE_CONFIG_H=1', language: ['c', 'cpp', 'objc'])

# If building with contribs, read the relevant paths from the machine file
# to use it during checks (check_header, find_library) later.
contrib_dir = meson.get_external_property('contrib_dir', '')

if contrib_dir != ''
    message('Using contribs: ' + contrib_dir)

    contrib_incdir = meson.get_external_property('contrib_incdir')
    contrib_libdir = meson.get_external_property('contrib_libdir')
    # TODO: Remove contrib_inc_args and use contrib_incdir directly
    # once upstream solution to
    # https://github.com/mesonbuild/meson/pull/1386#issuecomment-1353858080
    # is found.
    contrib_inc_args = [f'-I@contrib_incdir@']

    # Contrib depdendency
    # This should be used ONLY in cases where you can not otherwise
    # form a proper dependency (by using a dependency() or find_library())
    # to contribs. It will add the contrib include and library directory
    # to the target it is used with.
    contrib_dep = declare_dependency(
        link_args: '-L' + contrib_libdir,
        compile_args: contrib_inc_args)
else
    contrib_incdir = []
    contrib_libdir = []
    contrib_inc_args = []
    contrib_dep = dependency('', required: false)
endif

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
host_system = host_machine.system()

list_inc_dirs = ['.', 'include']
if cc.get_id() == 'msvc' or cc.get_id() == 'clang-cl'
    # extra POSIX headers not found in the Windows SDK
    list_inc_dirs += 'compat/windows'
endif

if host_system == 'darwin'
    add_languages('objc', native: false)
endif

if host_machine.cpu_family().startswith('x86')
    if add_languages('nasm', native: false, required: false)
        vlc_nasm_args = []
        if host_machine.cpu_family() == 'x86'
            vlc_nasm_args += ['-DARCH_X86_64=0']
            if host_system in ['darwin', 'windows']
                vlc_nasm_args += ['-DPREFIX']
            endif
        else
            vlc_nasm_args += ['-DARCH_X86_64=1']
            if host_system == 'darwin'
                vlc_nasm_args += ['-DPREFIX']
            endif
        endif
        add_project_arguments(vlc_nasm_args, language: ['nasm'])
        cdata.set('HAVE_X86ASM', 1)
    endif
endif

# Rust build system
cargo_bin = find_program('cargo', required: get_option('rust'))

#
# General feature defines
#
vlc_conf_prefix = ''

feature_defines = [
    ['_GNU_SOURCE', 1], # Enable GNU extensions on systems that have them
]

foreach d : feature_defines
    cdata.set(d.get(0), d.get(1))
    vlc_conf_prefix = vlc_conf_prefix + '#define @0@ @1@\n'.format(d.get(0), d.get(1))
endforeach

vlc_tests = []

#
# SIMD support
#
subdir('buildsystem/simd_checks')

#
# Check for global dependencies
# These are dependencies needed by libvlc or
# libvlccore and by some modules too.
#
# ATTENTION: Take care to follow the naming convetions:
# - Libraries found with find_lirary() must be named `name_lib`
# - Libraries (or Frameworks) found with dependency() must be
#   named `name_dep`
#

# zlib library
z_dep = dependency('zlib', required: false)
if z_dep.found()
    cdata.set('HAVE_ZLIB', 1)
endif

# Math library
m_lib = cc.find_library('m', required: false)

# Dynamic library loading library
dl_lib = cc.find_library('dl', required: false)

# iconv library
iconv_dep = dependency('iconv', required: false)
if not iconv_dep.found()
    # check in contribs
    if cc.has_function('iconv_open', prefix: vlc_conf_prefix + '#include <iconv.h>',
                       dependencies: [contrib_dep, cc.find_library('iconv', dirs: contrib_libdir, required: false)])
        iconv_dep = declare_dependency(
            dependencies: [contrib_dep, cc.find_library('iconv', dirs: contrib_libdir)])
    endif
endif
iconv_const_test = '''
    #include <stddef.h>
    #include <iconv.h>
    _Static_assert(_Generic((iconv),
        size_t (*)(iconv_t, const char **, size_t *, char **, size_t *) : 1, default: 0),
        "Const prototype not matched");
'''

if iconv_dep.found()
    cdata.set('HAVE_ICONV', 1)

    # Check if iconv() prototype uses const
    if cc.compiles(iconv_const_test, name: 'Test iconv() for const-using prototype', dependencies: iconv_dep)
        cdata.set('ICONV_CONST', 'const')
    else
        cdata.set('ICONV_CONST', '')
    endif
endif

if host_system == 'darwin'
    corefoundation_dep = dependency('CoreFoundation', required: true)
    foundation_dep = dependency('Foundation', required: true)
else
    corefoundation_dep = []
    foundation_dep = []
endif

# Gettext
intl_dep = dependency('intl', required: get_option('nls'))
if intl_dep.found()
    cdata.set('HAVE_GETTEXT', 1)
    cdata.set('ENABLE_NLS', 1)

    subdir('po')
endif

# Domain name i18n support via GNU libidn
idn_dep = dependency('libidn', required: false)
if idn_dep.found()
    cdata.set('HAVE_IDN', 1)
endif

# Threads
threads_dep = dependency('threads', required: true)

# Check for X11
if (get_option('x11')
    .disable_auto_if(host_system in ['darwin', 'windows'])
    .allowed())
    x11_dep = dependency('x11', required: get_option('x11'))
else
    x11_dep = disabler()
endif
if not x11_dep.found()
    cdata.set('X_DISPLAY_MISSING', 1)
endif

#
# Check for headers
#

check_c_headers = [
    ['stdbit.h'],
    ['stdckdint.h'],
    ['arpa/inet.h'],
    ['threads.h'],
    ['netinet/tcp.h', { 'prefix' : ['#include <sys/types.h>'] }],
    ['search.h'],
    ['sys/uio.h'],
    ['sys/socket.h'],
    ['net/if.h'],
    ['execinfo.h'],
    ['features.h'],
    ['getopt.h'],
    ['linux/dccp.h'],
    ['linux/magic.h'],
    ['netinet/udplite.h'],
    ['pthread.h'],
    ['poll.h'],
    ['sys/auxv.h'],
    ['sys/eventfd.h'],
    ['sys/mount.h', { 'prefix' : ['#include <sys/types.h>'] }],
    # Android API < 26 doesn't have a correct sys/shm.h implementation
    # Bionic >= 26 source code also says it's not useful because of SELinux.
    ['sys/shm.h', { 'exclude_host': ['android'] }],
    ['sys/soundcard.h'],
    ['valgrind/valgrind.h'],
    ['X11/Xlib.h'],
    ['xlocale.h'],
    ['zlib.h', { 'args' : [contrib_inc_args] }],
    ['wordexp.h'],
    ['dxgidebug.h'],
    ['GL/wglew.h',
        { 'prefix' : ['#include <windows.h>', '#include <GL/glew.h>'],
          'args' : [contrib_inc_args] }],
]

check_cpp_headers = []

foreach header : check_c_headers
    header_kwargs = header.get(1, {})

    if host_system in header_kwargs.get('exclude_host', [])
        continue
    endif

    # TODO: Once we require meson 1.0, drop the array join here
    # See: https://github.com/mesonbuild/meson/pull/11099
    if cc.check_header(header[0],
                       prefix: '\n'.join(header_kwargs.get('prefix', [])),
                       args: header_kwargs.get('args', []))
        cdata.set('HAVE_' + header[0].underscorify().to_upper(), 1)
    endif
endforeach

foreach header : check_cpp_headers
    header_kwargs = header.get(1, {})
    # TODO: Once we require meson 1.0, drop the array join here
    # See: https://github.com/mesonbuild/meson/pull/11099
    if cpp.check_header(header[0],
                       prefix: '\n'.join(header_kwargs.get('prefix', [])),
                       args: header_kwargs.get('args', []))
        cdata.set('HAVE_' + header[0].underscorify().to_upper(), 1)
    endif
endforeach

if not cdata.has('HAVE_STDBIT_H')
    list_inc_dirs += 'compat/stdbit'
endif
if not cdata.has('HAVE_STDCKDINT_H')
    list_inc_dirs += 'compat/stdckdint'
endif
vlc_include_dirs = include_directories(list_inc_dirs)

#
# Darwin specific checks
#

if host_system == 'darwin'

    # Check if compiling for iOS
    have_ios = cc.get_define('TARGET_OS_IPHONE',
        prefix: '#include <TargetConditionals.h>') == '1'

    # Check if compiling for tvOS
    have_tvos = cc.get_define('TARGET_OS_TV',
        prefix: '#include <TargetConditionals.h>') == '1'

    # If none of the above, assume compiling for macOS
    have_osx = not have_ios and not have_tvos

else
    have_ios = false
    have_tvos = false
    have_osx = false
endif

#
# Windows and MinGW checks
#

have_mingw = false
have_win_desktop = false
have_win_store = false
libcom_cppflags = []

if host_system == 'windows'

    # Defines needed for Windows
    windows_defines = [
        ['UNICODE',      1], # Define to 1 for Unicode (Wide Chars) APIs
    ]

windows_version_test = '''
#ifdef _WIN32_WINNT
# error _WIN32_WINNT already defined
#else
# include <sdkddkver.h>
# if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0601
#  error _WIN32_WINNT toolchain default high enough
# endif
#endif
'''
    if cc.compiles(windows_version_test, name: 'need _WIN32_WINNT defined to Win7')
        windows_defines += [
            ['_WIN32_WINNT', '0x0601'] # Define for Windows 7 APIs
        ]
    endif

    foreach d : windows_defines
        cdata.set(d.get(0), d.get(1))
        vlc_conf_prefix = vlc_conf_prefix + '#define @0@ @1@\n'.format(d.get(0), d.get(1))
    endforeach

    mingw_check = '''
    #ifndef __MINGW32__
    # error Not compiling with mingw
    #endif
    '''

    # Check if MinGW is used at all
    if cc.compiles(mingw_check)

        # Check which kind of MinGW
        mingw_version_major = cc.get_define('__MINGW64_VERSION_MAJOR',
            prefix: '#include <_mingw.h>')

        if mingw_version_major == ''
            error('Cannot compile with MinGW, use MinGW-w64 >= 6.0 instead.')
        endif

        # Check that MinGW w64 is at least 6.0
        if mingw_version_major.to_int() < 6
            error('MinGW-w64 6.0 or higher required!')
        endif

        have_mingw = true

        mingw_version_minor = cc.get_define('__MINGW64_VERSION_MINOR',
            prefix: '#include <_mingw.h>')

        mingw_version = '@0@.@1@'.format(mingw_version_major, mingw_version_minor)
        message('Using MinGW-w64 ' + mingw_version)

        # Defines needed for MinGW

        mingw_defines = []

ucrt_version_test = '''
#include <crtdefs.h>
#if !(defined(_UCRT) || (__MSVCRT_VERSION__ >= 0x1400) || (__MSVCRT_VERSION__ >= 0xE00 && __MSVCRT_VERSION__ < 0x1000))
# error This is NOT a UCRT build
#endif
'''
        if cc.compiles(ucrt_version_test, name: 'compiles with Universal C Runtime')
            # for UCRT build we use the standard compatibility define of UCRT
            mingw_defines += [
                ['__USE_MINGW_ANSI_STDIO',      0],
            ]
        else
            # Define to force use of MinGW printf
            mingw_defines += [
                ['__USE_MINGW_ANSI_STDIO',      1],
            ]
        endif

        foreach d : mingw_defines
            cdata.set(d.get(0), d.get(1))
            vlc_conf_prefix = vlc_conf_prefix + '#define @0@ @1@\n'.format(d.get(0), d.get(1))
        endforeach

        # fno-strict-aliasing is necessary for WRL and IID_PPV_ARGS to work safely
        # MSVC doesn't have this option but doesn't do pointer aliasing, so it
        # should work too
        libcom_cppflags += '-fno-strict-aliasing'
        if not cpp.has_argument(libcom_cppflags)
            error('-fno-strict-aliasing is necessary for Windows C++ modules')
        endif

        # Check for fnative-struct or mms-bitfields support for MinGW
        if cc.has_argument('-mms-bitfields')
            add_project_arguments('-mms-bitfields',
                language: ['c', 'cpp'])
            # Check for the warning flag without "-Wno-", GCC accepts
            # -Wno-<anything> for unsupported warnings, which can trigger
            # other warnings instead.
            if cc.has_argument('-Wincompatible-ms-struct')
                add_project_arguments('-Wno-incompatible-ms-struct',
                    language: ['c', 'cpp'])
            endif
        elif cc.has_argument('-fnative-struct')
            add_project_arguments('-fnative-struct',
                    language: ['c', 'cpp'])
        endif

        # DEP, ASLR, NO SEH
        add_project_link_arguments('-Wl,--nxcompat', '-Wl,--no-seh', '-Wl,--dynamicbase',
            language: ['c', 'cpp'])
    endif

    # Check if we are building for Windows Store
    if get_option('winstore_app')
        have_win_store = true
        windowsappcompat_lib = cc.find_library('windowsappcompat')
        add_project_dependencies(windowsappcompat_lib, language: ['c', 'cpp'])
    else
        have_win_desktop = true
    endif

endif

add_project_arguments(cc.get_supported_arguments([
    '-Wno-deprecated-copy', # Some Qt version are generating tons of warning that cannot be
                            # avoided so mute them
]), language: ['c', 'cpp', 'objc'])

add_project_arguments(cc.get_supported_arguments([
    '-Wextra',
    '-Wsign-compare',
    '-Wundef',
    '-Wpointer-arith',
    '-Wvolatile-register-var',
    '-Wformat',
    '-Wformat-security',
    '-Wduplicated-branches',
    '-Wduplicated-cond',
    '-Wbad-function-cast',
    '-Wwrite-strings',
    '-Wmissing-prototypes',
    '-Werror-implicit-function-declaration',
    '-Winit-self',
    '-Wlogical-op',
    '-Wshadow=local',
    '-Wmultistatement-macros',
    '-pipe'
]), language: ['c'])

add_project_arguments(cpp.get_supported_arguments([
    '-Wextra',
    '-Wsign-compare',
    '-Wundef',
    '-Wpointer-arith',
    '-Wvolatile-register-var',
    '-Wformat',
    '-Wformat-security',
    '-Wduplicated-branches',
    '-Wduplicated-cond',
]), language: ['cpp'])

if get_option('extra_checks')
    add_project_arguments(cc.get_supported_arguments([
        '-Werror=missing-field-initializers',
        '-Werror=format',
        '-Werror=return-mismatch',
        '-Werror=incompatible-pointer-types',
        '-Werror=restrict',
        '-Werror=int-conversion',
        '-Werror=implicit-int',
        '-Werror=declaration-missing-parameter-type',
        '-Werror=excess-initializers',
    ]), language: ['c'])
    add_project_arguments(cpp.get_supported_arguments([
        '-Werror=missing-field-initializers',
        '-Werror=format',
        '-Werror=excess-initializers',
    ]), language: ['cpp'])
endif

if get_option('branch_protection') \
    .require(host_machine.cpu_family() == 'aarch64', error_message: 'Branch protection is only available for AArch64') \
    .require(cc.has_argument('-mbranch-protection=standard'), error_message: 'Compiler does not support `-mbranch-protection`') \
    .allowed()
    add_project_arguments('-mbranch-protection=standard', language: ['c', 'cpp'])
endif

if cc.get_id() not in ['clang-cl']
    add_project_arguments(cc.get_supported_arguments([
        '-Wall', # too verbose with clang-cl
    ]), language: ['c', 'cpp'])
endif

add_project_arguments(cc.first_supported_argument(['-Werror-implicit-function-declaration', '-we4013']), language: ['c'])

#
# Check if other libs are needed
#
rt_lib = []
possible_rt_libs = ['rt']
if host_system != 'windows'
    possible_rt_libs += ['pthread']
endif
foreach l : possible_rt_libs
    possible_rt_lib_lib = cc.find_library(l, required: false)
    if possible_rt_lib_lib.found() and \
       cc.has_function('clock_nanosleep', dependencies: possible_rt_lib_lib)
        rt_lib = possible_rt_lib_lib
        break
    endif
endforeach

#
# Socket library checks
#

# Check for socket library
socket_libs = cc.find_library('socket', required: false)

# Check for function 'connect' (optionally link with socket lib if it exists)
if not cc.has_function('connect', prefix: vlc_conf_prefix + '#include <sys/socket.h>', dependencies: socket_libs)

    if host_system == 'windows'
        # If not found and on windows:
        socket_libs = []
        socket_libs += cc.find_library('iphlpapi', required: true)
        socket_libs += cc.find_library('ws2_32', required: true)
    endif
endif

# Define some strings for the function check since the required headers
# will differ based on the platform we're building for
if host_system == 'windows'
  arpa_inet_h = '#include <ws2tcpip.h>\n#include <windows.h>'
  net_if_h = '#include <windows.h>\n#include <iphlpapi.h>'
  getpid_h = '#include <process.h>'
  swab_h   = '#include <stdlib.h>'
else
  arpa_inet_h = '#include <arpa/inet.h>'
  net_if_h = '#include <net.if.h>'
  getpid_h = '#include <unistd.h>'
  swab_h   = '#include <unistd.h>'
endif

#
# Check for functions
# Entry format: [function, prefix]

# General functions
check_functions = [
    ['accept4',          '#include <sys/socket.h>'],
    ['dup3',             '#include <unistd.h>'],
    ['qsort_r',          '#include <stdlib.h>'],
    ['fcntl',            '#include <fcntl.h>'],
    ['flock',            '#include <sys/file.h>'],
    ['fstatvfs',         '#include <sys/statvfs.h>'],
    ['fstatat',          '#include <sys/stat.h>'],
    ['fork',             '#include <unistd.h>'],
    ['getmntent_r',      '#include <mntent.h>'],
    ['getpwuid_r',       '#include <pwd.h>'],
    ['isatty',           '#include <unistd.h>'],
    ['isatty',           '#include <io.h>'],
    ['memalign',         '#include <malloc.h>'],
    ['mkostemp',         '#include <unistd.h>'],
    ['mkostemp',         '#include <stdlib.h>'],
    ['mmap',             '#include <sys/mman.h>'],
    ['open_memstream',   '#include <stdio.h>'],
    ['pipe2',            '#include <unistd.h>'],
    ['posix_fadvise',    '#include <fcntl.h>'],
    ['strcoll',          '#include <string.h>'],
    ['wordexp',          '#include <wordexp.h>'],

    ['uselocale',        '#include <locale.h>'],
    ['uselocale',        '#include <xlocale.h>'],
    ['newlocale',        '#include <locale.h>'],
    ['newlocale',        '#include <xlocale.h>'],
    ['setlocale',        '#include <locale.h>'],

    ['getenv',           '#include <stdlib.h>'],

    ['if_nametoindex',   net_if_h],
    ['if_nameindex',     net_if_h],

    ['backtrace',        '#include <execinfo.h>'],
    ['_lock_file',       '#include <stdio.h>'],
]

# Linux specific functions
if host_system == 'linux'
    check_functions += [
        ['eventfd',              '#include <sys/eventfd.h>'],
        ['vmsplice',             '#include <fcntl.h>'],
        ['sched_getaffinity',    '#include <sched.h>'],
        ['recvmmsg',             '#include <sys/socket.h>'],
        ['memfd_create',         '#include <sys/mman.h>'],
    ]
endif

# Windows specific functions
if host_system == 'windows'
    check_functions += [
        ['_lock_file',      '#include <windows.h>'],
    ]
endif

foreach f : check_functions
    # DO NOT SIMPLIFY this if away by moving the the has_function
    # into the cdata.set! There are some functions checked twice
    # in different headers, if one is found with one header and
    # then not found using the other header, it would overwrite
    # the previous value!

    if cc.has_function(f[0], prefix: vlc_conf_prefix + f[1], dependencies: [socket_libs])
        cdata.set('HAVE_' + f[0].underscorify().to_upper(), 1)
    endif
endforeach

# Libcompat functions (if missing, provided in compat)
# Entry format: [function, prefix]
libcompat_functions = [
    ['aligned_alloc',    '#include <stdlib.h>'],
    ['atof',             '#include <stdlib.h>'],
    ['atoll',            '#include <stdlib.h>'],
    ['dirfd',            '#include <dirent.h>'],
    ['fdopendir',        '#include <dirent.h>'],
    ['flockfile',        '#include <stdio.h>'],
    ['fsync',            '#include <unistd.h>'],
    ['getdelim',         '#include <stdio.h>'],
    ['getpid',           getpid_h],
    ['lfind',            '#include <search.h>'],
    ['lldiv',            '#include <stdlib.h>'],
    ['memrchr',          '#include <string.h>'],
    ['nrand48',          '#include <stdlib.h>'],
    ['poll',             '#include <poll.h>'],
    ['posix_memalign',   '#include <stdlib.h>'],
    ['readv',            '#include <sys/uio.h>'],
    ['recvmsg',          '#include <sys/socket.h>'],
    ['rewind',           '#include <stdio.h>'],
    ['sendmsg',          '#include <sys/socket.h>'],
    ['setenv',           '#include <stdlib.h>'],
    ['strcasecmp',       '#include <strings.h>'],
    ['strcasestr',       '#include <string.h>'],
    ['strdup',           '#include <string.h>'],
    ['strlcpy',          '#include <string.h>'],
    ['strndup',          '#include <string.h>'],
    ['strnlen',          '#include <string.h>'],
    ['strnstr',          '#include <string.h>'],
    ['strsep',           '#include <string.h>'],
    ['strtof',           '#include <stdlib.h>'],
    ['strtok_r',         '#include <string.h>'],
    ['strtoll',          '#include <stdlib.h>'],
    ['swab',             swab_h],
    ['tdestroy',         '#include <search.h>'],
    ['tfind',            '#include <search.h>'],
    ['timegm',           '#include <time.h>'],
    ['timespec_get',     '#include <time.h>'],
    ['gmtime_r',         '#include <time.h>'],
    ['localtime_r',      '#include <time.h>'],
    ['strverscmp',       '#include <string.h>'],
    ['writev',           '#include <sys/uio.h>'],
    ['asprintf',         '#include <stdio.h>'],
    ['vasprintf',        '#include <stdio.h>'],

    ['gettimeofday',     '#include <sys/time.h>'],

    ['clock_gettime',    '#include <time.h>'],
    ['clock_nanosleep',  '#include <time.h>'],
    ['clock_getres',     '#include <time.h>'],

    ['inet_pton',        arpa_inet_h],
    ['inet_ntop',        arpa_inet_h],
]

# Linux specific functions
if host_system == 'linux'
    libcompat_functions += [
        ['getauxval',              '#include <sys/auxv.h>'],
    ]
endif

libcompat_sources = []

if have_win_store
    libcompat_sources += 'gai_strerror.c'
endif

# Check all functions in libcompat_functions array
foreach f : libcompat_functions
    if cc.has_function(f[0], prefix: vlc_conf_prefix + f[1], dependencies: [rt_lib, socket_libs])
        cdata.set('HAVE_' + f[0].underscorify().to_upper(), 1)
    else
        libcompat_sources += f[0] + '.c'
    endif
endforeach

# These functions need to be checked with has_header_symbol as
libcompat_functions = [
    ['realpath',         'stdlib.h'],
]

foreach f : libcompat_functions
    if cc.has_header_symbol(f[1], f[0], prefix: vlc_conf_prefix)
        cdata.set('HAVE_' + f[0].underscorify().to_upper(), 1)
    else
        libcompat_sources += f[0] + '.c'
    endif
endforeach

# Check for function 'nanf' (optionally link with libm if it exists)
if cc.has_function('nanf', prefix: vlc_conf_prefix + '#include <math.h>', dependencies: m_lib)
    cdata.set('HAVE_NANF', 1)
endif

# Check for function 'sincos' (optionally link with libm if it exists)
if cc.has_function('sincos', prefix: vlc_conf_prefix + '#include <math.h>', dependencies: m_lib)
    cdata.set('HAVE_SINCOS', 1)
else
    libcompat_sources += 'sincos.c'
endif

# Check for function 'fdatasync' (define it to 'fsync' if missing)
if not cc.has_function('fdatasync', prefix: vlc_conf_prefix + '#include <unistd.h>')
    cdata.set('fdatasync', 'fsync')
endif

#
# Additional checks
#

# Check which kind of restrict keyword is supported
# Program based on autoconf c.m4
#
# Copyright (C) 2001-2012 Free Software Foundation, Inc.
#
# Written by David MacKenzie, with help from
# Akim Demaille, Paul Eggert,
# Franc,ois Pinard, Karl Berry, Richard Pixley, Ian Lance Taylor,
# Roland McGrath, Noah Friedman, david d zuhn, and many others.
restrict_test = '''
    #define restrict_kw @0@
    typedef int * int_ptr;
    int foo (int_ptr restrict_kw ip) { return ip[0]; }

    int main() {
        int s[1];
        int * restrict_kw t = s;
        t[0] = 0;
        return foo(t);
    }
'''

# Order is the same as in AC_C_RESTRICT

# Check for __restrict support
if cc.compiles(restrict_test.format('__restrict'), name: 'Test __restrict support')
    cdata.set('restrict', '__restrict')

# Check for __restrict__ support
elif cc.compiles(restrict_test.format('__restrict__'), name: 'Test __restrict__ support')
    cdata.set('restrict', '__restrict__')

# Check for _Restrict support
elif cc.compiles(restrict_test.format('_Restrict'), name: 'Test _Restrict support')
    cdata.set('restrict', '_Restrict')

# Check for restrict support
elif not cc.compiles(restrict_test.format('restrict'), name: 'Test restrict support')
    cdata.set('restrict', '')
endif


# Check for C++ typeof support
if cpp.compiles('int a; typeof(a) foo[1];', name: 'Test C++ typeof support')
    cdata.set('HAVE_CXX_TYPEOF', 1)
endif

# Check for __attribute__((packed)) support
if cc.compiles('struct __attribute__((packed)) foo { int bar; };',
               name: '__attribute__((packed))')
    cdata.set('HAVE_ATTRIBUTE_PACKED', 1)
endif

# Check for C11 _Thread_local storage qualifier support
if cc.compiles('_Thread_local int foo = 0;', name: 'Test _Thread_local support')
    cdata.set('HAVE_THREAD_LOCAL', 1)
endif

# Check for wrong (non-POSIX) qsort_r prototype
qsort_r_test = '''
    #define _GNU_SOURCE
    #include <stdlib.h>
    _Static_assert(_Generic((qsort_r),
        void (*)(void *, size_t, size_t, void *,
                 int (*)(void *, const void *, const void *)) : 1, default: 0),
        "Bad prototype not matched");
'''
if cc.compiles(qsort_r_test, name: 'Test qsort_r non-POSIX prototype')
    cdata.set('HAVE_BROKEN_QSORT_R', 1)
endif

# Check for max_align_t type
if cc.has_type('max_align_t', prefix: '#include <stddef.h>')
    cdata.set('HAVE_MAX_ALIGN_T', 1)
endif

# Check for struct timespec
if cc.has_type('struct timespec', prefix: '#include <time.h>')
    cdata.set('HAVE_STRUCT_TIMESPEC', 1)
endif

# Add -fvisibility=hidden if compiler supports those
add_project_arguments(
    cc.get_supported_arguments('-fvisibility=hidden'),
    language: ['c'])

# Stack smashing protection (default enabled for optimized builds)
if (get_option('ssp')
    .disable_auto_if(get_option('optimization') == '0')
    .allowed())
    add_project_arguments(
        cc.get_supported_arguments('-fstack-protector-strong'),
        language: ['c', 'cpp'])

    if host_system == 'windows'
        # Win32 requires linking to ssp for stack-protection
ssp_test = '''
#include <stdio.h>

int main(void) {
    char buf[100];
    fgets(buf, sizeof(buf), stdin);
    return 0;
}
'''
        # Check if linker supports -lssp
        if cc.links(ssp_test, args: ['-fstack-protector-strong', '-lssp'],
                    name: 'linker supports stack protectors')
            add_project_link_arguments('-lssp', language: ['c', 'cpp'])
        endif
    endif
endif

# Check if linker supports -Bsymbolic
symbolic_linkargs = []
if cc.has_link_argument('-Wl,-Bsymbolic')
    symbolic_linkargs += '-Wl,-Bsymbolic'
endif

# Check for struct sockaddr_storage type
# Define it to `sockaddr` if missing
sockaddr_prefix = '#include <sys/types.h>\n'
if host_system == 'windows'
    sockaddr_prefix += '#include <winsock2.h>'
else
    sockaddr_prefix += '#include <sys/socket.h>'
endif

have_sockaddr_storage = cc.has_type('struct sockaddr_storage', prefix: sockaddr_prefix)
if not have_sockaddr_storage
    cdata.set('sockaddr_storage', 'sockaddr')
    cdata.set('ss_family', 'sa_family')
endif

# Check for ssize_t type
# Define it to `ptrdiff_t` if missing
if not cc.has_type('ssize_t', prefix: '#include <sys/types.h>')
    cdata.set('ssize_t', 'ptrdiff_t')
    cdata.set('SSIZE_MAX', 'PTRDIFF_MAX')
endif

# Check for struct pollfd type
# TODO: Refactor once updating to meson 1.0.0
# which supports prefix arrays.
pollfd_prefix = '#include <sys/types.h>\n'
if cdata.get('HAVE_POLL', 0) == 1
    pollfd_prefix += '#include <poll.h>'
elif host_system == 'windows'
    pollfd_prefix += '#include <winsock2.h>'
endif

if cc.has_type('struct pollfd', prefix: '\n'.join([vlc_conf_prefix, pollfd_prefix]))
    cdata.set('HAVE_STRUCT_POLLFD', 1)
endif

# Check for if_nameindex function and struct
if cc.has_function('if_nameindex', prefix: '#include <net/if.h>')
  cdata.set('HAVE_IF_NAMEINDEX', 1)
endif

if cc.has_type('struct if_nameindex', prefix: '#include <net/if.h>')
  cdata.set('HAVE_STRUCT_IF_NAMEINDEX', 1)
endif

# Check for locale_t type in C++ locale header
if cpp.has_type('locale_t', prefix: '#include <locale>')
    cdata.set('HAVE_CXX_LOCALE_T', 1)
endif

# Check if build machine is big endian
if build_machine.endian() == 'big'
    cdata.set('WORDS_BIGENDIAN', 1)
endif

# Define the shared library extension
if host_system == 'windows'
    cdata.set_quoted('LIBEXT', '.dll')
elif host_system == 'darwin'
    cdata.set_quoted('LIBEXT', '.dylib')
else
    cdata.set_quoted('LIBEXT', '.so')
endif

#
# Populate config.h with additional infos
#

cdata.set_quoted('VERSION',         vlc_version)
cdata.set_quoted('PACKAGE_VERSION', vlc_version)
cdata.set_quoted('VERSION_MESSAGE', f'@vlc_version@ @vlc_version_codename@')

cdata.set('VERSION_MAJOR',      vlc_version_major)
cdata.set('VERSION_MINOR',      vlc_version_minor)
cdata.set('VERSION_REVISION',   vlc_version_revision)
cdata.set('VERSION_EXTRA',      vlc_version_extra)

cdata.set('PACKAGE_VERSION_MAJOR',      vlc_version_major)
cdata.set('PACKAGE_VERSION_MINOR',      vlc_version_minor)
cdata.set('PACKAGE_VERSION_REVISION',   vlc_version_revision)
cdata.set('PACKAGE_VERSION_EXTRA',      vlc_version_extra)
cdata.set_quoted('PACKAGE_VERSION_DEV', vlc_version_type)

cdata.set('LIBVLC_ABI_MAJOR', libvlc_abi_version_major)
cdata.set('LIBVLC_ABI_MINOR', libvlc_abi_version_minor)
cdata.set('LIBVLC_ABI_MICRO', libvlc_abi_version_micro)

cdata.set_quoted('PACKAGE',             vlc_package_name)
cdata.set_quoted('PACKAGE_NAME',        vlc_package_name)
cdata.set_quoted('PACKAGE_STRING',      f'@vlc_package_name@ @vlc_version@')
cdata.set_quoted('COPYRIGHT_YEARS',     vlc_copyright_years)
cdata.set_quoted('COPYRIGHT_MESSAGE',   f'Copyright © @vlc_copyright_years@ the VideoLAN team')

# Compiler and build system info
cdata.set_quoted('VLC_COMPILER',        cc.get_id() + ' ' + cc.version())
cdata.set_quoted('VLC_COMPILE_BY',      '[not implemented with meson]') # TODO
cdata.set_quoted('VLC_COMPILE_HOST',    '[not implemented with meson]') # TODO
cdata.set_quoted('CONFIGURE_LINE',      '[not implemented with meson]') # TODO

# Paths
prefix_path = get_option('prefix')
vlc_pkg_suffix = meson.project_name().to_lower()

libdir_path = prefix_path / get_option('libdir')
pkglibdir_path = libdir_path / vlc_pkg_suffix
libexecdir_path = prefix_path / get_option('libexecdir')
pkglibexecdir_path = libexecdir_path / vlc_pkg_suffix
sysdatadir_path = prefix_path / get_option('datadir')
pkgdatadir_path = sysdatadir_path / vlc_pkg_suffix
localedir_path = prefix_path / get_option('localedir')

cdata.set_quoted('LIBDIR', libdir_path)
cdata.set_quoted('PKGLIBDIR', pkglibdir_path)
cdata.set_quoted('LIBEXECDIR', libexecdir_path)
cdata.set_quoted('PKGLIBEXECDIR', pkglibexecdir_path)
cdata.set_quoted('SYSDATADIR', sysdatadir_path)
cdata.set_quoted('PKGDATADIR', pkgdatadir_path)
cdata.set_quoted('LOCALEDIR', localedir_path)

# Enable stream outputs
if get_option('stream_outputs')
    cdata.set('ENABLE_SOUT', 1)
endif

# Enable VLM
if get_option('videolan_manager')
    if not get_option('stream_outputs')
        error('The VideoLAN manager requires stream outputs.')
    endif
    cdata.set('ENABLE_VLM', 1)
endif

# Allow running as root
# (useful for people running on embedded platforms)
if get_option('run_as_root')
    cdata.set('ALLOW_RUN_AS_ROOT', 1)
endif

# Optimize for memory usage vs speed
if get_option('optimize_memory')
    cdata.set('OPTIMIZE_MEMORY', 1)
endif

# Allow binary package maintainer to pass a custom string
# to avoid cache problem
if get_option('binary_version') != ''
    cdata.set_quoted('DISTRO_VERSION', get_option('binary_version'))
endif

if get_option('css_engine').allowed()
    cdata.set('HAVE_CSS', 1)
endif


# Font options
if get_option('default_font_path') != ''
    cdata.set_quoted('DEFAULT_FONT_FILE', get_option('default_font_path'))
endif

if get_option('default_monospace_font_path') != ''
    cdata.set_quoted('DEFAULT_MONOSPACE_FONT_FILE', get_option('default_monospace_font_path'))
endif

if get_option('default_font_family') != ''
    cdata.set_quoted('DEFAULT_FAMILY', get_option('default_font_family'))
endif

if get_option('default_monospace_font_family') != ''
    cdata.set_quoted('DEFAULT_MONOSPACE_FAMILY', get_option('default_monospace_font_family'))
endif

gcrypt_dep = dependency('libgcrypt',
                         version: '>= 1.6.0',
                         required: get_option('libgcrypt'))
if gcrypt_dep.found()
    cdata.set('HAVE_GCRYPT', 1)
endif

# Flex/Bison support
# This defines the `bison_gen` and `flex_gen` generator variables to be used
# to process Bison and Flex files respectively.

flex = find_program('flex', required: false)
bison = find_program('bison', required: false)

if flex.found() and bison.found()
    bison_gen = generator(bison,
        output: ['@BASENAME@.c', '@BASENAME@.h'],
        arguments: ['@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@'])

    flex_gen = generator(flex,
        output: '@PLAINNAME@.yy.c',
        arguments: ['-o', '@OUTPUT@', '@INPUT@'])
endif

# Some missing functions are implemented in compat
subdir('compat')

# Headers
subdir('include')

# libvlccore
subdir('src')

# LibVLC
subdir('lib')

# VLC plugins
subdir('modules')

# VLC binaries
subdir('bin')

# Generate config.h
configure_file(input: 'config.h.meson',
  output: 'config.h',
  configuration: cdata)

if (get_option('tests').allowed())
    # Integration and non-regression tests, some unittest are there too but the
    # modules/, src/ and lib/ folders should be favoured for those.
    subdir('test')
endif

if get_option('rust').allowed()
    warning('''
    The Rust module support is currently EXPERIMENTAL and INCOMPLETE.
    Testing and reporting or contributing missing plugins and features is welcome!
    ''')
endif

warning('''
    The Meson build system of VLC is currently EXPERIMENTAL and INCOMPLETE.
    Testing and reporting or contributing missing plugins and features is welcome!
''')
