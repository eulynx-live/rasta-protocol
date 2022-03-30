# Install script for directory: /home/simon/Projects/rasta-protocol/src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librasta.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librasta.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librasta.so"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/simon/Projects/rasta-protocol/src/bin/lib/librasta.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librasta.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librasta.so")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librasta.so")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/config.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/dictionary.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/event_system.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/fifo.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/logging.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rasta_new.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rasta_red_multiplexer.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastacrc.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastadeferqueue.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastafactory.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastahandle.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastalist.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastamd4.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastamodule.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastaredundancy_new.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastautil.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rmemory.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/udp.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastablake2.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastasiphash24.h"
    "/home/simon/Projects/rasta-protocol/src/rasta/headers/rastahashing.h"
    "/home/simon/Projects/rasta-protocol/src/sci/headers/hashmap.h"
    "/home/simon/Projects/rasta-protocol/src/sci/headers/sci.h"
    "/home/simon/Projects/rasta-protocol/src/sci/headers/sci_telegram_factory.h"
    "/home/simon/Projects/rasta-protocol/src/sci/headers/scils.h"
    "/home/simon/Projects/rasta-protocol/src/sci/headers/scils_telegram_factory.h"
    "/home/simon/Projects/rasta-protocol/src/sci/headers/scip.h"
    "/home/simon/Projects/rasta-protocol/src/sci/headers/scip_telegram_factory.h"
    )
endif()

