cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(wlgamma)

add_definitions("-std=c99 -Wall -D_DEFAULT_SOURCE")

file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/src/*.c")
set(WLR_GAMMA_CONTROL_PROTO "${PROJECT_SOURCE_DIR}/wl/wlr-gamma-control-unstable-v1.xml")
set(WLR_GAMMA_CONTROL_HEADER "${PROJECT_SOURCE_DIR}/src/wlr-gamma-control.h")
set(WLR_GAMMA_CONTROL_SOURCE "${PROJECT_SOURCE_DIR}/src/wlr-gamma-control.c")
add_custom_command(OUTPUT ${WLR_GAMMA_CONTROL_HEADER} COMMAND wayland-scanner client-header ${WLR_GAMMA_CONTROL_PROTO} ${WLR_GAMMA_CONTROL_HEADER} DEPENDS ${WLR_GAMMA_CONTROL_PROTO} VERBATIM)
add_custom_command(OUTPUT ${WLR_GAMMA_CONTROL_SOURCE} COMMAND wayland-scanner private-code  ${WLR_GAMMA_CONTROL_PROTO} ${WLR_GAMMA_CONTROL_SOURCE} DEPENDS ${WLR_GAMMA_CONTROL_PROTO} VERBATIM)
add_executable(wlgamma ${SOURCES} ${WLR_GAMMA_CONTROL_SOURCE})
set_target_properties(wlgamma PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_custom_target(wlr-gamma-control-header DEPENDS ${WLR_GAMMA_CONTROL_HEADER})
add_custom_target(wlr-gamma-control-source DEPENDS ${WLR_GAMMA_CONTROL_SOURCE})
add_dependencies(wlgamma wlr-gamma-control-header wlr-gamma-control-source)

add_custom_target(manual COMMAND mkdir -p "${PROJECT_SOURCE_DIR}/sys/share/man/man1" && gzip -c "${PROJECT_SOURCE_DIR}/sys/wlgamma.1" > "${PROJECT_SOURCE_DIR}/sys/share/man/man1/wlgamma.1.gz")
add_dependencies(wlgamma manual)

target_link_libraries(wlgamma m)
target_link_libraries(wlgamma wayland-client)

install(TARGETS wlgamma DESTINATION bin)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/sys/share/" DESTINATION share)
