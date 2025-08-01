# ############################### Setting targets ####################################################################

set(TARGET
    ScenarioEngine)

# ############################### Loading desired rules ##############################################################

include(${CMAKE_SOURCE_DIR}/support/cmake/rule/disable_static_analysis.cmake)
include(${CMAKE_SOURCE_DIR}/support/cmake/rule/disable_iwyu.cmake)
include(${CMAKE_SOURCE_DIR}/support/cmake/rule/enable_fpic.cmake)

# ############################### Setting target files ###############################################################

file(
    GLOB
    SRC_OSCTYPEDEFS
    "${CMAKE_CURRENT_LIST_DIR}/OSCTypeDefs/*.[hc]pp")
file(
    GLOB
    SRC_SOURCEFILES
    "${CMAKE_CURRENT_LIST_DIR}/SourceFiles/*.[hc]pp"
    "${CMAKE_CURRENT_LIST_DIR}/SourceFiles/*.[hc]")

if(NOT
   USE_OSI)
    list(
        REMOVE_ITEM
        SRC_SOURCEFILES
        "${CMAKE_CURRENT_LIST_DIR}/SourceFiles/OSIReporter.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/SourceFiles/OSITrafficCommand.hpp")
    list(
        REMOVE_ITEM
        SRC_SOURCEFILES
        "${CMAKE_CURRENT_LIST_DIR}/SourceFiles/OSIReporter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SourceFiles/OSITrafficCommand.cpp")
endif(
    NOT
    USE_OSI)

set(SRC_ADDITIONAL
    ${EXTERNALS_PUGIXML_PATH}/pugixml.cpp)

source_group(
    OSCTypeDefs
    FILES ${SRC_OSCTYPEDEFS})
source_group(
    "Source Files"
    FILES ${SRC_SOURCEFILES})
source_group(
    "External Libraries"
    FILES ${SRC_ADDITIONAL})

# ############################### Creating library ###################################################################

add_library(
    ${TARGET}
    STATIC
    ${SRC_OSCTYPEDEFS}
    ${SRC_SOURCEFILES})

target_link_libraries(
    ${TARGET}
    PRIVATE project_options)

target_include_directories(
    ${TARGET}
    PRIVATE ${SCENARIO_ENGINE_PATH}/SourceFiles
            ${SCENARIO_ENGINE_PATH}/OSCTypeDefs
            ${COMMON_MINI_PATH}
            ${CONTROLLERS_PATH}
            ${REPLAYER_PATH})

target_include_directories(
    ${TARGET}
    SYSTEM
    PUBLIC ${ROAD_MANAGER_PATH}
           ${EXTERNALS_OSI_INCLUDES}
           ${EXTERNALS_SUMO_INCLUDES}
           ${EXTERNALS_PUGIXML_PATH}
           ${EXTERNALS_EXPR_PATH}
           ${EXTERNALS_FMT_INCLUDES})

target_link_libraries(
    ${TARGET}
    PRIVATE CommonMini)

disable_static_analysis(${TARGET})
disable_iwyu(${TARGET})
enable_fpic(${TARGET})
