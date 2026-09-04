add_library(error INTERFACE)
target_sources(error INTERFACE
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES error.hpp)

# ConsoleLogger is written against <print>, which libstdc++ 13 does not have --
# and arm-none-eabi-g++ is 13.2. It is also the wrong abstraction for a target
# with no console. Only host_transport links this, so build it for the host
# only; logger.hpp remains includable everywhere for the Logger interface and
# NullLogger, which are header-only.
if(EMBEDDED_CPP_MCU STREQUAL "host")
  add_library(logger logger.cpp)
  target_sources(logger PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES logger.hpp)
  target_link_libraries(logger PRIVATE project_options)
endif()
