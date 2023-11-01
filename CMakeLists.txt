cmake_minimum_required(VERSION 3.20)

project(
  CAN2CAN
  VERSION 0.1.0
  DESCRIPTION "CAN to CAN communication"
  LANGUAGES C CXX ASM
)

message("Build type: " ${CMAKE_BUILD_TYPE})

# Setup compiler settings
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# include firmware file include("cmake/st-hal.cmake")

# ----------------------------------------------------------------------------
# Sources
# ----------------------------------------------------------------------------
set(PORT_DIR ${CMAKE_SOURCE_DIR}/Port/STM32F103xx)

# project source fles --------------------------------------------------------
set(SOURCE_FILES
  ${CMAKE_SOURCE_DIR}/Core/Src/main.c
  ${CMAKE_SOURCE_DIR}/Core/Src/gpio.c
  ${CMAKE_SOURCE_DIR}/Core/Src/freertos.c
  ${CMAKE_SOURCE_DIR}/Core/Src/can.c
  ${CMAKE_SOURCE_DIR}/Core/Src/stm32f1xx_it.c
  ${CMAKE_SOURCE_DIR}/Core/Src/stm32f1xx_hal_msp.c
  ${CMAKE_SOURCE_DIR}/Core/Src/stm32f1xx_hal_timebase_tim.c
  ${CMAKE_SOURCE_DIR}/Core/Src/syscalls.c
  ${CMAKE_SOURCE_DIR}/Core/Src/sysmem.c
  ${CMAKE_SOURCE_DIR}/Core/Src/system_stm32f1xx.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_can.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/croutine.c
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/list.c
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/queue.c
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/tasks.c
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/timers.c
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c
  ${PORT_DIR}/startup_stm32f103xb.s
)

# Include directories --------------------------------------------------------
set(INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/Core/Inc
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Inc
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS
  ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3
  ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
  ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
  ${PORT_DIR}
)

# Symbols definition for all compilers ---------------------------------------
set(COMPILER_DEFS
  USE_HAL_DRIVER 
  STM32F103xB 
)

set(PROJECT_DEFINITIONS
  ${COMPILER_DEFS}
)

# Compiler Options -----------------------------------------------------------
set(CPU         -mcpu=cortex-m3)
set(CPU_PARAMS  ${CPU} -mthumb -mfloat-abi=soft)

set(COMPILER_OPTS
  ${CPU_PARAMS}
)

# Linker script --------------------------------------------------------------
set(LINKER_SCRIPT ${PORT_DIR}/STM32F103CBTx_FLASH.ld)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} 
  PUBLIC 
  ${SOURCE_FILES}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME}
  PUBLIC 
  ${INCLUDE_DIRS}
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME}
  PUBLIC
  ${PROJECT_DEFINITIONS}
  $<$<CONFIG:Debug>:DEBUG>$<$<CONFIG:Release>:> # Configuration specific
)

# Compiler options
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE ${COMPILER_OPTS}
          -Wall
          -Wextra
          -Wpedantic
          -Wno-unused-parameter
          -MMD 
          -MP
          $<$<COMPILE_LANGUAGE:C>:>
          $<$<COMPILE_LANGUAGE:CXX>:-Wno-volatile -Wold-style-cast -Wuseless-cast -Wsuggest-override>
          $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp -MMD -MP>
          $<$<CONFIG:Debug>:-Og -g3 -ggdb -g -gdwarf-2>
          $<$<CONFIG:Release>:-Og -g0>
)

# Linker options
target_link_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
  -T${LINKER_SCRIPT}
  ${CPU_PARAMS}
  -static  
  -Wl,--start-group 
  -lc 
  -lm 
  -Wl,--end-group
  -Wl,-Map=${CMAKE_PROJECT_NAME}.map,--cref
  -Wl,--gc-sections
  --specs=nano.specs
  --specs=nosys.specs 
  -Wl,-z,max-page-size=8 # Allow good software remapping across address space (with proper GCC section making)
  -Wl,--print-memory-usage
)

# Execute post-build to print size, generate hex and bin
add_custom_command(
  TARGET ${CMAKE_PROJECT_NAME}
  POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
)
