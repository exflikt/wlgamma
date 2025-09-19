cmake_minimum_required(VERSION 3.10)
set(CMAK_EXPORT_COMPILE_COMMANDS ON)

project(wlgamma)

add_definitions("-std=c99 -Wall -D_DEFAULT_SOURCE")

file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/src/*.c")
add_executable(wlgamma ${SOURCES})
set_target_properties(wlgamma PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_custom_target(wlr-gamma-control-header COMMAND wayland-scanner client-header "${PROJECT_SOURCE_DIR}/wl/wlr-gamma-control-unstable-v1.xml" "${PROJECT_SOURCE_DIR}/src/wlr-gamma-control.h")
add_dependencies(wlgamma wlr-gamma-control-header)

add_custom_target(wlr-gamma-control-source COMMAND wayland-scanner private-code  "${PROJECT_SOURCE_DIR}/wl/wlr-gamma-control-unstable-v1.xml" "${PROJECT_SOURCE_DIR}/src/wlr-gamma-control.c")
add_dependencies(wlgamma wlr-gamma-control-source)

add_custom_target(manual COMMAND mkdir -p "${PROJECT_SOURCE_DIR}/sys/share/man/man1" && gzip -c "${PROJECT_SOURCE_DIR}/sys/wlgamma.1" > "${PROJECT_SOURCE_DIR}/sys/share/man/man1/wlgamma.1.gz")
add_dependencies(wlgamma manual)

target_link_libraries(wlgamma m)
target_link_libraries(wlgamma wayland-client)

install(TARGETS wlgamma DESTINATION bin)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/sys/share/" DESTINATION share)
