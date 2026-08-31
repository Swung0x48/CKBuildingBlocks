# physics_RT

set(PHYSICS_RT_SOLVER_COMPATIBILITY_ID "ivp-2.1-authority-66hz-v1"
        CACHE STRING "Cross-platform physics_RT solver compatibility identifier")
string(LENGTH "${PHYSICS_RT_SOLVER_COMPATIBILITY_ID}" _physics_rt_solver_id_length)
if (PHYSICS_RT_SOLVER_COMPATIBILITY_ID STREQUAL "" OR
        PHYSICS_RT_SOLVER_COMPATIBILITY_ID MATCHES "[^A-Za-z0-9._-]" OR
        _physics_rt_solver_id_length GREATER 64)
    message(FATAL_ERROR "PHYSICS_RT_SOLVER_COMPATIBILITY_ID must be 1-64 portable ASCII characters")
endif ()

set(PHYSICS_RT_SOURCE_BUILD_ID "unknown")
find_package(Git QUIET)
if (GIT_FOUND OR Git_FOUND)
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE _physics_rt_git_result
            OUTPUT_VARIABLE _physics_rt_git_sha
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )
    string(LENGTH "${_physics_rt_git_sha}" _physics_rt_git_sha_length)
    if (_physics_rt_git_result EQUAL 0 AND
            _physics_rt_git_sha_length EQUAL 40 AND
            _physics_rt_git_sha MATCHES "^[0-9a-fA-F]+$")
        set(PHYSICS_RT_SOURCE_BUILD_ID "${_physics_rt_git_sha}")
        execute_process(
                COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE _physics_rt_git_status
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
        )
        if (NOT _physics_rt_git_status STREQUAL "")
            string(APPEND PHYSICS_RT_SOURCE_BUILD_ID "-dirty")
        endif ()
    endif ()
endif ()

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
                AUTHORITY_API.md
                PhysicsRTApi.cpp
                PhysicsRTApi.h
                PhysicsRTApiInternal.h
                CKIpionManager.cpp
                CKIpionManager.h
                PhysicsCallback.cpp
                PhysicsCallback.h
                PhysicsContact.cpp
                PhysicsContact.h
                ${BEHAVIORS_SRCS}
        LINKS ${PHYSICS_RT_IVP_LIBRARIES}
)

foreach (_physics_rt_target IN ITEMS physics_RT physics_RTStatic)
    if (TARGET ${_physics_rt_target})
        target_compile_definitions(${_physics_rt_target} PRIVATE
                PHYSICSRT_BUILDING_LIBRARY=1
                "PHYSICSRT_SOURCE_BUILD_ID=\"${PHYSICS_RT_SOURCE_BUILD_ID}\""
                "PHYSICSRT_SOLVER_COMPATIBILITY_ID=\"${PHYSICS_RT_SOLVER_COMPATIBILITY_ID}\""
        )
        target_include_directories(${_physics_rt_target} PUBLIC
                "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/physics_RT>"
        )
    endif ()
endforeach ()

if (CKBB_INSTALL)
    install(FILES PhysicsRTApi.h
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/physics_RT"
            COMPONENT Development
    )
endif ()

if (CKBB_BUILD_TESTS AND BUILD_TESTING AND TARGET CK2_3DStatic)
    add_executable(physics_rt_regression_tests
            tests/test_physics_regressions.cpp
            tests/physics_rt_c_abi_compile.c
            PhysicsRTApi.cpp
            CKIpionManager.cpp
            PhysicsCallback.cpp
            PhysicsContact.cpp
    )
    target_compile_features(physics_rt_regression_tests PRIVATE cxx_std_17)
    target_compile_definitions(physics_rt_regression_tests PRIVATE
            CKBB_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
            PHYSICSRT_BUILDING_LIBRARY=1
            "PHYSICSRT_SOURCE_BUILD_ID=\"${PHYSICS_RT_SOURCE_BUILD_ID}\""
            "PHYSICSRT_SOLVER_COMPATIBILITY_ID=\"${PHYSICS_RT_SOLVER_COMPATIBILITY_ID}\""
    )
    target_include_directories(physics_rt_regression_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(physics_rt_regression_tests PRIVATE
            CK2_3DStatic
            ${PHYSICS_RT_IVP_LIBRARIES}
    )
    set_target_properties(physics_rt_regression_tests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME physics_rt_regression_tests
            COMMAND $<TARGET_FILE:physics_rt_regression_tests>)
    set_tests_properties(physics_rt_regression_tests PROPERTIES TIMEOUT 60)
endif ()
