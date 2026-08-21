cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(version)
FetchCMakeFriendlyVersion()
set(TARGET_VERSION "${RESULT_VAR}")

# Define project name and base it on C++
project(
    ornlslicer
    LANGUAGES CXX C
    VERSION ${TARGET_VERSION}
    DESCRIPTION "Advanced object slicer by ORNL"
    HOMEPAGE_URL "https://github.com/ORNLSlicer/ORNLSlicer"
)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

include(build_info)
set(ORNLSLICER_PACKAGE_TYPE "local" CACHE STRING "Package/distribution type reported in runtime diagnostics.")
FetchBuildInfo()
set(ORNLSLICER_BUILD_VERSION "${RESULT_VAR}")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_info.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/include/ornlslicer/build_info.h
    @ONLY
)

option(ORNLSLICER_ENABLE_COMPILER_CACHE "Use sccache or ccache when available" ON)
if(ORNLSLICER_ENABLE_COMPILER_CACHE)
    find_program(SCCACHE_PROGRAM sccache)
    find_program(CCACHE_PROGRAM ccache)

    if(SCCACHE_PROGRAM)
        message(STATUS "Using sccache compiler launcher: ${SCCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
    elseif(CCACHE_PROGRAM)
        message(STATUS "Using ccache compiler launcher: ${CCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    else()
        message(STATUS "No compiler cache found; configure with sccache or ccache in PATH to enable one.")
    endif()
endif()

function(FindDependencies)
    find_package(Qt6 REQUIRED COMPONENTS
        Core Gui Widgets Concurrent OpenGL Network Charts Core5Compat
    )

    find_package(assimp        REQUIRED)
    find_package(Boost         REQUIRED COMPONENTS system)
    find_package(CGAL          REQUIRED)
    find_package(nlohmann_json REQUIRED)
    find_package(OpenCASCADE REQUIRED)
    find_package(VTK           REQUIRED)
    find_package(Eigen3        REQUIRED NO_MODULE)
    find_package(zip           REQUIRED)
    find_package(polyclipping  REQUIRED)
    find_package(psimpl        REQUIRED)

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

    set(OPENCASCADE_LIBRARIES TKDESTEP TKMesh)

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
        ${OPENCASCADE_LIBRARIES}
        ornl::psimpl
        ornl::polyclipping
        zip::zip
        ${VTK_LIBRARIES}
        ${OPTIONAL_PACKAGES}
    )

    set(RESULT_INCLUDE_DIRS "")
    if(OpenCASCADE_INCLUDE_DIRS)
        list(APPEND RESULT_INCLUDE_DIRS ${OpenCASCADE_INCLUDE_DIRS})
    elseif(OpenCASCADE_INCLUDE_DIR)
        list(APPEND RESULT_INCLUDE_DIRS ${OpenCASCADE_INCLUDE_DIR})
    endif()

    set(RESULT_LIBRARY_DIRS "")
    if(OpenCASCADE_LIBRARY_DIRS)
        list(APPEND RESULT_LIBRARY_DIRS ${OpenCASCADE_LIBRARY_DIRS})
    elseif(OpenCASCADE_LIBRARY_DIR)
        list(APPEND RESULT_LIBRARY_DIRS ${OpenCASCADE_LIBRARY_DIR})
    endif()

    set(RESULT_VAR ${RESULT_VAR} PARENT_SCOPE)
    set(RESULT_INCLUDE_DIRS ${RESULT_INCLUDE_DIRS} PARENT_SCOPE)
    set(RESULT_LIBRARY_DIRS ${RESULT_LIBRARY_DIRS} PARENT_SCOPE)
endfunction()

FindDependencies()
set(ORNLSLICER_DEPS ${RESULT_VAR})
set(ORNLSLICER_INCLUDE_DIRS ${RESULT_INCLUDE_DIRS})
set(ORNLSLICER_LIBRARY_DIRS ${RESULT_LIBRARY_DIRS})

# Find all project files.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/**.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/**.h")
file(GLOB_RECURSE RESOURCES CONFIGURE_DEPENDS "resources/**.qrc")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(${PROJECT_NAME}_obj OBJECT)
target_include_directories(
    ${PROJECT_NAME}_obj
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated/include
        ${ORNLSLICER_INCLUDE_DIRS}
)
target_sources(
    ${PROJECT_NAME}_obj
    PRIVATE ${SOURCES} ${RESOURCES}
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include
    FILES ${HEADERS}
)

if(ORNLSLICER_LIBRARY_DIRS)
    target_link_directories(${PROJECT_NAME}_obj PRIVATE ${ORNLSLICER_LIBRARY_DIRS})
endif()
target_link_libraries(${PROJECT_NAME}_obj ${ORNLSLICER_DEPS})
target_compile_options(${PROJECT_NAME}_obj PRIVATE $<$<COMPILE_LANGUAGE:CXX>: -fpermissive -Wno-deprecated-declarations>)

option(ORNLSLICER_ENABLE_PCH "Use precompiled headers for common C++ and Qt headers" ON)
if(ORNLSLICER_ENABLE_PCH)
    target_precompile_headers(
        ${PROJECT_NAME}_obj
        PRIVATE
            <algorithm>
            <array>
            <cmath>
            <cstddef>
            <cstdint>
            <functional>
            <limits>
            <map>
            <memory>
            <string>
            <tuple>
            <utility>
            <vector>
            <QtCore/QHash>
            <QtCore/QList>
            <QtCore/QMap>
            <QtCore/QObject>
            <QtCore/QSharedPointer>
            <QtCore/QString>
            <QtCore/QStringBuilder>
            <QtCore/QVariant>
            <QtCore/QVector>
            <QtCore/QtGlobal>
            <QtGui/QColor>
            <QtGui/QMatrix4x4>
            <QtGui/QQuaternion>
            <QtGui/QVector3D>
            <QtWidgets/QGridLayout>
            <QtWidgets/QLabel>
            <QtWidgets/QWidget>
    )
endif()

option(ORNLSLICER_ENABLE_UNITY_BUILD "Build ORNLSlicer sources with CMake unity builds" OFF)
if(ORNLSLICER_ENABLE_UNITY_BUILD)
    set_target_properties(
        ${PROJECT_NAME}_obj
        PROPERTIES
            UNITY_BUILD ON
            UNITY_BUILD_BATCH_SIZE 16
    )
endif()

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE src/main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/include)
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_obj)

if(WIN32)
    message(STATUS "Windows detected, generating Windows targets.")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-mwindows")

    add_executable(${PROJECT_NAME}_cli)
    target_sources(${PROJECT_NAME}_cli PRIVATE src/main.cpp)
    target_include_directories(${PROJECT_NAME}_cli PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/include)
    target_link_libraries(${PROJECT_NAME}_cli ${PROJECT_NAME}_obj)
endif()

option(ORNLSLICER_BUILD_TESTS "Build ORNLSlicer test executables" ${BUILD_TESTING})
if(ORNLSLICER_BUILD_TESTS)
    enable_testing()
    add_executable(${PROJECT_NAME}_mesh_repair_tests tests/mesh_repair_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_mesh_repair_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_mesh_repair_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME mesh_repair_tests COMMAND ${PROJECT_NAME}_mesh_repair_tests)

    add_executable(${PROJECT_NAME}_point_order_optimizer_tests tests/point_order_optimizer_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_point_order_optimizer_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_point_order_optimizer_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME point_order_optimizer_tests COMMAND ${PROJECT_NAME}_point_order_optimizer_tests)

    add_executable(${PROJECT_NAME}_optimization_anchor_tests tests/optimization_anchor_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_optimization_anchor_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_optimization_anchor_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME optimization_anchor_tests COMMAND ${PROJECT_NAME}_optimization_anchor_tests)

    add_executable(${PROJECT_NAME}_optimizer_empty_input_tests tests/optimizer_empty_input_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_optimizer_empty_input_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_optimizer_empty_input_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME optimizer_empty_input_tests COMMAND ${PROJECT_NAME}_optimizer_empty_input_tests)

    add_executable(${PROJECT_NAME}_gcode_settings_importer_tests tests/gcode_settings_importer_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_gcode_settings_importer_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_gcode_settings_importer_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME gcode_settings_importer_tests COMMAND ${PROJECT_NAME}_gcode_settings_importer_tests)

    add_executable(${PROJECT_NAME}_common_parser_tests tests/common_parser_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_common_parser_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_common_parser_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME common_parser_tests COMMAND ${PROJECT_NAME}_common_parser_tests)

    add_executable(${PROJECT_NAME}_arc_specialties_parser_tests tests/arc_specialties_parser_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_arc_specialties_parser_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_arc_specialties_parser_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME arc_specialties_parser_tests COMMAND ${PROJECT_NAME}_arc_specialties_parser_tests)

    add_executable(${PROJECT_NAME}_as_printed_model_exporter_tests tests/as_printed_model_exporter_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_as_printed_model_exporter_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_as_printed_model_exporter_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME as_printed_model_exporter_tests COMMAND ${PROJECT_NAME}_as_printed_model_exporter_tests)

    add_executable(${PROJECT_NAME}_session_manager_tests tests/session_manager_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_session_manager_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_session_manager_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME session_manager_tests COMMAND ${PROJECT_NAME}_session_manager_tests)

    add_executable(${PROJECT_NAME}_path_modifier_tests tests/path_modifier_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_path_modifier_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_path_modifier_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME path_modifier_tests COMMAND ${PROJECT_NAME}_path_modifier_tests)

    add_executable(${PROJECT_NAME}_spiral_path_tests tests/spiral_path_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_spiral_path_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_spiral_path_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME spiral_path_tests COMMAND ${PROJECT_NAME}_spiral_path_tests)

    add_executable(${PROJECT_NAME}_helical_clip_rounding_tests tests/helical_clip_rounding_tests.cpp)
    target_include_directories(
        ${PROJECT_NAME}_helical_clip_rounding_tests
        PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/generated/include
            ${ORNLSLICER_INCLUDE_DIRS}
    )
    target_link_libraries(${PROJECT_NAME}_helical_clip_rounding_tests PRIVATE ${PROJECT_NAME}_obj)
    add_test(NAME helical_clip_rounding_tests COMMAND ${PROJECT_NAME}_helical_clip_rounding_tests)

    set(ORNLSLICER_NEW_IMPACT_PROJECT "/home/rhc/develop/new-impact.s2p" CACHE FILEPATH
        "Optional local project used for the new-impact planar slicing regression test.")
    if(EXISTS "${ORNLSLICER_NEW_IMPACT_PROJECT}")
        add_test(
            NAME new_impact_project_regression
            COMMAND
                ${CMAKE_COMMAND}
                -DORNLSLICER_EXECUTABLE=$<TARGET_FILE:${PROJECT_NAME}>
                -DPROJECT_FILE=${ORNLSLICER_NEW_IMPACT_PROJECT}
                -DOUTPUT_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/Testing/new-impact-regression/new-impact
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_project_slice_regression.cmake
        )
    endif()
endif()

function(CreateSettingsTarget)
    option(ORNLSLICER_AUTO_GENERATE_MASTER_CONFIG "Generates the master configuration file as part of the build process" ON)

    if(ORNLSLICER_AUTO_GENERATE_MASTER_CONFIG)
        find_package(Python COMPONENTS Interpreter)
        if(Python_FOUND)
            file(
                GLOB_RECURSE MASTER_SETTING_SOURCES
                CONFIGURE_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/settings/*.yaml"
            )

            if(NOT MASTER_SETTING_SOURCES)
                message(FATAL_ERROR "No settings YAML files found under resources/settings.")
            endif()

            message(STATUS "Python interpreter found, enabling automatic master settings generator.")
            add_custom_command(
                OUTPUT
                    ${CMAKE_CURRENT_SOURCE_DIR}/resources/configs/master.conf
                    ${CMAKE_CURRENT_SOURCE_DIR}/resources/configs/setting_inputs.conf
                DEPENDS
                    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_master_config.py
                    ${MASTER_SETTING_SOURCES}
                COMMENT "Generating master settings json"
                COMMAND
                    ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_master_config.py
                    ${CMAKE_CURRENT_SOURCE_DIR}/resources/settings
                    ${CMAKE_CURRENT_SOURCE_DIR}/resources/configs/master.conf
                    ${CMAKE_CURRENT_SOURCE_DIR}/resources/configs/setting_inputs.conf
                VERBATIM
            )
            add_custom_target(
                generate_master_config
                DEPENDS
                    ${CMAKE_CURRENT_SOURCE_DIR}/resources/configs/master.conf
                    ${CMAKE_CURRENT_SOURCE_DIR}/resources/configs/setting_inputs.conf
            )
            add_dependencies(${PROJECT_NAME}_obj generate_master_config)
        else()
            message(FATAL_ERROR "Python not found, master settings generator cannot be enabled.")
        endif()
    else()
        message(STATUS "Automatic master settings generator not enabled.")
    endif()
endfunction()

function(CreateDocTarget)
    option(ORNLSLICER_BUILD_DOC "Build Doxygen documentation" OFF)
    if(ORNLSLICER_BUILD_DOC)
        find_package(Doxygen)
        if(DOXYGEN_FOUND)
            set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
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

CreateSettingsTarget()
CreateDocTarget()

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME})
if(WIN32)
    install(TARGETS ${PROJECT_NAME}_cli)
    install(FILES $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}_cli> DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(FILES ${CMAKE_CURRENT_LIST_DIR}/docs/ornlslicer-user-guide.pdf DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/templates              DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME})
