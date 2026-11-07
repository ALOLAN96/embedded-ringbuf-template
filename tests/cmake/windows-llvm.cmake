# Optional Windows/LLVM test toolchain without a Windows SDK.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER "${LLVM_BIN}/clang.exe")
set(CMAKE_AR "${LLVM_BIN}/llvm-ar.exe")
set(CMAKE_C_FLAGS_INIT "-ffreestanding -fno-builtin -fno-stack-protector -I\"${CMAKE_CURRENT_LIST_DIR}/../stubs\"")
set(CMAKE_C_LINK_EXECUTABLE "\"${LLVM_BIN}/lld.exe\" -flavor link /out:<TARGET> /entry:main /subsystem:console /nodefaultlib <OBJECTS> <LINK_LIBRARIES>")
set(COMPONENT_FREESTANDING_TEST ON CACHE BOOL "Use test-only runtime")

