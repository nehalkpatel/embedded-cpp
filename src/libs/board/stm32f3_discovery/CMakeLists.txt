cmake_minimum_required(VERSION 3.27)

if(CMAKE_PRESET STREQUAL "stm32f3_discovery")
  include(FetchContent)
  FetchContent_Declare(
    stm32cubef3
    GIT_REPOSITORY https://github.com/STMicroelectronics/STM32CubeF3
    GIT_TAG v1.11.4
  )
  FetchContent_MakeAvailable(stm32cubef3)
  
  file(GLOB HAL_SRC
    "${stm32cubef3_SOURCE_DIR}/Drivers/STM32F3xx_HAL_Driver/Src/*.c"
  )

  add_library(stm32f3_hal STATIC
    ${HAL_SRC}
    startup_stm32f303xc.s
    ${stm32cubef3_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F3xx/Source/Templates/system_stm32f3xx.c
  )
target_compile_options(stm32f3_hal INTERFACE ${COMMON_COMPILE_OPTIONS})
  add_library(stm32f3_sys STATIC
    syscalls.c
    main.cpp
  )
target_compile_options(stm32f3_sys INTERFACE ${COMMON_COMPILE_OPTIONS})
  # target_link_options(stm32f3_sys PUBLIC -lc -lstdc++)

  target_include_directories(stm32f3_hal PUBLIC
    ${stm32cubef3_SOURCE_DIR}/Drivers/STM32F3xx_HAL_Driver/Inc
    ${stm32cubef3_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F3xx/Include
    ${stm32cubef3_SOURCE_DIR}/Drivers/CMSIS/Core/Include
    ${stm32cubef3_SOURCE_DIR}/Drivers/CMSIS/Include
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
  target_compile_definitions(stm32f3_hal PUBLIC STM32F3 STM32F303xC USE_HAL_DRIVER)

  # target_include_directories(stm32f3_sys PUBLIC ${stm32cubef3_SOURCE_DIR}/Drivers/STM32F3xx_HAL_Driver/Inc
  #   ${stm32cubef3_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F3xx/Include
  #   ${stm32cubef3_SOURCE_DIR}/Drivers/CMSIS/Core/Include
  #   ${stm32cubef3_SOURCE_DIR}/Drivers/CMSIS/Include
  #   PRIVATE
  #   ${CMAKE_CURRENT_SOURCE_DIR}
  # )
  # target_compile_definitions(stm32f3_sys PUBLIC STM32F3 STM32F303xC USE_HAL_DRIVER)

  target_link_options(stm32f3_hal PUBLIC -TSTM32F303VCTx_FLASH.ld -L${CMAKE_CURRENT_SOURCE_DIR})
endif()
