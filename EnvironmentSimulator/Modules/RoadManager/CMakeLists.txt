# ############################### Setting targets ####################################################################

set(TARGET
    RoadManager)

# ############################### Loading desired rules ##############################################################

include(${CMAKE_SOURCE_DIR}/support/cmake/rule/disable_static_analysis.cmake)
include(${CMAKE_SOURCE_DIR}/support/cmake/rule/disable_iwyu.cmake)
include(${CMAKE_SOURCE_DIR}/support/cmake/rule/enable_fpic.cmake)

if(MSVC)
    add_definitions("/wd4482")
endif()

# ############################### Setting target files ###############################################################

set(SOURCES
    RoadManager.cpp
    odrSpiral.cpp
    LaneIndependentRouter.cpp)

add_library(
    pugixml_lib
    ${EXTERNALS_PUGIXML_PATH}/pugixml.cpp)

set(INCLUDES
    RoadManager.hpp
    odrSpiral.h
    LaneIndependentRouter.hpp)

# ############################### Creating library ###################################################################

add_library(
    ${TARGET}
    STATIC
    ${SOURCES}
    ${SRC_ADDITIONAL}
    ${INCLUDES})

target_link_libraries(
    ${TARGET}
    PRIVATE CommonMini
            pugixml_lib
            project_options
            fmt)

target_include_directories(
    ${TARGET}
    PRIVATE ${COMMON_MINI_PATH})

target_include_directories(
    ${TARGET}
    SYSTEM
    PUBLIC ${EXTERNALS_PUGIXML_PATH}
           ${EXTERNALS_FMT_INCLUDES}
           ${ROAD_MANAGER_PATH})

disable_static_analysis(${TARGET})
disable_iwyu(${TARGET})
enable_fpic(${TARGET})
