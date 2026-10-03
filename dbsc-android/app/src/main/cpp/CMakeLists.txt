cmake_minimum_required(VERSION 3.22.1)
project(dbsc C)

set(CMAKE_C_STANDARD 99)
set(DBSC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dbsc)

file(GLOB DBSC_C "${DBSC_DIR}/*.c")
list(FILTER DBSC_C EXCLUDE REGEX ".*/resolver\\.c$")
list(FILTER DBSC_C EXCLUDE REGEX ".*/imc\\.c$")

add_library(dbsc SHARED android_bridge.c ${DBSC_C})

set_source_files_properties("${DBSC_DIR}/comm.c" PROPERTIES COMPILE_DEFINITIONS "main=dbsc_original_main")

target_include_directories(dbsc PRIVATE "${DBSC_DIR}" "${DBSC_DIR}/chat")
target_compile_definitions(dbsc PRIVATE ANDROID=1 index=strchr rindex=strrchr)
target_compile_options(dbsc PRIVATE -fcommon -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-format -Wno-deprecated-non-prototype)
target_link_libraries(dbsc log z dl m)
