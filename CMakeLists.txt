cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(version)
FetchCMakeFriendlyVersion()
set(TARGET_VERSION "${RESULT_VAR}")

# Define project name and base it on C++
project(
    slicer2
    LANGUAGES CXX C
    VERSION ${TARGET_VERSION}
    DESCRIPTION "Advanced object slicer by ORNL"
    HOMEPAGE_URL "https://github.com/ORNLSlicer/Slicer-2"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui")

include(build_info)
FetchBuildInfo()

function(FindDependencies)
    find_package(Qt6 REQUIRED COMPONENTS
        Core Gui Widgets Concurrent OpenGL Network Charts Core5Compat
    )

    find_package(assimp        REQUIRED)
    find_package(Boost         REQUIRED COMPONENTS system)
    find_package(CGAL          REQUIRED)
    find_package(nlohmann_json REQUIRED)
    find_package(VTK           REQUIRED)
    find_package(Eigen3        REQUIRED NO_MODULE)
    find_package(zip           REQUIRED)
    find_package(polyclipping  REQUIRED)
    find_package(psimpl        REQUIRED)
    find_package(sockets       REQUIRED)

    include(CGAL_Eigen3_support)
    if(TARGET CGAL::Eigen3_support)
        message(STATUS "Eigen found and configred with CGAL.")
    else()
    	message(FATAL_ERROR "CGAL was NOT compiled with Eigen support.")
    endif()

    set(OPTIONAL_PACKAGES "")
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "OpenMP found, enabled.")
        list(APPEND OPTIONAL_PACKAGES OpenMP::OpenMP_CXX)
    else()
        message(WARNING "OpenMP NOT found, the build will not be optimized.")
    endif()

    set(RESULT_VAR "")
    list(
        APPEND RESULT_VAR
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::Core
        Qt6::Concurrent
        Qt6::Network
        Qt6::Charts
        Qt6::Core5Compat
        assimp::assimp
        Boost::system
        CGAL::CGAL
        Eigen3::Eigen
        CGAL::Eigen3_support
        nlohmann_json::nlohmann_json
        ornl::sockets
        ornl::psimpl
        ornl::polyclipping
        zip::zip
        ${VTK_LIBRARIES}
        ${OPTIONAL_PACKAGES}
    )

    set(RESULT_VAR ${RESULT_VAR} PARENT_SCOPE)
endfunction()

FindDependencies()
set(SLICER2_DEPS ${RESULT_VAR})

# Find all project files.
file(GLOB_RECURSE SOURCES "src/**.cpp")
file(GLOB_RECURSE HEADERS "include/**.h")
file(GLOB_RECURSE RESOURCES "resources/**.qrc")

add_library(${PROJECT_NAME}_obj OBJECT)
target_sources(
    ${PROJECT_NAME}_obj
    PRIVATE ${SOURCES} ${RESOURCES}
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include
    FILES ${HEADERS}
)

target_link_libraries(${PROJECT_NAME}_obj ${SLICER2_DEPS})
target_compile_options(${PROJECT_NAME}_obj PRIVATE $<$<COMPILE_LANGUAGE:CXX>: -fpermissive -Wno-deprecated-declarations>)

add_executable(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_obj)

if(WIN32)
    message(STATUS "Windows detected, generating Windows targets.")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-mwindows -Wl,--nxcompat -Wl,--dynamicbase -Wl,--high-entropy-va")

    add_executable(${PROJECT_NAME}_cli)
    target_link_libraries(${PROJECT_NAME}_cli ${PROJECT_NAME}_obj)
endif()

function(CreateODSTarget)
    option(SLICER2_AUTO_GENERATE_MASTER_CONFIG "Generates the master configuration file as part of the build process" ON)

    if(SLICER2_AUTO_GENERATE_MASTER_CONFIG)
        find_package(Python COMPONENTS Interpreter)
        if(Python_FOUND)
            message(STATUS "Python interpreter found, enabling automatic master ODS -> JSON generator.")
            add_custom_command(
                OUTPUT  ${CMAKE_CURRENT_SOURCE_DIR}/resources/configs/master.conf
                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mss.ods
                COMMENT "Generating master settings json"
                COMMAND
                    ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/ods_to_json.py
                    ${CMAKE_CURRENT_SOURCE_DIR}/mss.ods
                    ${CMAKE_CURRENT_SOURCE_DIR}/resources/configs/master.conf
            )
        else()
            message(FATAL_ERROR "Python not found, ODS -> JSON generator cannot be enabled.")
        endif()
    else()
        message(STATUS "Automatic ODS -> JSON generator not enabled.")
    endif()
endfunction()

function(CreateDocTarget)
    option(SLICER2_BUILD_DOC "Build Doxygen documentation" OFF)
    if(SLICER2_BUILD_DOC)
        find_package(Doxygen)
        if(DOXYGEN_FOUND)
            set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in)
            set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

            configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
            message(STATUS "Automatic Doxygen generation enabled")

            add_custom_target(
                doc_doxygen ALL
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMENT "Generating API documentation with Doxygen"
                VERBATIM
            )
        else()
            message(FATAL_ERROR "Doxygen not found, cannot enable documentation generation.")
        endif()
    else()
        message(STATUS "Doxygen documentation generation not enabled.")
    endif()
endfunction()

CreateODSTarget()
CreateDocTarget()

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME})
if(WIN32)
    install(TARGETS ${PROJECT_NAME}_cli)
endif()

install(FILES ${CMAKE_CURRENT_LIST_DIR}/doc/slicer2_user_guide.pdf DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/templates              DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME})

