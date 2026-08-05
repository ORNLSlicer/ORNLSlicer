foreach(required_var ORNLSLICER_EXECUTABLE PROJECT_FILE OUTPUT_PREFIX)
    if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
        message(FATAL_ERROR "${required_var} is required.")
    endif()
endforeach()

if(NOT EXISTS "${ORNLSLICER_EXECUTABLE}")
    message(FATAL_ERROR "Slicer executable does not exist: ${ORNLSLICER_EXECUTABLE}")
endif()

if(NOT EXISTS "${PROJECT_FILE}")
    message(FATAL_ERROR "Project file does not exist: ${PROJECT_FILE}")
endif()

get_filename_component(output_dir "${OUTPUT_PREFIX}" DIRECTORY)
file(REMOVE_RECURSE "${output_dir}")
file(MAKE_DIRECTORY "${output_dir}")

set(stdout_log "${OUTPUT_PREFIX}.stdout.log")
set(stderr_log "${OUTPUT_PREFIX}.stderr.log")
set(gcode_file "${OUTPUT_PREFIX}.gcode")

execute_process(
    COMMAND
        "${ORNLSLICER_EXECUTABLE}"
        --input_project_file "${PROJECT_FILE}"
        --output_location "${OUTPUT_PREFIX}"
    RESULT_VARIABLE slice_result
    OUTPUT_FILE "${stdout_log}"
    ERROR_FILE "${stderr_log}"
)

if(NOT slice_result EQUAL 0)
    message(FATAL_ERROR
        "Slicing failed with exit code ${slice_result}.\n"
        "stdout: ${stdout_log}\n"
        "stderr: ${stderr_log}"
    )
endif()

if(NOT EXISTS "${gcode_file}")
    message(FATAL_ERROR "Expected generated G-code does not exist: ${gcode_file}")
endif()

file(SIZE "${gcode_file}" gcode_size)
if(gcode_size LESS_EQUAL 0)
    message(FATAL_ERROR "Generated G-code is empty: ${gcode_file}")
endif()

message(STATUS "Generated ${gcode_file} (${gcode_size} bytes).")
