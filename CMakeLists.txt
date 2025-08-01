cmake_minimum_required(VERSION 3.14)

project(sbfoster_midas_unpacking_pipeline_plugin VERSION 1.0 LANGUAGES CXX)

# Set the project namespace; overrideable by parent scope if desired
if(NOT DEFINED PROJECT_NAMESPACE)
  set(PROJECT_NAMESPACE analysis_pipeline)
endif()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Suppress false-positive GCC warning if top-level
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wno-stringop-overflow)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---- CPM Setup ----
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

# ---- MIDAS (external) ----
if(NOT DEFINED ENV{MIDASSYS})
  message(FATAL_ERROR "MIDASSYS must be set in the environment.")
endif()

set(MIDASSYS_INCLUDE_DIRS $ENV{MIDASSYS}/include $ENV{MIDASSYS}/midasio)
set(MIDASSYS_LIB_DIR $ENV{MIDASSYS}/lib)

# ---- Core dependencies ----
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree Hist)
find_package(analysis_pipeline_core REQUIRED)
find_package(unpacker_data_products_core REQUIRED)
find_package(midas_event_unpacker_plugin REQUIRED)
find_package(DataProducts REQUIRED)
find_package(Unpackers REQUIRED)

# ---- Sources ----
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE STAGE_HEADERS CONFIGURE_DEPENDS include/stages/*.h)
list(FILTER STAGE_HEADERS EXCLUDE REGEX "LinkDef\\.h$")
set(ROOT_LINKDEF include/LinkDef.h)

# ---- Build plugin library ----
add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(${PROJECT_NAMESPACE}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
add_library(${PROJECT_NAMESPACE}::sbfoster_midas_unpacking_pipeline ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

foreach(dir IN LISTS MIDASSYS_INCLUDE_DIRS)
  target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${dir}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/midas>
  )
endforeach()

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    ROOT::Core ROOT::RIO ROOT::Tree ROOT::Hist
    analysis_pipeline::spdlog_header_only
    analysis_pipeline::nlohmann_json_header_only
    analysis_pipeline::analysis_pipeline_core
    analysis_pipeline::unpacker_data_products_core
    analysis_pipeline::midas_event_unpacker_plugin
    DataProducts::data_products
    Unpackers::unpackers
    ${MIDASSYS_LIB_DIR}/libmfe.a
    ${MIDASSYS_LIB_DIR}/libmidas.a
)

# ---- ROOT Dictionary ----
set(ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/include")
foreach(dir IN LISTS MIDASSYS_INCLUDE_DIRS)
  list(APPEND ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${dir}")
endforeach()

function(append_target_includes_to_root_dict tgt)
  get_target_property(incs ${tgt} INTERFACE_INCLUDE_DIRECTORIES)
  foreach(dir IN LISTS incs)
    if(dir MATCHES "^\\$<BUILD_INTERFACE:(.+)>$")
      list(APPEND ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${CMAKE_MATCH_1}")
    elseif(NOT dir MATCHES "^\\$")
      list(APPEND ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${dir}")
    endif()
  endforeach()
  set(ROOT_DICTIONARY_INCLUDE_OPTIONS ${ROOT_DICTIONARY_INCLUDE_OPTIONS} PARENT_SCOPE)
endfunction()

# ----------------------- Plugin Root Detection --------------------
file(GLOB PLUGIN_ROOT_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline
  ${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline/*
)

set(PLUGIN_ROOTS "")
foreach(dir IN LISTS PLUGIN_ROOT_DIRS)
  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline/${dir})
    list(APPEND PLUGIN_ROOTS ${dir})
  endif()
endforeach()

list(LENGTH PLUGIN_ROOTS plugin_root_count)
if(plugin_root_count EQUAL 1)
  list(GET PLUGIN_ROOTS 0 PLUGIN_ROOT_DIR)
else()
  message(FATAL_ERROR
    "Expected one plugin directory under include/analysis_pipeline/, found ${plugin_root_count}: ${PLUGIN_ROOTS}")
endif()

set(PLUGIN_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_pipeline/${PLUGIN_ROOT_DIR}")

# ----------------------- ROOT Dictionary Generation --------------
set(ROOT_LINKDEF ${PLUGIN_ROOT_PATH}/LinkDef.h)
set(ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/include")

append_target_includes_to_root_dict(analysis_pipeline::analysis_pipeline_core)
append_target_includes_to_root_dict(analysis_pipeline::unpacker_data_products_core)
append_target_includes_to_root_dict(analysis_pipeline::midas_event_unpacker_plugin)
append_target_includes_to_root_dict(DataProducts::data_products)
append_target_includes_to_root_dict(Unpackers::unpackers)

foreach(midas_dir IN LISTS MIDAS_INCLUDE_DIRS)
  list(APPEND ROOT_DICTIONARY_INCLUDE_OPTIONS "-I${midas_dir}")
endforeach()

file(GLOB_RECURSE STAGE_HEADERS CONFIGURE_DEPENDS
  ${PLUGIN_ROOT_PATH}/stages/*.h)
file(GLOB_RECURSE DATAPRODUCT_HEADERS CONFIGURE_DEPENDS
  ${PLUGIN_ROOT_PATH}/data_products/*.h)

set(ALL_DICT_HEADERS ${STAGE_HEADERS} ${DATAPRODUCT_HEADERS})
list(FILTER ALL_DICT_HEADERS EXCLUDE REGEX "LinkDef\\.h$")

ROOT_GENERATE_DICTIONARY(G__${PROJECT_NAME}
  ${ALL_DICT_HEADERS}
  LINKDEF ${ROOT_LINKDEF}
  OPTIONS ${ROOT_DICTIONARY_INCLUDE_OPTIONS} -Wno-inconsistent-missing-override
)

target_sources(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/G__${PROJECT_NAME}.cxx
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# ---- Install (only if top-level) ----
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.rootmap
    ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}_rdict.pcm
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )

  install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAMESPACE}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  )
endif()
