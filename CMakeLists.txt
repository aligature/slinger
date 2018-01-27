cmake_minimum_required(VERSION 3.0)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_CXX_ALL "-std=c++1z")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_ALL} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_ALL}")

ADD_DEFINITIONS(-DBOOST_ALL_DYN_LINK)

project(slinger)
set(SOURCE main.cpp)
add_executable(${PROJECT_NAME} ${SOURCE})

set(CMAKE_VERBOSE_MAKEFILE ON)
find_package(Boost 1.40 COMPONENTS system program_options filesystem thread log REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

set(OPENSSL_ROOT_DIR /opt/local/)
find_package(OpenSSL REQUIRED)
message("OPENSSL_INCLUDE_DIR: " ${OPENSSL_INCLUDE_DIR})
message("OPENSSL_INCLUDE_DIR: " ${OPENSSL_LIB_DIR})

find_package(Casablanca REQUIRED)
include_directories(${CASABLANCA_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${CASABLANCA_LIBRARIES})
#target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} /opt/local/lib/libssl.dylib /opt/local/lib/libcrypto.dylib)