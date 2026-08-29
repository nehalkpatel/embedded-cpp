add_library(error INTERFACE)
target_sources(error INTERFACE
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES error.hpp)

add_library(logger logger.cpp)
target_sources(logger PUBLIC
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES logger.hpp)
target_link_libraries(logger PRIVATE project_options)
