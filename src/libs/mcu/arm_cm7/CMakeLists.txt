# Cortex-M7 MCU backend.
#
# A note on the name: only cortex_m7.* , systick.* and delay.cpp are genuinely
# core peripherals. Everything else here is STM32F7-specific and belongs to a
# vendor layer that does not exist yet, because there is exactly one consumer.
# The filenames carry the distinction; split the directory when an arm_cm4
# backend arrives and wants to share the GPIO code.
add_library(arm_cm7_mcu
  cortex_m7.cpp systick.cpp delay.cpp gpio_pin.cpp gpio_port.cpp exti.cpp usart.cpp i2c.cpp)
target_sources(arm_cm7_mcu PUBLIC
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES cmsis.hpp cortex_m7.hpp systick.hpp
        gpio_pin.hpp gpio_port.hpp gpio_registers.hpp exti.hpp
        usart.hpp i2c.hpp)
# cmsis_f767 is PUBLIC because cmsis.hpp, a public header, includes it.
target_link_libraries(arm_cm7_mcu
  PUBLIC mcu cmsis_f767
  PRIVATE project_options)
