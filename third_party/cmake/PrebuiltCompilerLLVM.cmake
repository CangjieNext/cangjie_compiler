# Compiler-only build. Callers must pin both the LLVM source and build inputs.
if(NOT CANGJIE_CJNATIVE_SOURCE_DIR OR NOT EXISTS "${CANGJIE_CJNATIVE_SOURCE_DIR}/llvm/include/llvm/IR/Module.h")
    message(FATAL_ERROR "A matching LLVM source directory is required")
endif()
if(NOT EXISTS "${CANGJIE_PREBUILT_LLVM_BUILD_DIR}/include/llvm/Config/llvm-config.h")
    message(FATAL_ERROR "A configured, target-matched LLVM build directory is required")
endif()
set(LLVM_LIBS)
foreach(LIB_NAME ${LLVM_LIB_NAMES})
    set(LIB_PATH "${CANGJIE_PREBUILT_LLVM_BUILD_DIR}/lib/${LIB_NAME}")
    if(NOT EXISTS "${LIB_PATH}")
        message(FATAL_ERROR "Missing LLVM component: ${LIB_PATH}")
    endif()
    list(APPEND LLVM_LIBS "${LIB_PATH}")
endforeach()
list(APPEND LLVM_LIBS ${LLVM_LINK_FLAGS})
set(LLVM_LIBS ${LLVM_LIBS} PARENT_SCOPE)
set(LLVM_INCLUDE_DIRS
    "${CANGJIE_CJNATIVE_SOURCE_DIR}/llvm/include"
    "${CANGJIE_PREBUILT_LLVM_BUILD_DIR}/include"
    PARENT_SCOPE)
add_custom_target(cjnative)
set(BINARY_TARGET_DEPENDS cjnative)
