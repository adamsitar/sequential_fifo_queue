set(SANITIZE_CONDITION "$<CONFIG:Debug>")

set(SANITIZE_FLAGS
  "$<${SANITIZE_CONDITION}:-fsanitize=implicit-integer-sign-change>"
  "$<${SANITIZE_CONDITION}:-fsanitize=implicit-integer-conversion>"
  "$<${SANITIZE_CONDITION}:-fsanitize=implicit-unsigned-integer-truncation>"
  "$<${SANITIZE_CONDITION}:-fsanitize=implicit-signed-integer-truncation>"
  # common
  "$<${SANITIZE_CONDITION}:-fsanitize=address>"
  "$<${SANITIZE_CONDITION}:-fsanitize=undefined>"
  "$<${SANITIZE_CONDITION}:-fno-omit-frame-pointer>"
  "$<${SANITIZE_CONDITION}:-g>"
)

add_compile_options(${SANITIZE_FLAGS})
add_link_options(${SANITIZE_FLAGS})
