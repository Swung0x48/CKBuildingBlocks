# Sounds
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/Behaviors BEHAVIORS_SRCS)
set(SOUNDS_LINK_LIBRARIES)
if (WIN32)
    list(APPEND SOUNDS_LINK_LIBRARIES winmm)
endif ()

ckbb_add_module(Sounds SOURCES SoundBehavior.cpp ${BEHAVIORS_SRCS} LINKS ${SOUNDS_LINK_LIBRARIES})
