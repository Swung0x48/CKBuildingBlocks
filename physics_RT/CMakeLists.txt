# physics_RT

add_subdirectory(ivp)

set(PHYSICS_RT_IVP_LIBRARIES ivp_controller ivp_compact_builder)
if (CMAKE_LINK_GROUP_USING_RESCAN_SUPPORTED)
    set(PHYSICS_RT_IVP_LIBRARIES
            "$<LINK_GROUP:RESCAN,ivp_controller,ivp_compact_builder,ivp_physics,ivp_surface_manager,ivp_utility,ivp_collision>"
    )
endif ()

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
        LINKS ${PHYSICS_RT_IVP_LIBRARIES}
)
