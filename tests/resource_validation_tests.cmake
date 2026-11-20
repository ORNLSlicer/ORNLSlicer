set(FIXTURE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/resource-validation-fixture")
file(REMOVE_RECURSE "${FIXTURE_ROOT}")
file(MAKE_DIRECTORY "${FIXTURE_ROOT}/resources" "${FIXTURE_ROOT}/docs")
file(WRITE "${FIXTURE_ROOT}/resources/test.qrc"
    "<RCC><qresource><file alias=\"icon\">image with spaces.png</file><file>../docs/guide.pdf</file></qresource></RCC>")
file(WRITE "${FIXTURE_ROOT}/resources/image with spaces.png" "hydrated asset bytes")
file(WRITE "${FIXTURE_ROOT}/docs/guide.pdf" "hydrated document bytes")

function(CheckValidation EXPECTED_RESULT EXPECTED_MESSAGE)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DRESOURCE_ROOT=${FIXTURE_ROOT}/resources" -P "${VALIDATOR}"
        RESULT_VARIABLE RESULT
        OUTPUT_VARIABLE OUTPUT
        ERROR_VARIABLE ERROR
    )
    if(EXPECTED_RESULT STREQUAL "success")
        if(NOT RESULT EQUAL 0)
            message(FATAL_ERROR "Hydrated resources were rejected:\n${OUTPUT}${ERROR}")
        endif()
    else()
        if(RESULT EQUAL 0 OR NOT "${OUTPUT}${ERROR}" MATCHES "${EXPECTED_MESSAGE}")
            message(FATAL_ERROR "Expected rejection mentioning ${EXPECTED_MESSAGE}:\n${OUTPUT}${ERROR}")
        endif()
        if(NOT "${OUTPUT}${ERROR}" MATCHES "git lfs pull")
            message(FATAL_ERROR "Missing LFS recovery instructions:\n${OUTPUT}${ERROR}")
        endif()
    endif()
endfunction()

CheckValidation(success "")

set(POINTER "version https://git-lfs.github.com/spec/v1\noid sha256:0123456789abcdef\nsize 123\n")
file(WRITE "${FIXTURE_ROOT}/resources/image with spaces.png" "${POINTER}")
CheckValidation(failure "image with spaces.png")

file(WRITE "${FIXTURE_ROOT}/resources/image with spaces.png" "hydrated asset bytes")
file(WRITE "${FIXTURE_ROOT}/docs/guide.pdf" "${POINTER}")
CheckValidation(failure "guide.pdf")

file(WRITE "${FIXTURE_ROOT}/docs/guide.pdf" "hydrated document bytes")
CheckValidation(success "")
file(REMOVE_RECURSE "${FIXTURE_ROOT}")
