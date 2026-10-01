cmake_minimum_required(VERSION 3.22)
project(anim_retarget C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_APPS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(DISABLE_WERROR ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/json-c EXCLUDE_FROM_ALL)
set_target_properties(json-c PROPERTIES
  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
  $<TARGET_PROPERTY:json-c,INTERFACE_INCLUDE_DIRECTORIES>)

set(TOOL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/inc)
set(WEB_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/web-tool/inc)
set(WEB_LIB_SOURCES ${CMAKE_SOURCE_DIR}/web-tool/src/web_session.c)
set(THIRD_PARTY_INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/third_party/cgltf
  ${CMAKE_SOURCE_DIR}/third_party/cglm/include)

file(GLOB TOOL_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.c)
set(TOOL_LIB_SOURCES ${TOOL_SOURCES})
list(REMOVE_ITEM TOOL_LIB_SOURCES ${CMAKE_SOURCE_DIR}/src/main.c)

add_executable(anim-retarget ${TOOL_SOURCES})
target_include_directories(anim-retarget PRIVATE ${TOOL_INCLUDE_DIRS})
target_include_directories(anim-retarget SYSTEM PRIVATE ${THIRD_PARTY_INCLUDE_DIRS})
target_compile_options(anim-retarget PRIVATE -Wall -Wextra -Werror)
target_link_libraries(anim-retarget PRIVATE json-c m)

enable_testing()

set(CEST_DIR ${CMAKE_SOURCE_DIR}/external/cest)

set(SAN_COMPILE -fsanitize=address)
set(SAN_LINK -fsanitize=address)
if(NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
  list(APPEND SAN_LINK -static-libasan)
endif()

file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/test/*.test.cpp)

set(SCALAR_MATH_FLAGS -U__SSE__ -U__SSE2__ -U__SSE3__ -U__SSSE3__ -U__SSE4_1__ -U__SSE4_2__
  -U__AVX__ -U__AVX2__ -U__FMA__ -U__MMX__)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test-output)

add_custom_target(build_tests)

foreach(TEST_FILE ${TEST_FILES})
  get_filename_component(FILE_NAME ${TEST_FILE} NAME_WE)
  set(EXEC_NAME test_${FILE_NAME})
  add_executable(${EXEC_NAME} EXCLUDE_FROM_ALL ${TEST_FILE} ${TOOL_LIB_SOURCES} ${WEB_LIB_SOURCES})
  target_include_directories(${EXEC_NAME} PRIVATE ${TOOL_INCLUDE_DIRS} ${WEB_INCLUDE_DIRS} ${CEST_DIR})
  target_include_directories(${EXEC_NAME} SYSTEM PRIVATE ${THIRD_PARTY_INCLUDE_DIRS})
  target_compile_options(${EXEC_NAME} PRIVATE
    -g -O0 -Wall -Wunused-value -Werror
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++20>
    $<$<COMPILE_LANGUAGE:C>:-std=c99>
    ${SAN_COMPILE})
  if(FILE_NAME STREQUAL "scalar-math")
    target_compile_options(${EXEC_NAME} PRIVATE ${SCALAR_MATH_FLAGS})
  endif()
  target_link_options(${EXEC_NAME} PRIVATE ${SAN_LINK})
  target_link_libraries(${EXEC_NAME} PRIVATE json-c m)
  target_compile_definitions(${EXEC_NAME} PRIVATE
    FIXTURES_DIR="${CMAKE_SOURCE_DIR}/test/data"
    DOCS_DIR="${CMAKE_SOURCE_DIR}/docs"
    TEST_OUTPUT_DIR="${CMAKE_BINARY_DIR}/test-output")
  add_test(NAME ${EXEC_NAME} COMMAND ${EXEC_NAME})
  add_dependencies(build_tests ${EXEC_NAME})
endforeach()
