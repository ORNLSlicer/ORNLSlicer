# RCC accepts Git LFS pointer text as resource data, producing a GUI with missing images.
function(ValidateResources RESOURCE_DIRECTORY)
    file(GLOB_RECURSE RESOURCE_COLLECTIONS "${RESOURCE_DIRECTORY}/*.qrc")
    set(POINTER_FILES "")
    foreach(COLLECTION IN LISTS RESOURCE_COLLECTIONS)
        get_filename_component(COLLECTION_DIRECTORY "${COLLECTION}" DIRECTORY)
        file(READ "${COLLECTION}" COLLECTION_XML)
        string(REGEX MATCHALL "<file[^>]*>[^<]+</file>" RESOURCE_ENTRIES "${COLLECTION_XML}")
        foreach(ENTRY IN LISTS RESOURCE_ENTRIES)
            string(REGEX REPLACE "<file[^>]*>([^<]+)</file>" "\\1" RESOURCE_PATH "${ENTRY}")
            get_filename_component(RESOURCE_PATH "${COLLECTION_DIRECTORY}/${RESOURCE_PATH}" ABSOLUTE)
            file(READ "${RESOURCE_PATH}" HEADER LIMIT 64)
            if(HEADER MATCHES "^version https://git-lfs.github.com/spec/v1")
                list(APPEND POINTER_FILES "${RESOURCE_PATH}")
            endif()
        endforeach()
    endforeach()

    if(POINTER_FILES)
        list(REMOVE_DUPLICATES POINTER_FILES)
        list(LENGTH POINTER_FILES POINTER_COUNT)
        list(SUBLIST POINTER_FILES 0 5 EXAMPLES)
        list(JOIN EXAMPLES "\n  " EXAMPLES)
        message(FATAL_ERROR
            "${POINTER_COUNT} bundled resources are unresolved Git LFS pointers, including:\n  ${EXAMPLES}\n"
            "These would cause missing GUI icons, arrows, and controls.\n"
            "From the repository root, run:\n  git lfs install\n  git lfs pull\n"
            "If the objects are already cached, run git lfs checkout instead.\n"
            "Then configure and rebuild the application.")
    endif()
endfunction()

if(DEFINED RESOURCE_ROOT)
    ValidateResources("${RESOURCE_ROOT}")
endif()
