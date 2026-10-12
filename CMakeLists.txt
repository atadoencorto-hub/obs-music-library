
# --- Detect if the plugin is built out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.16...3.26)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${PROJECT_NAME} MODULE)

# --- OBS libraries ---
if(BUILD_OUT_OF_TREE)
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE OBS::obs-frontend-api
  )
else()
  target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE OBS::frontend-api
  )
endif()

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE OBS::libobs
)

# --- Qt6 ---
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

if(BUILD_OUT_OF_TREE)
  if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
    find_package(Qt6 REQUIRED COMPONENTS Gui)
  endif()
endif()

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE Qt::Core Qt::Widgets
)

# --- Metadata: TagLib and utf8cpp ---
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# --- Download utf8cpp ---
FetchContent_Declare(
  utf8cpp
  GIT_REPOSITORY https://github.com/nemtrif/utfcpp.git
  GIT_TAG v4.0.6
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(utf8cpp)

# --- Configure utf8cpp headers ---
set(
  utf8cpp_INCLUDE_DIR
  "${utf8cpp_SOURCE_DIR}/source"
  CACHE PATH "utf8cpp include directory" FORCE
)

# --- Download and build TagLib ---
FetchContent_Declare(
  taglib
  URL https://github.com/taglib/taglib/archive/refs/tags/v2.1.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(taglib)

# --- Link TagLib ---
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE tag
)

# --- TagLib include directories ---
# Required for fileref.h, tfile.h and related headers
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE
    "${taglib_SOURCE_DIR}"
    "${taglib_SOURCE_DIR}/taglib"
    "${taglib_SOURCE_DIR}/taglib/toolkit"
    "${taglib_SOURCE_DIR}/taglib/mpeg"
    "${taglib_SOURCE_DIR}/taglib/mpeg/id3v1"
    "${taglib_SOURCE_DIR}/taglib/mpeg/id3v2"
    "${taglib_SOURCE_DIR}/taglib/mpeg/id3v2/frames"
    "${taglib_SOURCE_DIR}/taglib/ogg"
    "${taglib_SOURCE_DIR}/taglib/ogg/vorbis"
    "${taglib_SOURCE_DIR}/taglib/ogg/flac"
    "${taglib_SOURCE_DIR}/taglib/flac"
    "${taglib_SOURCE_DIR}/taglib/mp4"
    "${taglib_SOURCE_DIR}/taglib/asf"
    "${taglib_SOURCE_DIR}/taglib/riff"
    "${taglib_SOURCE_DIR}/taglib/riff/wav"
    "${taglib_SOURCE_DIR}/taglib/riff/aiff"
    "${taglib_SOURCE_DIR}/taglib/ape"
    "${taglib_BINARY_DIR}"
    "${taglib_BINARY_DIR}/taglib"
)

# --- Linux Qt compatibility ---
if(
  (OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
  AND Qt6_VERSION VERSION_LESS "6.9.0"
)
  find_package(Qt6 COMPONENTS GuiPrivate)

  target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE Qt::GuiPrivate
  )
endif()

# --- Compiler options ---
target_compile_options(
  ${PROJECT_NAME}
  PRIVATE
    $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
    -Wno-comma>
)

# --- MSVC runtime ---
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# --- Qt automatic processing ---
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

# --- Version information ---
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h
)

# --- Windows installer configuration ---
if(OS_WINDOWS)
  configure_file(
    cmake/windows/resources/installer-Windows.iss.in
    "${CMAKE_CURRENT_BINARY_DIR}/installer-Windows.generated.iss"
  )
endif()

# --- Plugin source files ---
target_sources(
  ${PROJECT_NAME}
  PRIVATE
    media-slider.cpp
    media-controls.cpp
    media-control.cpp
    slider-absoluteset-style.cpp
    media-controls.hpp
    media-control.hpp
    media-slider.hpp
    slider-absoluteset-style.hpp
    version.h
    MediaControls.ui
)

# --- Plugin output configuration ---
if(BUILD_OUT_OF_TREE)
  set_target_properties_plugin(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES OUTPUT_NAME ${_name}
  )
else()
  set_target_properties_obs(
    ${PROJECT_NAME}
    PROPERTIES FOLDER "plugins/exeldro" PREFIX ""
  )
endif()
