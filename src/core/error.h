#pragma once
#include <cstdint>
#include <string_view>

#define ALL_ERROR_CODES                                                        \
  /* General errors */                                                         \
  X(none, "success")                                                           \
  X(generic, "generic error")                                                  \
  X(out_of_memory, "out of memory")                                            \
  X(invalid_pointer, "invalid pointer")                                        \
  X(upstream_failure, "upstream allocator failure")                            \
  /* Freelist / offset list errors */                                          \
  X(list_full, "list is full")                                                 \
  X(list_empty, "list is empty")                                               \
  /* Segment management errors */                                              \
  X(segment_exhausted, "all segment slots occupied")                           \
  X(invalid_segment, "invalid segment")                                        \
  X(invalid_segment_id, "invalid segment ID")                                  \
  X(invalid_metadatda, "invalid metadata for segment ID")                      \
  X(segment_still_active, "segment still active")                              \
  /* Buffer registration errors */                                             \
  X(null_buffer_ptr, "null buffer pointer")                                    \
  X(buffer_not_registered, "buffer for this tag not registered")               \
  X(buffer_already_registered, "buffer already registered for this tag")       \
  /* Pointer ownership errors */                                               \
  X(not_owned, "pointer not owned")

enum class error : std::uint8_t {
#define X(name, str) name,
  ALL_ERROR_CODES
#undef X
};

constexpr std::string_view to_string(error e) noexcept {
  switch (e) {
#define X(name, str)                                                           \
  case error::name:                                                            \
    return str;
    ALL_ERROR_CODES
#undef X
  }
  return "unknown error";
}
