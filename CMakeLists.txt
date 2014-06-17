cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(pocl)
set(MAJOR_VERSION 0)
set(MINOR_VERSION 10)
set(VERSION_SUFFIX "-pre")
set(VERSION_STRING ${MAJOR_VERSION}.${MINOR_VERSION}${VERSION_SUFFIX})
set(POCL_VERSION ${VERSION_STRING})



######################################################################################

if(UNIX)
  include(GNUInstallDirs)

  # for libpocl.so
  set(POCL_INSTALL_PUBLIC_LIBDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" CACHE PATH "POCL public libdir")

  # for llvmopencl.so
  set(POCL_INSTALL_PRIVATE_LIBDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pocl" CACHE PATH "POCL private libdir")

  # for pocl.icd
  set(POCL_INSTALL_ICD_VENDORDIR "/etc/OpenCL/vendors" CACHE PATH "POCL ICD file destination")

  # for kernel-<target>.bc
  set(POCL_INSTALL_PRIVATE_DATADIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/pocl" CACHE PATH "POCL private datadir")

  # for poclu.h
  set(POCL_INSTALL_PUBLIC_HEADER_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}" CACHE PATH "POCL public header dir")

  # for _kernel.h et al
  set(POCL_INSTALL_PRIVATE_HEADER_DIR "${POCL_INSTALL_PRIVATE_DATADIR}/include" CACHE PATH "POCL private header dir")

  # for pocl-standalone et al
  set(POCL_INSTALL_PUBLIC_BINDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}" CACHE PATH "POCL public bindir")

  # for PoclConfig.cmake & stuff
  set(POCL_INSTALL_CMAKE_CONFIG_DIR "${POCL_INSTALL_PRIVATE_LIBDIR}" CACHE PATH   "Installation directory for CMake files")

  # TODO maybe use output of pkg-config --variable=pc_path pkg-config ?
  set(POCL_INSTALL_PKGCONFIG_DIR "${POCL_INSTALL_PUBLIC_LIBDIR}/pkgconfig" CACHE PATH "Destination for pocl.pc")

  if(APPLE)
    set(POCL_INSTALL_OPENCL_HEADER_DIR "${POCL_INSTALL_PUBLIC_HEADER_DIR}/OpenCL" CACHE PATH "POCL header dir for OpenCL headers")
  else()
    set(POCL_INSTALL_OPENCL_HEADER_DIR "${POCL_INSTALL_PUBLIC_HEADER_DIR}/CL" CACHE PATH "POCL header dir for OpenCL headers")
  endif()

else()
  # todo other platforms
  message(FATAL_ERROR "not implemented yet")
endif()


option(BUILD_SHARED_LIBS "ON=Build shared libs, OFF=static libs" ON)

# required b/c SHARED libs defaults to ON while OBJECT defaults to OFF
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

######################################################################################
# AC_CONFIG_TESTDIR([tests])
enable_testing()

######################################################################################

find_package(PkgConfig MODULE REQUIRED)

PKG_CHECK_MODULES(HWLOC REQUIRED hwloc>=1.0)
if(NOT HWLOC_FOUND)
  message(FATAL_ERROR "hwloc package is required!")
endif()

######################################################################################

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(LLVM RESULT_VARIABLE RES)
if(NOT RES)
  message(FATAL_ERROR "Could not load LLVM.cmake")
endif()

if("${LLVM_VERSION}" VERSION_LESS "3.2")
  message(FATAL_ERROR "POCL requires LLVM version >= 3.2 !")
endif()

######################################################################################

# if variable FEATURE_X isnt defined, sets it to DEFAULT_FEATURE_X;
# also, if DEFAULT_FEATURE_X is 0, prevents FEATURE_X being 1
# since it takes DEFAULT_FEATURE_X=0 to mean "FEATURE_X is unavailable"
macro(setup_cached_var VARNAME DESCRIPTION DOCS_FEATURE_IS_UNAVAILABLE DOCS_REQUESTED_DISABLING_FEATURE)

  if(DEFINED ${VARNAME})
    set(_CACHED "(cached)")
  else()
    set(_CACHED "")
    set(${VARNAME} ${DEFAULT_${VARNAME}})
  endif()

  if(${VARNAME} AND (NOT ${DEFAULT_${VARNAME}}))
    message(WARNING "${DOCS_FEATURE_IS_UNAVAILABLE}")
    set(${VARNAME} 0)
    set(_CACHED "(override)")
  endif()
  if((NOT ${VARNAME}) AND ${DEFAULT_${VARNAME}} )
    message(STATUS "${DOCS_REQUESTED_DISABLING_FEATURE}")
  endif()
  if(${VARNAME})
    message(STATUS "${DESCRIPTION} ${_CACHED}: 1")
  else()
    message(STATUS "${DESCRIPTION} ${_CACHED}: 0")
  endif()
endmacro()


######################################################################################

if(NOT DEFINED DEFAULT_ENABLE_DOCS)
  include(Sphinx RESULT_VARIABLE RES)
  if(SPHINX_EXECUTABLE)
    set(DEFAULT_ENABLE_DOCS 1 CACHE INTERNAL "Enable building of documentation")
  else()
    set(DEFAULT_ENABLE_DOCS 0 CACHE INTERNAL "Enable building of documentation")
  endif()
endif()


setup_cached_var(ENABLE_DOCS "Documentation building"
  "Requested enabling documentation, but could not find Sphinx ! -> disabling docs.."
  "Sphinx found, but requested disabling docs")


######################################################################################

set(NEW_PRINTF_WORKS 1)

if("${LLVM_VERSION}" VERSION_LESS "3.4")
  message(STATUS "Turning off new printf()")
  set(NEW_PRINTF_WORKS 0)
else()
  message(STATUS "Enabling new printf()")
endif()

######################################################################################

if(NOT DEFINED DEFAULT_USE_VECMATHLIB)
  # vecmathlib
  if(CLANGXX_WORKS AND EXISTS "${CMAKE_SOURCE_DIR}/lib/kernel/vecmathlib/vecmathlib.h")
    set(DEFAULT_USE_VECMATHLIB 1 CACHE INTERNAL "vecmathlib availability")
  else()
    set(DEFAULT_USE_VECMATHLIB 0 CACHE INTERNAL "vecmathlib availability")
  endif()
endif()

setup_cached_var(USE_VECMATHLIB "Vecmathlib use"
  "Requested enabling vecmathlib use, but either clang++ doesnt work or vecmathlib sources are missing.. -> disabling vecmathlib use"
  "Vecmathlib is usable, but requested disabling it")

######################################################################################

if(UNIX)
  if(APPLE)
    # MacOS ld outputs useless warnings like
    # ld: warning: -macosx_version_min not specificed, assuming 10.7
    # suppress them with -w.
    set(DEFAULT_HOST_LD_FLAGS "-Wl,-dylib -lm")
  else()
    set(DEFAULT_HOST_LD_FLAGS "-shared -lm")
  endif()
  # TODO win..
endif()
######################################################################################

option(USE_VECMATHLIB_BUILTINS_ONLY "Use only __builtin_* functions in the kernel library." OFF)

if(USE_VECMATHLIB)
  set(DEFAULT_KERNEL_CLANGXX_FLAGS "-DVML_NO_IOSTREAM")
endif()
set(DEFAULT_KERNEL_CL_FLAGS "-D__OPENCL_VERSION__=120")

if(USE_VECMATHLIB_BUILTINS_ONLY AND USE_VECMATHLIB)
  set(DEFAULT_KERNEL_CL_FLAGS "${DEFAULT_KERNEL_CL_FLAGS} -DPOCL_VECMATHLIB_BUILTIN")
  set(DEFAULT_KERNEL_CLANGXX_FLAGS "${DEFAULT_KERNEL_CLANGXX_FLAGS} -DPOCL_VECMATHLIB_BUILTIN")
endif()

if(NOT DEFINED KERNEL_CLANGXX_FLAGS)
  set(KERNEL_CLANGXX_FLAGS "${DEFAULT_KERNEL_CLANGXX_FLAGS}")
endif()
if(NOT DEFINED KERNEL_CL_FLAGS)
  set(KERNEL_CL_FLAGS "${DEFAULT_KERNEL_CL_FLAGS}")
endif()

message(STATUS "Clang++ flags for compiling kernel library: ${KERNEL_CLANGXX_FLAGS}")
message(STATUS "OpenCL flags for compiling kernel library: ${KERNEL_CL_FLAGS}")


######################################################################################

# DONE
option(STATIC_LLVM "Link LLVM statically. Default is dynamic." OFF)

if(STATIC_LLVM)
  message(STATUS "Linking LLVM statically")
  set(POCL_LLVM_LIBS ${LLVM_LIBFILES})
else()
  message(STATUS "Linking LLVM dynamically")
  set(POCL_LLVM_LIBS "-lLLVM-${LLVM_VERSION}")
  # in
  find_library(LLVM_SHARED_LIB_FILE "LLVM-${LLVM_VERSION}" PATHS "${LLVM_LIBDIR}" NO_DEFAULT_PATH)
  if(LLVM_SHARED_LIB_FILE AND EXISTS "${LLVM_SHARED_LIB_FILE}")
    message(STATUS "Using ${LLVM_SHARED_LIB_FILE}")
  else()
    set(POCL_LLVM_LIBS ${LLVM_LIBS})
    message(STATUS "${LLVM_SHARED_LIB_FILE} not found. LLVM is built with cmake.")
  endif()
endif()

######################################################################################

option(CUSTOM_BUFFER_ALLOCATOR "Use a custom OpenCL optimized region-based memory allocator instead of allocating buffers with malloc directly" ON)

######################################################################################

# examples are in their own cmakelists.txt

######################################################################################

find_library(LTDL_LIB "ltdl")
find_file(LTDL_H "ltdl.h")
if(LTDL_LIB AND LTDL_H)
  message(STATUS "ltdl found")

  get_filename_component(LTDL_H_INCLUDE_DIR "${LTDL_H}" DIRECTORY)
  string(FIND "${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES}" "${LTDL_H_INCLUDE_DIR}" LTPOSITION)
  # include the directory of ltdl.h, if its not in the default system include dirs
  if(LTPOSITION LESS "0")
    include_directories("${LTDL_H_INCLUDE_DIR}")
  endif()

else()
  message(FATAL_ERROR "Could not find LTDL library!")
endif()

######################################################################################

include(FindThreads)
if(Threads_FOUND)
  set(LD_FLAGS_BIN ${CMAKE_THREAD_LIBS_INIT})
else()
  message(FATAL_ERROR "Could not find threading library for this system")
endif()

######################################################################################
# DONE

if(NOT DEFINED DEFAULT_ENABLE_ICD)

  PKG_CHECK_MODULES(OCL_ICD ocl-icd>=1.3)
  if(NOT OCL_ICD_FOUND)
    # TODO maybe some other ways to find ocl-icd
    find_library(OCL_ICD_FOUND OpenCL)
    # TODO:
    # INCLUDE (CheckLibraryExists)
    # CHECK_LIBRARY_EXISTS(library function location variable
  endif()

  if(OCL_ICD_FOUND)
    set(DEFAULT_ENABLE_ICD 1 CACHE INTERNAL "ocl-icd availability")
  else()
    set(DEFAULT_ENABLE_ICD 1 CACHE INTERNAL "ocl-icd availability")
  endif()

endif()

setup_cached_var(ENABLE_ICD "Using ocl-icd"
  "Requested build with icd, but ocl-icd not found! some examples will not work.."
  "ocl-icd found, but requested build without icd")


######################################################################################
# DONE

# Option to allow to link a OpenCL program directly against libpocl
# instead of going through an ICD Loader
option(DIRECT_LINKAGE "Allow OpenCL programs to be linked directly against the pocl library instead of using a ICD Loader (OpenCL library)" OFF)

if(NOT DIRECT_LINKAGE)
  if(ENABLE_ICD)
    message(STATUS "Disabling direct linkage, using ocl-icd")
  else()
    message(FATAL_ERROR "at least one of ICD or direct-linkage must be enabled !")
  endif()
endif()


######################################################################################
# DONE

if(DEFINED TESTS_USE_ICD)

  if(TESTS_USE_ICD AND (NOT ENABLE_ICD))
    message(FATAL_ERROR "Cannot use ICD for tests when not building it")
  endif()
  if((NOT TESTS_USE_ICD) AND ENABLE_ICD)
    message(FATAL_ERROR "Cannot avoid ICD for tests when direct-linkage is disabled")
  endif()

else() # undefined -> auto

  set(TESTS_USE_ICD ${ENABLE_ICD})

endif()

message(STATUS "Run tests with ICD: ${TESTS_USE_ICD}")


######################################################################################
# DONE

# OpenCL headers

if(DEFINED INSTALL_OPENCL_HEADERS)
  message(STATUS "Install POCL's OpenCL headers: ${INSTALL_OPENCL_HEADERS}")
else() # Undefined = auto -> check
  find_file(OPENCL_H opencl.h PATH_SUFFIXES CL OpenCL)
  if(OPENCL_H)
    message(STATUS "OpenCL.h found, NOT installing our headers")
    set(IOH 0)
  else()
    message(STATUS "OpenCL.h not found, installing our headers")
    set(IOH 1)
  endif()
  set(INSTALL_OPENCL_HEADERS ${IOH} CACHE BOOL "Install POCL's OpenCL headers. (Ones from Kronos should be installed instead)")
endif()

######################################################################################
# DONE

# TODO Check cl.hpp usability. It is broken on a few platforms.

if(DEFINED HAVE_OPENCL_HPP)
  message(STATUS "OpenCL.hpp + OpenGL (cached): ${HAVE_OPENCL_HPP}")
else()
  find_file(OPENCL_HPP cl.hpp PATH_SUFFIXES CL OpenCL)
  set(_OPENCL_HPP 0)
  if(OPENCL_HPP)
    # test opengl availability first
    FIND_PACKAGE(OpenGL MODULE)
    if(OPENGL_FOUND)
      set(_OPENCL_HPP 1)
      message(STATUS "OpenCL.hpp found, OpenGL too, enabling OPENCL_HPP")
    else()
      message(WARNING "OpenCL.hpp found, OpenGL NOT found")
      message(WARNING "Please install an OpenGL implementation with gl.h or OpenGL.h. It is required by the C++ bindings")
      message(WARNING "Disabling OPENCL_HPP, a few tests will be skipped")
    endif()
  else()
    message(STATUS "cl.hpp not found, tests will use pocl's own cl.hpp")
  endif()
  # TODO should this be enabled even if openGL is not found ?
  set(HAVE_OPENCL_HPP ${_OPENCL_HPP} CACHE INTERNAL "Opencl.hpp found & usable")
endif()

######################################################################################
# TODO check if this works!

option(PEDANTIC "Compile host library with stricter compiler flags." OFF)
if(PEDANTIC)
  add_compile_options("-Wno-unused-result" "-Werror") # maybe "-Wimplicit"
endif()

######################################################################################
# DONE

PKG_CHECK_MODULES(GLEW glew)
if(NOT GLEW_FOUND)
  message(WARNING "libGLEW not found. A few tests will not work")
else()
  set(HAVE_GLEW 1)
endif()

######################################################################################
# DONE

if("${LLVM_CLANG_VERSION}" MATCHES "SPIR")
  set(CLANG_SPIR 1)
  message(STATUS "Using a SPIR generator Clang from Khronos.")
else()
  set(CLANG_SPIR 0)
  message(STATUS "NOT using SPIR")
endif()


####################################################################

# The Clang flags that are used always when compiling OpenCL C code.
# Note: We use -Wno-format to avoid warnings for printf, where we
# currently have to use %lld for long arguments (to handle 32-bit
# architectures).
set(FORCED_CLFLAGS "-Xclang -ffake-address-space-map -fno-math-errno -fblocks -fno-builtin -fasm -Wno-format")


####################################################################

# line 620

if(UNIX)
  if(APPLE)
    # TODO MACOSX_BUNDLE target prop
    set(ICD_LD_FLAGS "-single_module")
  else()
    set(ICD_LD_FLAGS "-Wl,-Bsymbolic")
  endif()
endif()

####################################################################

# TODO some more work required here

set(DEFAULT_HOST_LLC_FLAGS "-relocation-model=pic")
# TODO host clang flags / llc flags - properly add_compile_options etc
set(DEFAULT_HOST_CLANG_FLAGS "${DEFAULT_HOST_CLANG_FLAGS} ${CLANG_TARGET_OPTION}${LLC_TRIPLE}")
set(DEFAULT_HOST_LLC_FLAGS "${DEFAULT_HOST_LLC_FLAGS} -mtriple=${LLC_TRIPLE}")
if(NOT LLC_HOST_CPU MATCHES "unknown")
  set(DEFAULT_HOST_CLANG_FLAGS "${DEFAULT_HOST_CLANG_FLAGS} ${CLANG_MARCH_FLAG}${LLC_HOST_CPU}")
  set(DEFAULT_HOST_LLC_FLAGS "${DEFAULT_HOST_LLC_FLAGS} -mcpu=${LLC_HOST_CPU}")
endif()

if(CL_DISABLE_LONG)
  set(DEFAULT_HOST_CLANG_FLAGS "${DEFAULT_HOST_CLANG_FLAGS} -D_CL_DISABLE_LONG")
endif()

####################################################################

set(HOST_AS_FLAGS "${DEFAULT_HOST_AS_FLAGS}" CACHE STRING "Parameters to as for code generation in the host.")
set(HOST_LD_FLAGS "${DEFAULT_HOST_LD_FLAGS}" CACHE STRING "Parameter to compiler to generate loadable module.")
set(HOST_CLANG_FLAGS "${DEFAULT_HOST_CLANG_FLAGS}" CACHE STRING "Parameters to clang for host compilation.")
set(HOST_LLC_FLAGS "${DEFAULT_HOST_LLC_FLAGS}" CACHE STRING "Parameters to llc for code generation in the host.")

####################################################################

#line 760

set(OCL_TARGETS "host")


# THESE are only used in makefile.am & scripts/pocl*
set(TCE_TARGET_CLANG_FLAGS "" CACHE STRING "Extra parameters to Clang for TCE compilation.")
set(CELL_TARGET_CLANG_FLAGS "" CACHE STRING "Extra parameters to Clang for CELL compilation.")
# TODO HOST_CLANG_FLAGS -> CPU_TARGET_CLANG_FLAGS
#set(HOST_CLANG_FLAGS "" CACHE STRING "Extra parameters to Clang for cpu target compilation.")
#AC_ARG_VAR([TARGET_CLANG_FLAGS],
#           [Parameters to for target compilation.])

set(TCE_TARGET_LLC_FLAGS "" CACHE STRING "Extra parameters to LLVM's llc for TCE compilation.")
set(CELL_TARGET_LLC_FLAGS "" CACHE STRING "Extra parameters to LLVM's llc for CELL compilation.")
# TODO HOST_LLC_FLAGS -> CPU_TARGET_LLC_FLAGS
#set(HOST_LLC_FLAGS "" CACHE STRING "Extra parameters to LLVM's llc for cpu target compilation.")
#AC_DEFINE_UNQUOTED([TARGET_LLC_FLAGS],
#                   [Parameters to llc for code generation in the target.])

# TODO OCL_KERNEL_TARGET -> CPU_TARGET_TRIPLE
# TODO OCL_KERNEL_TARGET_CPU -> OCL_KERNEL_TARGET_CPU
# these are used here:
#lib/CL/devices/basic/basic.c:  dev->llvm_target_triplet = OCL_KERNEL_TARGET;
#lib/CL/devices/basic/basic.c:  dev->llvm_cpu = OCL_KERNEL_TARGET_CPU;
set(OCL_KERNEL_TARGET "${LLC_TRIPLE}") #The kernel target triplet.
set(OCL_KERNEL_TARGET_CPU "${LLC_HOST_CPU}") #The kernel target CPU variant.

# TODO this is required
# mostly used in tests to skip based on cpu
#AC_SUBST([HOST_CPU], [$host_cpu])
#AC_DEFINE_UNQUOTED([HOST_CPU], ["$host_cpu"], [The host CPU type.])


####################################################################

# Determine which device drivers to build.

set(OCL_DRIVERS "basic pthreads")

if(NOT DEFINED DEFAULT_ENABLE_TCE)

  find_program(TCE_CONFIG NAMES "tce-config")
  if(TCE_CONFIG)

    find_program(TCECC NAMES "tcecc")
    #... TODO check tce usability
    execute_process(COMMAND "${TCE_CONFIG}" --libs --cxxflags --includes OUTPUT_VARIABLE TCE_FLAGS RESULT_VARIABLE RESV)
    if(RESV)
      message(WARNING "running tce-config produced an error")
    else()
      set(TCE_FLAGS "${TCE_FLAGS}" CACHE INTERNAL "tce flags")
      execute_process(COMMAND "${TCE_CONFIG}" --prefix OUTPUT_VARIABLE TCE_PREFIX RESULT_VARIABLE RESV)
      set(TCE_PREFIX "${TCE_PREFIX}" CACHE INTERNAL "tce prefix")

      separate_arguments(FLAGS UNIX_COMMAND "${TCE_FLAGS}")
      custom_try_compile_c_cxx("${TCECC}" "cc" "#include <Application.hh>" "Application::initialize()" COMPILE_RESULT ${FLAGS})
      if(COMPILE_RESULT)
        message(WARNING "running tcecc with a sample program produced an error - disabling TCE")
      else()
        set(TCE_USABLE 1)
      endif()
    endif()

  endif()

  if(TCE_CONFIG AND TCECC AND TCE_USABLE)

    execute_process(COMMAND "${TCE_CONFIG}" --libs OUTPUT_VARIABLE TCE_LIBS RESULT_VARIABLE RESV)
    set(TCE_LIBS "${TCE_LIBS}" CACHE INTERNAL "tce-config --libs")
    # TODO
    set(LD_FLAGS_BIN ${LD_FLAGS_BIN} ${TCE_LIBS})

    execute_process(COMMAND "${TCE_CONFIG}" --includes OUTPUT_VARIABLE TCE_INCLUDES RESULT_VARIABLE RESV)
    set(TCE_INCLUDES "${TCE_INCLUDES}" CACHE INTERNAL "tce-config --includes")

    execute_process(COMMAND "${TCE_CONFIG}" --version OUTPUT_VARIABLE TCE_VERSION RESULT_VARIABLE RESV)
    set(TCE_VERSION "${TCE_VERSION}" CACHE INTERNAL "tce-config --version")

    set(DEFAULT_ENABLE_TCE 1 CACHE INTERNAL "TCE available")

    if(TCE_VERSION MATCHES "trunk")
      set(DEFAULT_ENABLE_TCEMC 1 CACHE INTERNAL "TCEMC available")
    endif()

  else()
    set(DEFAULT_ENABLE_TCE 0 CACHE INTERNAL "TCE available")
    set(DEFAULT_ENABLE_TCEMC 0 CACHE INTERNAL "TCEMC available")
  endif()

endif()

setup_cached_var(ENABLE_TCE "TCE"
  "Requested enabling TCE, but no usable TCE installation found !"
  "TCE is available, but requested disabling it")

if(ENABLE_TCE)
  set(OCL_DRIVERS "${OCL_DRIVERS} tce")
  set(OCL_TARGETS "${OCL_TARGETS} tce")
  if(DEFAULT_ENABLE_TCEMC)
    set(ENABLE_TCEMC 1)
    set(OCL_DRIVERS "${OCL_DRIVERS} tcemc") # TCEMC is a "superset" of TCE (lp:tce) features.
  endif()
else()
  set(ENABLE_TCEMC 0)
endif()

##########################################################

# Check if CellSPU support is found

# The libspe version requirement is not strict. This is the only one tested.
# SPU backend was removed in LLVM 3.3 (and we don't accept LLVM 3.1 anymore in pocl)
# so compile the spu backend only when LLVM 3.2 is found

if(NOT DEFINED DEFAULT_ENABLE_SPU)
  set(_SPU 0)
  if(LLVM_VERSION MATCHES "3.2")
    PKG_CHECK_MODULES(LIBSPE "libspe2>=2.2.80")
    if(LIBSPE_FOUND)
      set(_SPU 1)
    endif()
  else()
    message(STATUS "Skipping cellspu target, needs llvm 3.2")
  endif()
  set(DEFAULT_ENABLE_SPU ${_SPU} CACHE INTERNAL "Build cell SPU")
endif()

setup_cached_var(ENABLE_SPU "Cell SPU"
  "Requested enabling Cell SPU, but no usable libspe found !"
  "Cell SPU available, but requested disabling it")

if(ENABLE_SPU)
  set(OCL_DRIVERS "${OCL_DRIVERS} spu")
  set(OCL_TARGETS "${OCL_TARGETS} cellspu")
endif()

##########################################################

message(STATUS "Building the following device drivers: ${OCL_DRIVERS}")

set(BUILDDIR "${CMAKE_BINARY_DIR}")
set(SRCDIR "${CMAKE_SOURCE_DIR}")

##########################################################

# Checks for library features.

include(CheckSymbolExists)
CHECK_SYMBOL_EXISTS("posix_memalign" "stdlib.h" HAVE_POSIX_MEMALIGN)

#AC_C_BIGENDIAN
include(TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

##########################################################

# TODO

#AC_MSG_NOTICE([Checking host compiler characteristics])
#message(STATUS "Checking host compiler characteristics")

#CFLAGS="-h $host"
# -> TRIPLE = LLC_TRIPLE

# HOST_SIZEOF_* is actually unused; some code still uses SIZEOF_* though.

#AC_CHECK_SIZEOF([long])
#AC_SUBST([HOST_SIZEOF_LONG], )
CHECK_SIZEOF("long" SIZEOF_LONG ${LLC_TRIPLE})
CHECK_SIZEOF("__fp16" SIZEOF___FP16 ${LLC_TRIPLE})
CHECK_SIZEOF("double" SIZEOF_DOUBLE ${LLC_TRIPLE})
CHECK_SIZEOF("void*" SIZEOF_VOID_P ${LLC_TRIPLE})


CHECK_ALIGNOF("float16" "typedef float float16  __attribute__((__ext_vector_type__(16)));" ALIGNOF_FLOAT16 ${LLC_TRIPLE})

CHECK_ALIGNOF("double16" "typedef double double16  __attribute__((__ext_vector_type__(16)));" ALIGNOF_DOUBLE16 ${LLC_TRIPLE})

if(ALIGNOF_FLOAT16 GREATER ALIGNOF_DOUBLE16)
  set(MAX_EXTENDED_ALIGNMENT "${ALIGNOF_FLOAT16}")
else()
  set(MAX_EXTENDED_ALIGNMENT "${ALIGNOF_DOUBLE16}")
endif()

##########################################################

# POCL_DEVICE_ADDRESS_BITS
# Value based on host processor, for basic and pthreads devices
if("${SIZEOF_VOID_P}" STREQUAL "8")
  set(POCL_DEVICE_ADDRESS_BITS 64)
elseif("${SIZEOF_VOID_P}" STREQUAL "4")
  set(POCL_DEVICE_ADDRESS_BITS 32)
else()
  message(FATAL_ERROR "Cannot figure out POCL_DEVICE_ADDRESS_BITS")
endif()


##########################################################


# cmake docs:
# SOVERSION: What version number is this target.

# For shared libraries VERSION and SOVERSION can be used to specify the
#  build version and API version respectively. When building or installing
#  appropriate symlinks are created if the platform supports symlinks and
#  the linker  supports so-names. If only one of both is specified the
#  missing is assumed to have the same version number.
#
# For executables VERSION can be used to specify the build version.
# SOVERSION is ignored if NO_SONAME property is set. For shared libraries
# and executables on Windows the VERSION attribute is parsed to extract
#  a "major.minor" version number. These numbers are used as the
#  image version of the binary.

# cmake usage:
# SET_TARGET_PROPERTIES(pocl PROPERTIES SOVERSION 1.6.3 VERSION 4) ...



# The libtool library version string to use (passed to -version-info).
# See: http://www.nondot.org/sabre/Mirrored/libtool-2.1a/libtool_6.html
# libpocl.so should get only API additions as we are implementing a standard.
#
# The library version encodings into the library file name are platform
# dependant. Therefore we need to be a bit verbose here for the pocl.icd file
# creation to succeed (see Makefile.am).
# Chiefly, GNU differs from BSD, and others are untested. See e.g.
# http://en.opensuse.org/openSUSE%3aShared_library_packaging_policy#Versioning_schemes
#
# 0:0:0 == 0.6
# 1:0:0 == 0.7 (not backwards compatible with 0:0:0 due to the ICD)
# 2:0:1 == 0.8 (currently backwards compatible with 0.7, thus age = 1).
# 3:0:2 == 0.9 (currently backwards compatible with 0.7, thus age = 2).
# 4:0:3 == 0.10 (currently backwards compatible with 0.7, thus age = 3).


set(LIB_CURRENT_VERSION 4)
set(LIB_REVISION_VERSION 0)
set(LIB_AGE_VERSION 3)

# LIB_FIRST_VERSION=$(($LIB_CURRENT_VERSION - $LIB_AGE_VERSION))
math(EXPR LIB_FIRST_VERSION "${LIB_CURRENT_VERSION} - ${LIB_AGE_VERSION}")

# libtool takes "c:r:a" arguments, but the result is "<lib>.so.(c-a).a.r"
# cmake has "build version" and "API version"
# these vars map libtool -> cmake
# for set_target_properties
set(LIB_BUILD_VERSION "${LIB_FIRST_VERSION}.${LIB_AGE_VERSION}.${LIB_REVISION_VERSION}")
set(LIB_API_VERSION "${LIB_FIRST_VERSION}")

# The kernel compiler opt plugin shared library, however, changes more
# drastically. Let's try to follow the similar 'current' numbering as
# the pocl host API library and perhaps tune the 'revision' and 'age' later.

# DONE - lib/llvmopencl..
# AC_SUBST([KERNEL_COMPILER_LIB_VERSION], ["3:0:0"])
set(KERNEL_COMPILER_LIB_VERSION "3.0.0")

##########################################################


#TODO
# these vars are copies b/c tons of sources use BUILD_ICD etc
set(BUILD_ICD ${ENABLE_ICD})
set(BUILD_SPU ${ENABLE_SPU})
set(TCE_AVAILABLE ${ENABLE_TCE})
set(TCEMC_AVAILABLE ${ENABLE_TCEMC})
set(HAVE_OCL_ICD ${ENABLE_ICD})

configure_file("config.h.in.cmake" "config.h" ESCAPE_QUOTES)
include_directories("${CMAKE_BINARY_DIR}")


##########################################################

# determine library suffix (POCL_LIB_SUFFIX) to use for pocl.icd

if(ENABLE_ICD)

  if(UNIX)

    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
      set(POCL_LIB_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}.${LIB_BUILD_VERSION}")
    elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
      set(POCL_LIB_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}.${LIB_API_VERSION}")
    else()
      # TODO apple (Darwin)
      message(WARNING "Unknown UNIX variant (${CMAKE_SYSTEM_NAME})!")
      # fallback..
      set(POCL_LIB_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif()

  else()
    # TODO: other platforms
    # ...
  endif()

endif()

configure_file("${CMAKE_SOURCE_DIR}/pocl.icd.in.cmake" "${CMAKE_BINARY_DIR}/pocl.icd" @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/pocl.icd"
         DESTINATION "${POCL_INSTALL_ICD_VENDORDIR}")


##########################################################

if(UNIX)

  configure_file("${CMAKE_SOURCE_DIR}/pocl.pc.in.cmake" "${CMAKE_BINARY_DIR}/pocl.pc" @ONLY)
  install(FILES "${CMAKE_BINARY_DIR}/pocl.pc"
         DESTINATION "${POCL_INSTALL_PKGCONFIG_DIR}")

endif()

#############################################################

include_directories("fix-includes" "include")

add_subdirectory("include")

add_subdirectory("lib")

# these are set in lib/cmakelists.txt
message(STATUS "OPENCL_LIBS: ${OPENCL_LIBS}")
message(STATUS "OPENCL_CFLAGS: ${OPENCL_CFLAGS}")

get_target_property(LIBPOCL_LOCATION "pocl" LOCATION)
message(STATUS "LOCATION of libpocl: ${LIBPOCL_LOCATION}")
# requires libpocl location
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ocl-vendors")
file(WRITE "${CMAKE_BINARY_DIR}/ocl-vendors/pocl-tests.icd" "${LIBPOCL_LOCATION}")

# for tests / examples
set(POCLU_LINK_OPTIONS "-lm" ${OPENCL_LIBS} "poclu")
message(STATUS "POCLU LINK OPTS: ${POCLU_LINK_OPTIONS}")

# DONE - just pocl-standalone script
add_subdirectory("scripts")

# TODO In progress
add_subdirectory("tests")

# TODO In progress
set(ALL_TESTSUITES "AMD;AMDSDK2.9;opencl-book-samples;Parboil;piglit;Rodinia;VexCL;ViennaCL")
add_subdirectory("examples")

##########################################################

MESSAGE(STATUS " ")
MESSAGE(STATUS "*********************** SUMMARY ***************************")
MESSAGE(STATUS " ")
MESSAGE(STATUS "******* Directories:")
MESSAGE(STATUS " ")

MESSAGE(STATUS "POCL_INSTALL_CMAKE_CONFIG_DIR: ${POCL_INSTALL_CMAKE_CONFIG_DIR}")
MESSAGE(STATUS "POCL_INSTALL_ICD_VENDORDIR: ${POCL_INSTALL_ICD_VENDORDIR}")
MESSAGE(STATUS "POCL_INSTALL_OPENCL_HEADER_DIR: ${POCL_INSTALL_OPENCL_HEADER_DIR}")
MESSAGE(STATUS "POCL_INSTALL_PKGCONFIG_DIR: ${POCL_INSTALL_PKGCONFIG_DIR}")
MESSAGE(STATUS "POCL_INSTALL_PRIVATE_DATADIR: ${POCL_INSTALL_PRIVATE_DATADIR}")
MESSAGE(STATUS "POCL_INSTALL_PRIVATE_HEADER_DIR: ${POCL_INSTALL_PRIVATE_HEADER_DIR}")
MESSAGE(STATUS "POCL_INSTALL_PRIVATE_LIBDIR: ${POCL_INSTALL_PRIVATE_LIBDIR}")
MESSAGE(STATUS "POCL_INSTALL_PUBLIC_BINDIR: ${POCL_INSTALL_PUBLIC_BINDIR}")
MESSAGE(STATUS "POCL_INSTALL_PUBLIC_HEADER_DIR: ${POCL_INSTALL_PUBLIC_HEADER_DIR}")
MESSAGE(STATUS "POCL_INSTALL_PUBLIC_LIBDIR: ${POCL_INSTALL_PUBLIC_LIBDIR}")

MESSAGE(STATUS " ")
MESSAGE(STATUS "******* Enabled features:")
MESSAGE(STATUS " ")

MESSAGE(STATUS "CLANG_SPIR: ${CLANG_SPIR}")
MESSAGE(STATUS "CL_DISABLE_LONG (Disable cl_khr_int64): ${CL_DISABLE_LONG}")
MESSAGE(STATUS "ENABLE_DOCS: ${ENABLE_DOCS}")
MESSAGE(STATUS "ENABLE_ICD: ${ENABLE_ICD}")
MESSAGE(STATUS "ENABLE_SPU (Cell SPU enabled): ${ENABLE_SPU}")
MESSAGE(STATUS "ENABLE_TCE: ${ENABLE_TCE}")
MESSAGE(STATUS "ENABLE_TCEMC: ${ENABLE_TCEMC}")
MESSAGE(STATUS "HAVE_GLEW: ${HAVE_GLEW}")
MESSAGE(STATUS "HAVE_OPENCL_HPP: ${HAVE_OPENCL_HPP}")
MESSAGE(STATUS "INSTALL_OPENCL_HEADERS (Install our headers): ${INSTALL_OPENCL_HEADERS}")
MESSAGE(STATUS "NEW_PRINTF_WORKS: ${NEW_PRINTF_WORKS}")
MESSAGE(STATUS "OCL_DRIVERS (Drivers built): ${OCL_DRIVERS}")
MESSAGE(STATUS "OCL_TARGETS (Targets built): ${OCL_TARGETS}")
MESSAGE(STATUS "TESTS_USE_ICD: ${TESTS_USE_ICD}")
MESSAGE(STATUS "USE_VECMATHLIB: ${USE_VECMATHLIB}")
MESSAGE(STATUS "Available testsuites: ${ALL_TESTSUITES}")
MESSAGE(STATUS "Enabled testsuites: ${ENABLED_TESTSUITES}")

MESSAGE(STATUS " ")
MESSAGE(STATUS "******* Programs:")
MESSAGE(STATUS " ")

MESSAGE(STATUS "LLVM_CONFIG: ${LLVM_CONFIG}")
MESSAGE(STATUS "LLVM_OPT: ${LLVM_OPT}")
MESSAGE(STATUS "LLC: ${LLC}")
MESSAGE(STATUS "LLVM_AS: ${LLVM_AS}")
MESSAGE(STATUS "LLVM_LINK: ${LLVM_LINK}")
MESSAGE(STATUS "LLVM_LLI: ${LLVM_LLI}")
MESSAGE(STATUS "WITH_LLVM_CONFIG (User preferred llvm-config): ${WITH_LLVM_CONFIG}")


MESSAGE(STATUS " ")
MESSAGE(STATUS "******* Various Flags:")
MESSAGE(STATUS " ")

MESSAGE(STATUS "CELL_TARGET_CLANG_FLAGS: ${CELL_TARGET_CLANG_FLAGS}")
MESSAGE(STATUS "CELL_TARGET_LLC_FLAGS: ${CELL_TARGET_LLC_FLAGS}")
MESSAGE(STATUS "CLANG_MARCH_FLAG: ${CLANG_MARCH_FLAG}")
MESSAGE(STATUS "CLANG_TARGET_OPTION: ${CLANG_TARGET_OPTION}")
MESSAGE(STATUS "HOST_AS_FLAGS: ${HOST_AS_FLAGS}")
MESSAGE(STATUS "HOST_CLANG_FLAGS: ${HOST_CLANG_FLAGS}")
MESSAGE(STATUS "HOST_LD_FLAGS: ${HOST_LD_FLAGS}")
MESSAGE(STATUS "HOST_LLC_FLAGS: ${HOST_LLC_FLAGS}")
MESSAGE(STATUS "ICD_LD_FLAGS: ${ICD_LD_FLAGS}")
MESSAGE(STATUS "KERNEL_CLANGXX_FLAGS: ${KERNEL_CLANGXX_FLAGS}")
MESSAGE(STATUS "KERNEL_CL_FLAGS: ${KERNEL_CL_FLAGS}")
MESSAGE(STATUS "LD_FLAGS_BIN: ${LD_FLAGS_BIN}")
MESSAGE(STATUS "LIB_API_VERSION: ${LIB_API_VERSION}")
MESSAGE(STATUS "LIB_BUILD_VERSION: ${LIB_BUILD_VERSION}")
MESSAGE(STATUS "LLVM_VERSION: ${LLVM_VERSION}")
MESSAGE(STATUS "LLVM_CFLAGS: ${LLVM_CFLAGS}")
MESSAGE(STATUS "LLVM_CXXFLAGS: ${LLVM_CXXFLAGS}")
MESSAGE(STATUS "LLVM_CPPFLAGS: ${LLVM_CPPFLAGS}")
MESSAGE(STATUS "LLVM_LDFLAGS: ${LLVM_LDFLAGS}")
MESSAGE(STATUS "LLVM_LIBDIR: ${LLVM_LIBDIR}")
MESSAGE(STATUS "LLVM_INCLUDEDIR: ${LLVM_INCLUDEDIR}")
# these are a bit too long..
#MESSAGE(STATUS "LLVM_LIBS: ${LLVM_LIBS}")
#MESSAGE(STATUS "LLVM_LIBFILES: ${LLVM_LIBFILES}")
MESSAGE(STATUS "LLVM_ALL_TARGETS: ${LLVM_ALL_TARGETS}")
MESSAGE(STATUS "LLVM_HOST_TARGET: ${LLVM_HOST_TARGET}")
MESSAGE(STATUS "LLC_TRIPLE: ${LLC_TRIPLE}")
MESSAGE(STATUS "LLC_HOST_CPU: ${LLC_HOST_CPU}")
MESSAGE(STATUS "MAX_EXTENDED_ALIGNMENT: ${MAX_EXTENDED_ALIGNMENT}")
MESSAGE(STATUS "OCL_KERNEL_TARGET: ${OCL_KERNEL_TARGET}")
MESSAGE(STATUS "OCL_KERNEL_TARGET_CPU: ${OCL_KERNEL_TARGET_CPU}")
MESSAGE(STATUS "POCL_DEVICE_ADDRESS_BITS: ${POCL_DEVICE_ADDRESS_BITS}")
MESSAGE(STATUS "POCL_LLVM_LIBS: ${POCL_LLVM_LIBS}")
MESSAGE(STATUS "TCE_TARGET_CLANG_FLAGS: ${TCE_TARGET_CLANG_FLAGS}")
MESSAGE(STATUS "TCE_TARGET_LLC_FLAGS: ${TCE_TARGET_LLC_FLAGS}")


MESSAGE(STATUS " ")