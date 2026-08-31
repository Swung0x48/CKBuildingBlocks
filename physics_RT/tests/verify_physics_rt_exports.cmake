if (NOT DEFINED DUMPBIN OR DUMPBIN STREQUAL "" OR NOT EXISTS "${DUMPBIN}")
    message(FATAL_ERROR "A valid MSVC dumpbin executable is required")
endif ()
if (NOT DEFINED DLL OR DLL STREQUAL "" OR NOT EXISTS "${DLL}")
    message(FATAL_ERROR "physics_RT DLL does not exist: ${DLL}")
endif ()

execute_process(
        COMMAND "${DUMPBIN}" /nologo /exports "${DLL}"
        RESULT_VARIABLE dump_result
        OUTPUT_VARIABLE dump_output
        ERROR_VARIABLE dump_error
)
if (NOT dump_result EQUAL 0)
    message(FATAL_ERROR "dumpbin /exports failed: ${dump_error}")
endif ()

string(REPLACE "\r\n" "\n" dump_output "${dump_output}")
string(REGEX MATCHALL
        "\n[ \t]+[0-9]+[ \t]+[0-9A-Fa-f]+[ \t]+[0-9A-Fa-f]+[ \t]+[^ \t\n]+"
        export_rows "${dump_output}")

set(actual_exports)
foreach (row IN LISTS export_rows)
    string(REGEX REPLACE
            ".*[ \t]([^ \t\n]+)$" "\\1" export_name "${row}")
    list(APPEND actual_exports "${export_name}")
endforeach ()
list(SORT actual_exports)

set(expected_exports
        CKGetPluginInfo
        CKGetPluginInfoCount
        PhysicsRT_GetApi
        RegisterBehaviorDeclarations
)
list(SORT expected_exports)

if (NOT actual_exports STREQUAL expected_exports)
    string(JOIN ", " actual_text ${actual_exports})
    string(JOIN ", " expected_text ${expected_exports})
    message(FATAL_ERROR
            "physics_RT exports must be exactly [${expected_text}], got [${actual_text}]")
endif ()
