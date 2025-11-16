set(SANITIZE_CONDITION "$<CONFIG:Debug>")
set(IS_CLANG "$<CXX_COMPILER_ID:Clang,AppleClang>")
set(CLANG_AND_DEBUG "$<AND:${SANITIZE_CONDITION},${IS_CLANG}>")

# Clang-only sanitizers (implicit conversion checks)
set(CLANG_SANITIZE_FLAGS
  "$<${CLANG_AND_DEBUG}:-fsanitize=implicit-integer-sign-change>"
  "$<${CLANG_AND_DEBUG}:-fsanitize=implicit-integer-conversion>"
  "$<${CLANG_AND_DEBUG}:-fsanitize=implicit-unsigned-integer-truncation>"
  "$<${CLANG_AND_DEBUG}:-fsanitize=implicit-signed-integer-truncation>"
)

# Common sanitizers (supported by both Clang and GCC)
set(COMMON_SANITIZE_FLAGS
  "$<${SANITIZE_CONDITION}:-fsanitize=address>"
  "$<${SANITIZE_CONDITION}:-fsanitize=undefined>"
  "$<${SANITIZE_CONDITION}:-fno-omit-frame-pointer>"
  "$<${SANITIZE_CONDITION}:-g>"
)

add_compile_options(${CLANG_SANITIZE_FLAGS} ${COMMON_SANITIZE_FLAGS})
add_link_options(${CLANG_SANITIZE_FLAGS} ${COMMON_SANITIZE_FLAGS})
