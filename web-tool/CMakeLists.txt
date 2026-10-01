cmake_minimum_required(VERSION 3.22)
project(anim_retarget_web C)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "configure with emcmake (make web)")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

get_filename_component(REPO_ROOT ${CMAKE_SOURCE_DIR} DIRECTORY)
set(RAYLIB_DIR ${REPO_ROOT}/external/raylib)
if(NOT EXISTS ${RAYLIB_DIR}/lib/libraylib.web.a)
  message(FATAL_ERROR "raylib web build missing: run scripts/init-web.sh")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_APPS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(DISABLE_WERROR ON CACHE BOOL "" FORCE)
add_subdirectory(${REPO_ROOT}/third_party/json-c ${CMAKE_BINARY_DIR}/json-c EXCLUDE_FROM_ALL)
set_target_properties(json-c PROPERTIES
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
  $<TARGET_PROPERTY:json-c,INTERFACE_INCLUDE_DIRECTORIES>)

file(GLOB TOOL_SOURCES CONFIGURE_DEPENDS ${REPO_ROOT}/src/*.c)
list(REMOVE_ITEM TOOL_SOURCES ${REPO_ROOT}/src/main.c)
file(GLOB WEB_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.c)

add_executable(anim-retarget-web ${TOOL_SOURCES} ${WEB_SOURCES})
set_target_properties(anim-retarget-web PROPERTIES SUFFIX .js)
target_include_directories(anim-retarget-web PRIVATE ${REPO_ROOT}/inc ${CMAKE_SOURCE_DIR}/inc)
target_include_directories(anim-retarget-web SYSTEM PRIVATE
  ${REPO_ROOT}/third_party/cgltf
  ${REPO_ROOT}/third_party/cglm/include
  ${RAYLIB_DIR}/include)
target_compile_options(anim-retarget-web PRIVATE -Wall -Wextra -Werror -O2
  -include ${CMAKE_SOURCE_DIR}/inc/cgltf_rename.h)
target_link_libraries(anim-retarget-web PRIVATE json-c m ${RAYLIB_DIR}/lib/libraylib.web.a)
target_link_options(anim-retarget-web PRIVATE
  -O2
  -sUSE_GLFW=3
  -sMODULARIZE=1
  -sEXPORT_NAME=createAnimRetarget
  -sALLOW_MEMORY_GROWTH=1
  -sINITIAL_MEMORY=67108864
  -sFORCE_FILESYSTEM=1
  -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,FS,HEAPU8,UTF8ToString
  -sEXPORTED_FUNCTIONS=_main,_malloc,_free)

set(WEB_PAGE_FILES index.html app.js style.css)
set(WEB_MAP_FILES ual-to-mixamo.map mixamo-identity.map)
foreach(PAGE_FILE ${WEB_PAGE_FILES})
  add_custom_command(TARGET anim-retarget-web POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_SOURCE_DIR}/www/${PAGE_FILE} ${CMAKE_BINARY_DIR}/${PAGE_FILE})
endforeach()
foreach(MAP_FILE ${WEB_MAP_FILES})
  add_custom_command(TARGET anim-retarget-web POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${REPO_ROOT}/docs/mappings/${MAP_FILE} ${CMAKE_BINARY_DIR}/mappings/${MAP_FILE})
endforeach()
