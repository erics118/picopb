# cross-compiles for the rp2040's cortex-m0+ with the firmware's compiler, arm-none-eabi-g++
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m0plus)

set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-m0plus -mthumb -fno-exceptions -fno-rtti")

# nosys.specs stubs out the system calls, since there's no operating system
set(CMAKE_EXE_LINKER_FLAGS_INIT "--specs=nosys.specs")
