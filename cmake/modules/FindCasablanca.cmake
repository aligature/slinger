# FindCasablanca package
#
# Tries to find the Casablanca (C++ REST SDK) library
#

find_package(PkgConfig)

include(LibFindMacros)

# Include dir
find_path(CASABLANCA_INCLUDE_DIR
  NAMES
    cpprest/http_client.h
  PATHS 
    ${CASABLANCA_PKGCONF_INCLUDE_DIRS}
    ${CASABLANCA_DIR}
    $ENV{CASABLANCA_DIR}
    /usr/local/include
    /usr/include
    ../../casablanca
    /opt/local/include
    /opt/build
    /opt/build/cpprestsdk
  PATH_SUFFIXES 
    Release/include
    include
    cpprestsdk
)

# Library
find_library(CASABLANCA_LIBRARY
  NAMES 
    cpprest
  PATHS 
    ${CASABLANCA_PKGCONF_LIBRARY_DIRS}
    ${CASABLANCA_DIR}
    ${CASABLANCA_DIR}
    $ENV{CASABLANCA_DIR}
    /usr/local
    /usr
    ../../casablanca
    /opt/local/lib
    /opt/build
    /opt/build/cpprestsdk/lib/release
  PATH_SUFFIXES
    lib
    Release/build.release/Binaries/
    build.release/Binaries/
    cpprestsdk
    release
    cpprestsdk
)

set(CASABLANCA_PROCESS_LIBS CASABLANCA_LIBRARY)
set(CASABLANCA_PROCESS_INCLUDES CASABLANCA_INCLUDE_DIR)
libfind_process(CASABLANCA)