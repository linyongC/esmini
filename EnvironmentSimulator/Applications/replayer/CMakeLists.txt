# ############################### Setting targets ####################################################################

set(TARGET1
    replayer)

set(TARGET2
    dat2csv)

set(TARGET3
    osireceiver)

# ############################### Loading desired rules ##############################################################

include(${CMAKE_SOURCE_DIR}/support/cmake/rule/disable_static_analysis.cmake)
include(${CMAKE_SOURCE_DIR}/support/cmake/rule/disable_iwyu.cmake)

# ############################### Setting target files ###############################################################

set(TARGET1_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Replay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helpText.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/collision.hpp)

set(TARGET1_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/Replay.hpp)

set(TARGET2_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/dat2csv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Replay.cpp)

set(TARGET3_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/osi_receiver.cpp)

# ############################### Creating executable for target1 (replayer) #########################################

if(BUILD_REPLAYER)

    add_executable(
        ${TARGET1}
        ${TARGET1_SOURCES}
        ${TARGET1_INCLUDES})

    target_include_directories(
        ${TARGET1}
        PRIVATE ${SCENARIO_ENGINE_PATH}/SourceFiles
                ${SCENARIO_ENGINE_PATH}/OSCTypeDefs
                ${VIEWER_BASE_PATH}
                ${CONTROLLERS_PATH}
                ${COMMON_MINI_PATH})

    target_include_directories(
        ${TARGET1}
        SYSTEM
        PUBLIC ${ROAD_MANAGER_PATH}
               ${EXTERNALS_OSI_INCLUDES}
               ${EXTERNALS_PUGIXML_PATH}
               ${EXTERNALS_DIRENT_INCLUDES})

    if(USE_OSG)
        target_include_directories(
            ${TARGET1}
            SYSTEM
            PUBLIC ${EXTERNALS_OSG_INCLUDES})
    endif()

    target_link_libraries(
        ${TARGET1}
        PRIVATE project_options
                RoadManager
                ScenarioEngine
                CommonMini
                Controllers
                ${TIME_LIB})

    if(USE_OSG)
        target_link_libraries(
            ${TARGET1}
            PRIVATE ViewerBase
                    ScenarioEngine
                    ${OSG_LIBRARIES})
    endif()

    if(USE_OSI)
        target_link_libraries(
            ${TARGET1}
            PRIVATE ${OSI_LIBRARIES})
    endif()

    disable_static_analysis(${TARGET1})
    disable_iwyu(${TARGET1})

    install(
        TARGETS ${TARGET1}
        DESTINATION "${INSTALL_PATH}")

endif(BUILD_REPLAYER)

# ############################### Creating executable for target2 (dat2csv) ##########################################

add_executable(
    ${TARGET2}
    ${TARGET2_SOURCES})

target_link_libraries(
    ${TARGET2}
    PRIVATE project_options
            RoadManager
            CommonMini
            ${TIME_LIB})

target_include_directories(
    ${TARGET2}
    PRIVATE ${COMMON_MINI_PATH}
            ${SCENARIO_ENGINE_PATH}/SourceFiles
            ${SCENARIO_ENGINE_PATH}/OSCTypeDefs
            ${CONTROLLERS_PATH})

target_include_directories(
    ${TARGET2}
    SYSTEM
    PUBLIC ${ROAD_MANAGER_PATH}
           ${EXTERNALS_OSI_INCLUDES}
           ${EXTERNALS_PUGIXML_PATH}
           ${EXTERNALS_OSG_INCLUDES}
           ${EXTERNALS_DIRENT_INCLUDES})

if(USE_OSI)
    target_link_libraries(
        ${TARGET2}
        PRIVATE ${OSI_LIBRARIES})
endif()

disable_static_analysis(${TARGET2})
disable_iwyu(${TARGET2})

install(
    TARGETS ${TARGET2}
    DESTINATION "${INSTALL_PATH}")

# ############################### Creating executable for target3 (osireceiver) ######################################

if(USE_OSI)

    add_executable(
        ${TARGET3}
        ${TARGET3_SOURCES})

    target_include_directories(
        ${TARGET3}
        SYSTEM
        PUBLIC ${ROAD_MANAGER_PATH}
               ${EXTERNALS_OSI_INCLUDES}
               ${EXTERNALS_DIRENT_INCLUDES})

    target_link_libraries(
        ${TARGET3}
        PRIVATE project_options
                ${TIME_LIB}
                ${SOCK_LIB}
                ${OSI_LIBRARIES})

    disable_static_analysis(${TARGET3})
    disable_iwyu(${TARGET3})

    install(
        TARGETS ${TARGET3}
        DESTINATION "${INSTALL_PATH}")

endif()
