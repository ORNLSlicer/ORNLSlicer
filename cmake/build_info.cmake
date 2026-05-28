include_guard(DIRECTORY)

include(version)

function(FetchBuildInfo)
    FetchVersion()
    set(RESULT_VAR "${RESULT_VAR}" PARENT_SCOPE)
endfunction()
