# 3DTrans

# Build reentrant qhull (libqhull_r) as a static library directly from its sources,
# without involving qhull's own CMake build system.
set(QHULL_R_DIR ${CMAKE_CURRENT_SOURCE_DIR}/qhull/src/libqhull_r)
set(QHULL_R_SOURCES
        ${QHULL_R_DIR}/global_r.c
        ${QHULL_R_DIR}/stat_r.c
        ${QHULL_R_DIR}/geom2_r.c
        ${QHULL_R_DIR}/poly2_r.c
        ${QHULL_R_DIR}/merge_r.c
        ${QHULL_R_DIR}/libqhull_r.c
        ${QHULL_R_DIR}/geom_r.c
        ${QHULL_R_DIR}/poly_r.c
        ${QHULL_R_DIR}/qset_r.c
        ${QHULL_R_DIR}/mem_r.c
        ${QHULL_R_DIR}/random_r.c
        ${QHULL_R_DIR}/usermem_r.c
        ${QHULL_R_DIR}/userprintf_r.c
        ${QHULL_R_DIR}/io_r.c
        ${QHULL_R_DIR}/user_r.c
        ${QHULL_R_DIR}/accessors_r.c
        ${QHULL_R_DIR}/rboxlib_r.c
        ${QHULL_R_DIR}/userprintf_rbox_r.c
)
add_library(qhullstatic_r STATIC ${QHULL_R_SOURCES})
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(qhullstatic_r PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif ()
# Expose qhull/src so consumers use #include "libqhull_r/libqhull_r.h" etc.
target_include_directories(qhullstatic_r PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/qhull/src)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/Behaviors BEHAVIORS_SRCS)
ckbb_add_module(3DTrans
        SOURCES 3DTrans.cpp PortalsManager.cpp PortalsManager.h 3DTrans.rc ${BEHAVIORS_SRCS}
        LINKS qhullstatic_r
        OUTPUT_NAME 3DTransfo
)
