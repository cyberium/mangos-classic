#
# This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# Minimum required cmake version to run the build
cmake_minimum_required(VERSION 3.2)

# Will cache object files if ccache is available
# Must be set at top of cmake outside of project header
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(CMANGOS_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake" CACHE PATH "Custom cmake directory")
set(HUNTER_CMAKE_DIR "${CMANGOS_CMAKE_DIR}/Hunter" CACHE PATH "Hunter cmake directory")
list(APPEND CMAKE_MODULE_PATH ${CMANGOS_CMAKE_DIR} ${HUNTER_CMAKE_DIR})

set(HUNTER_CONFIGURATION_TYPES Debug Release RelWithDebInfo MinSizeRel)
set(HUNTER_JOBS_NUMBER 4)
set(HUNTER_ROOT "../hunter")
set(HUNTER_DEBUG "ON")
#set(HUNTER_CACHE_SERVERS "https://github.com/cmangos/hunter-cache")
include(HunterGate)
HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.23.236.tar.gz"
    SHA1 "e998928e97e1a13cb35eebba2c62159eefc55d0d"
    LOCAL
)

project(CMaNGOS_Classic)

# Define here name of the binaries
set(CMANGOS_BINARY_SERVER_NAME "mangosd")
set(CMANGOS_BINARY_REALMD_NAME "realmd")

# Function to remove duplicate entries in provided string
function(RemoveDuplicateSubstring stringIn stringOut)
    separate_arguments(stringIn)
    list(REMOVE_DUPLICATES stringIn)
    string(REPLACE ";" " " stringIn "${stringIn}")
    set(${stringOut} "${stringIn}" PARENT_SCOPE)
endfunction()

# Macro that replace /Wx with /W0 (supposed to disable warnings)
MACRO(DisableWarnings)
  if(MSVC)
    set(DisableWarningsFlag "/W0")
    set(SearchWarningsFlag1 "/W[0-4]")
    set(SearchWarningsFlag2 "/Wall")
  else()
    set(DisableWarningsFlag "-w")
    set(SearchWarningsFlag1 "-[+W?]pedentic")
    set(SearchWarningsFlag2 "-Wall")
  endif()
  foreach(temp_flag
          CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS)
     if(${temp_flag} MATCHES "${SearchWarningsFlag1}")
        string(REGEX REPLACE "${SearchWarningsFlag1}" "${DisableWarningsFlag}" ${temp_flag} "${${temp_flag}}")
     elseif(${temp_flag} MATCHES "${SearchWarningsFlag2}")
        string(REGEX REPLACE "${SearchWarningsFlag2}" "${DisableWarningsFlag}" ${temp_flag} "${${temp_flag}}")
     else()
        set(${temp_flag} "${${temp_flag}} ${DisableWarningsFlag}")
     endif()
  endforeach()
ENDMACRO()

# Remove duplicate entries in default flags. (occur when after a second make or configure)
RemoveDuplicateSubstring("${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)

include(cmake/common.cmake)

# Set RPATH-handing (CMake parameters)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH ${LIBS_DIR})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

if(MSVC)
  # Turn on the ability to create folders to organize projects (.vcproj)
  # It creates "CMakePredefinedTargets" folder by default and adds CMake
  # defined projects like INSTALL.vcproj and ZERO_CHECK.vcproj
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

# set default buildoptions and print them
include(cmake/options.cmake)

# Force out-of-source build
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR
    "This project requires an out of source build. Remove the file 'CMakeCache.txt' found in this directory before continuing, create a separate build directory and run 'cmake [options] <srcs>' from there."
  )
endif()

# TODO: allow other compilers under windows in the future
if(WIN32 AND NOT MSVC AND NOT MINGW)
  message(FATAL_ERROR "Only MSVC++ and MinGW toolchains are supported on Windows at the moment.")
endif()

include(CheckPlatform)

# TODO: use MSVC_CXX_ARCHITECTURE_ID instead to identify platform on windows (not required on other systems)
# find platform: required to build 3rd party libraries w/o CMake files
# Find out what system we use to include the needed libs
if(WIN32)
  if(PLATFORM MATCHES X86) # 32-bit
    set(DEP_ARCH Win32)
  else() # 64-bit
    set(DEP_ARCH x64)
  endif()
endif()

# Override configuration-types - we don't use anything else than debug and release
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES "Release;Debug;RelWithDebInfo;MinSizeRel" CACHE STRING "Reset the configurations to what we need" FORCE)
else()
  if(NOT CMAKE_BUILD_TYPE)
    if (NOT DEBUG)
      message("Defaulting to release build.")
      set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    else()
      set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    endif()
  endif()
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY HELPSTRING "Choose the type of build")
  # set the valid options for cmake-gui drop-down list
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug;Release;RelWithDebInfo;MinSizeRel")
endif()

# find Git: used to get the revision number
find_package(Git)

if(NOT MSVC AND PCH)
  # Include cotire to manage PCH support
  include(cotire)
endif()

set(BIN_FOLDER_NAME bin)
set(CONF_FOLDER_NAME etc)
set(LIBS_FOLDER_NAME lib)

# Set Install folders
set(BIN_DIR ${CMAKE_INSTALL_PREFIX}/${BIN_FOLDER_NAME})
set(CONF_DIR ${CMAKE_INSTALL_PREFIX}/${CONF_FOLDER_NAME})
set(LIBS_DIR ${CMAKE_INSTALL_PREFIX}/${LIBS_FOLDER_NAME})

# On windows define more developpement folders and install provided libs
if(WIN32)
  set(BIN_DIR ${CMAKE_INSTALL_PREFIX})
  set(LIBS_DIR ${BIN_DIR})
  set(CONF_DIR ${BIN_DIR})
  if(MSVC)
    set(DEV_BIN_DIR ${CMAKE_SOURCE_DIR}/${BIN_FOLDER_NAME}/${DEP_ARCH}_$(Configuration))
    set(DEV_PROVIDED_LIBS_FOLDER ${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}_$(Configuration))
  else()
    set(DEV_BIN_DIR ${CMAKE_SOURCE_DIR}/${BIN_FOLDER_NAME}/${DEP_ARCH}_${CMAKE_BUILD_TYPE})
    set(DEV_PROVIDED_LIBS_FOLDER ${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}_${CMAKE_BUILD_TYPE})
  endif()

  # Define install rules for provided libs
  set(BIN_LIBS ${DEV_PROVIDED_LIBS_FOLDER}/libmySQL.dll)
  if(PLATFORM MATCHES X86)
    if(MSVC)
      # Only MSVC. TODO:: check why x64 is not provided
      set(BIN_LIBS ${BIN_LIBS} ${DEV_PROVIDED_LIBS_FOLDER}/dbghelp.dll)
    endif()
    set(BIN_LIBS ${BIN_LIBS} ${DEV_PROVIDED_LIBS_FOLDER}/libcrypto-1_1.dll)
  else()
    set(BIN_LIBS ${BIN_LIBS} ${DEV_PROVIDED_LIBS_FOLDER}/libcrypto-1_1-x64.dll)
  endif()
  install(FILES
      ${BIN_LIBS}
    DESTINATION ${LIBS_DIR}
  )
endif()

# For Unix systems set the rpath so that libraries are found
set(CMAKE_INSTALL_RPATH ../${LIBS_FOLDER_NAME})
set(CMAKE_INSTALL_NAME_DIR ${LIBS_DIR})

# Be sure to search only static lib
add_definitions(-DBOOST_ALL_NO_LIB)
set(Boost_USE_STATIC_LIBS ON)
#set(Boost_DEBUG ON)

# In our case use new way to treat BOOST_ROOT environement variable
if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

# Get dependencies lib from hunter's server and build them if need
hunter_add_package(Boost COMPONENTS system program_options thread regex)
hunter_add_package(ZLIB)
hunter_add_package(BZip2)
hunter_add_package(cryptopp)
hunter_add_package(MySQL-client)

# Find hunter packages
find_package(Boost CONFIG REQUIRED COMPONENTS system program_options thread regex)
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)
find_package(cryptopp CONFIG REQUIRED)
find_package(MySQL-client CONFIG REQUIRED)

# Find core revision
if(GIT_EXECUTABLE)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE REVISION_ID
    RESULT_VARIABLE GIT_RESULT
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(GIT_RESULT)
    set(REVISION_ID "Git repository not found")
    set(REVISION_DATE "\"0000-00-00T00:00:00+00:00\"")
  else()
    execute_process(
      COMMAND ${GIT_EXECUTABLE} show --quiet --date=iso-strict --format="%ad" ${REVISION_ID}
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE REVISION_DATE
      RESULT_VARIABLE GIT_RESULT
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(GIT_RESULT)
      set(REVISION_DATE "\"0000-00-00T00:00:00+00:00\"")
    endif()
  endif()
else()
  set(REVISION_ID "Git not found")
  set(REVISION_DATE "\"0000-00-00T00:00:00+00:00\"")
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/src/shared/revision.h.in
  ${CMAKE_SOURCE_DIR}/src/shared/revision.h
  @ONLY
)

set(DEFINITIONS_RELEASE NDEBUG)
set(DEFINITIONS_RELWITHDEBINFO NDEBUG _RELWITHDEBINFO MANGOS_RELWITHDEBINFO)
set(DEFINITIONS_MINSIZEREL NDEBUG _MINSIZEREL MANGOS_MINSIZEREL)
set(DEFINITIONS_DEBUG _DEBUG MANGOS_DEBUG)

if(NOT BUILD_GAME_SERVER AND BUILD_SCRIPTDEV)
  set(BUILD_SCRIPTDEV OFF)
  message(STATUS "BUILD_SCRIPTDEV forced to OFF due to BUILD_GAME_SERVER is not set")
endif()

if(NOT BUILD_GAME_SERVER AND BUILD_PLAYERBOT)
  set(BUILD_PLAYERBOT OFF)
  message(STATUS "BUILD_PLAYERBOT forced to OFF due to BUILD_GAME_SERVER is not set")
endif()

# print out the results before continuing
include(cmake/showoptions.cmake)

if(NOT BUILD_GAME_SERVER AND NOT BUILD_LOGIN_SERVER AND NOT BUILD_EXTRACTORS AND NOT BUILD_RECASTDEMOMOD)
  message(FATAL_ERROR "You must select something to build!")
endif()

if(XCODE)
  if(PLATFORM MATCHES X86)
    set(CMAKE_OSX_ARCHITECTURES i386)
  else()
    set(CMAKE_OSX_ARCHITECTURES x86_64)
  endif()
endif()

add_subdirectory(dep)

# Add definitions for all build types
# Don't place this above 'dep' subdirectory! Because of defines build will crash.
set(DEFINITIONS "")

if(UNIX)
  set(DEFINITIONS SYSCONFDIR="../${CONF_FOLDER_NAME}/")
endif()

if(POSTGRESQL)
  set(DEFINITIONS ${DEFINITIONS} DO_POSTGRESQL)
else()
  set(DEFINITIONS ${DEFINITIONS} DO_MYSQL)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${DEFINITIONS};${DEFINITIONS_DEBUG}")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${DEFINITIONS};${DEFINITIONS_RELWITHDEBINFO}")
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${DEFINITIONS};${DEFINITIONS_MINSIZEREL}")
else()
  set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${DEFINITIONS};${DEFINITIONS_RELEASE}")
endif()

if(BUILD_GAME_SERVER OR BUILD_LOGIN_SERVER OR BUILD_EXTRACTORS)
  add_subdirectory(src)
endif()

if(BUILD_EXTRACTORS)
  add_subdirectory(contrib/extractor)
  add_subdirectory(contrib/vmap_extractor)
  add_subdirectory(contrib/vmap_assembler)
  add_subdirectory(contrib/mmap)
endif()

if(BUILD_RECASTDEMOMOD)
  if (WIN32)
    if (NOT BUILD_EXTRACTORS)
      add_subdirectory(contrib/mmap)
    endif()
    add_subdirectory(contrib/recastdemomod)
  else()
    message(STATUS "BUILD_RECASTDEMOMOD forced to OFF. Not supported on non windows system.")
  endif()
endif()

if(BUILD_GIT_ID)
  add_subdirectory(contrib/git_id)
endif()

# set default startup project
if(MSVC)
  if(BUILD_GAME_SERVER)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CMANGOS_BINARY_SERVER_NAME})
  elseif(BUILD_LOGIN_SERVER)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CMANGOS_BINARY_REALMD_NAME})
  elseif(BUILD_RECASTDEMOMOD)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "RecastDemoMod")
  endif()
endif()

# if(SQL)
#   add_subdirectory(sql)
# endif()
