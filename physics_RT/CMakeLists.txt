# physics_RT

add_subdirectory(ivp)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/Behaviors BEHAVIORS_SRCS)
ckbb_add_module(physics_RT
        SOURCES physics_RT.cpp
                CKIpionManager.cpp
                CKIpionManager.h
                PhysicsCallback.cpp
                PhysicsCallback.h
                PhysicsContact.cpp
                PhysicsContact.h
                ${BEHAVIORS_SRCS}
        LINKS ivp_controller ivp_compact_builder
)
