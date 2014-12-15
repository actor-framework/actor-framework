INCLUDE(CMakeForceCompiler)

# Apparently this is important (not sure what system to set)
SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_PROCESSOR STM32F407)
SET(CMAKE_CROSSCOMPILING TRUE)

# sepcify cross compiler
#SET(CMAKE_FORCE_C_COMPILER arm-none-eabi-gcc GNU)
#SET(CMAKE_FORCE_CXX_COMPILER arm-none-eabi-g++ GNU)
#SET(CMAKE_C_COMPILER arm-none-eabi-gcc GNU)
#SET(CMAKE_CXX_COMPILER arm-none-eabi-g++ GNU)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_C_COMPILER_ID_RUN TRUE)
set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_C_COMPILER_FORCED TRUE)

set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_CXX_COMPILER_ID_RUN TRUE)
set(CMAKE_CXX_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

add_definitions(-DCOREIF_NG=1) # required for exceptions on this board with RIOT
include_directories("${RIOT_BASE_DIR}/boards/stm32f4discovery/include/.")

#SET(CMAKE_FIND_ROOT_PATH  /home/noir/Downloads/gcc-arm-none-eabi-4_8-2014q3/arm-none-eabi/ /home/noir/Downloads/gcc-arm-none-eabi-4_8-2014q3/)

# search for programs in the build host directories
#SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
#SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
#SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)


# RIOT headers
# SET(CMAKE_FIND_ROOT_PATH )

SET(CROSS_CXX_FLAGS "-static-libgcc -static-libstdc++ -static -Os -s -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mthumb -mno-thumb-interwork -mfpu=vfp -mfix-cortex-m3-ldrd -mlittle-endian -nostartfiles -ffunction-sections -fdata-sections -fno-builtin") # -march=armv7-m -mtune=cortex-m4
SET(CROSS_C_FLAGS "-static-libgcc -static-libstdc++ -static -Os -s -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mthumb -mno-thumb-interwork -mfpu=vfp -mfix-cortex-m3-ldrd -mlittle-endian -nostartfiles -ffunction-sections -fdata-sections -fno-builtin") # -march=armv7-m -mtune=cortex-m4
#SET(CROSS_CXX_FLAGS "-static-libgcc -static-libstdc++ -static -Os -s -march=armv7-m -mtune=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mthumb -mno-thumb-interwork -mfpu=vfp -mfix-cortex-m3-ldrd -mlittle-endian -nostartfiles -ffunction-sections -fdata-sections -fno-builtin")
#SET(CROSS_C_FLAGS "-static-libgcc -static-libstdc++ -static -Os -s -march=armv7-m -mtune=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mthumb -mno-thumb-interwork -mfpu=vfp -mfix-cortex-m3-ldrd -mlittle-endian -nostartfiles -ffunction-sections -fdata-sections -fno-builtin")
SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-static-libgcc -static-libstdc++ -static -Os -s")
SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "-static-libgcc -static-libstdc++ -static -Os -s")
