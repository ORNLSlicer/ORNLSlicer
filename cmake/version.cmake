include_guard(DIRECTORY)

function(__internalFetchVersion)
    file(READ ${CMAKE_CURRENT_LIST_DIR}/version.json JSON_STR)

    string(JSON MAJOR  GET ${JSON_STR} "major")
    string(JSON MINOR  GET ${JSON_STR} "minor")
    string(JSON PATCH  GET ${JSON_STR} "patch")
    string(JSON SUFFIX GET ${JSON_STR} "suffix")

    if(SUFFIX STREQUAL "")
        set(SUFFIX_SHORT "")
    else()
        string(SUBSTRING "${SUFFIX}" 0 1 SUFFIX_SHORT)
    endif()

    set(MAJOR  "${MAJOR}"  PARENT_SCOPE)
    set(MINOR  "${MINOR}"  PARENT_SCOPE)
    set(PATCH  "${PATCH}"  PARENT_SCOPE)
    set(SUFFIX "${SUFFIX}" PARENT_SCOPE)

    set(SUFFIX_SHORT "${SUFFIX_SHORT}" PARENT_SCOPE)
endfunction()

function(FetchCMakeFriendlyVersion)
    __internalFetchVersion()

    set(CMAKE_FRIENDLY_VERSION "${MAJOR}.${MINOR}.${PATCH}")
    message(STATUS "CMake friendly version: ${CMAKE_FRIENDLY_VERSION}")

    set(RESULT_VAR "${CMAKE_FRIENDLY_VERSION}" PARENT_SCOPE)
endfunction()

function(FetchVersion)
    __internalFetchVersion()

    if(SUFFIX STREQUAL "")
        set(VERSION "${MAJOR}.${MINOR}.${PATCH}")
    else()
        set(VERSION "${MAJOR}.${MINOR}.${PATCH}-${SUFFIX}")
    endif()
    message(STATUS "Version: ${VERSION}")

    set(RESULT_VAR "${VERSION}" PARENT_SCOPE)
endfunction()
