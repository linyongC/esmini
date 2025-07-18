# ############################### Setting targets ####################################################################

set(TARGET
    esminiRMLib)

# ############################### Loading desired rules ##############################################################

include(${CMAKE_SOURCE_DIR}/support/cmake/rule/disable_static_analysis.cmake)
include(${CMAKE_SOURCE_DIR}/support/cmake/rule/disable_iwyu.cmake)

# ############################### Setting target files ###############################################################

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/esminiRMLib.cpp)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/esminiRMLib.hpp)

# ############################### Creating library ###################################################################

add_library(
    ${TARGET}
    SHARED
    ${SOURCES}
    ${INCLUDES})

target_link_libraries(
    ${TARGET}
    PRIVATE project_options)

target_include_directories(
    ${TARGET}
    PRIVATE ${SCENARIO_ENGINE_PATH}/SourceFiles
            ${SCENARIO_ENGINE_PATH}/OSCTypeDefs
            ${COMMON_MINI_PATH})

target_include_directories(
    ${TARGET}
    SYSTEM
    PUBLIC ${ROAD_MANAGER_PATH}
           ${EXTERNALS_PUGIXML_PATH})

target_link_libraries(
    ${TARGET}
    PRIVATE RoadManager
    PRIVATE CommonMini
    # PRIVATE ${FMT_LIBRARIES}
    PRIVATE ${TIME_LIB})

disable_static_analysis(${TARGET})
disable_iwyu(${TARGET})

# ############################### Install ############################################################################

install(
    TARGETS ${TARGET}
    DESTINATION "${INSTALL_PATH}")

if(BUILD_EXAMPLES)
    if(WIN32)
        install(
            TARGETS ${TARGET}
            RUNTIME DESTINATION "${CODE_EXAMPLES_BIN_PATH}")
    else()
        install(
            TARGETS ${TARGET}
            LIBRARY DESTINATION "${CODE_EXAMPLES_BIN_PATH}")
    endif()
endif(BUILD_EXAMPLES)
