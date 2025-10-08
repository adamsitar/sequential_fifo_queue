
#pragma once
#include <memory_resource>

/**
 * @defgroup managed_resource managed_resource
 * @brief Abstract interface providing additional control methods
 */

/**
 * @defgroup managed_resource_api API
 * @ingroup managed_resource
 */

/**
 * @defgroup managed_resource_test Tests
 * @ingroup managed_resource
 */

/// @addtogroup managed_resource_api
/// @{

/**
 * @class managed_resource
 * @brief Abstract base class for managed memory resources.
 *
 * Extends @c std::pmr::memory_resource with additional
 * management methods for controlling memory pools or caches.
 * Applications subclass this to implement allocators that retain ownership
 * over allocated memory blocks and may release or reset them collectively.
 *
 * @note This class is non-copyable and non-movable.
 */
class Managed : public std::pmr::memory_resource
{
public:
  Managed() = default;
  ~Managed() override = default;
  Managed(const Managed&) = delete;
  Managed& operator=(const Managed&) = delete;
  Managed(Managed&&) = delete;
  Managed& operator=(Managed&&) = delete;

  /**
   * @brief Release or free all internally allocated memory.
   *
   * After this call, the resource should free any pooled or cached
   * memory it holds and reset any internal state.
   *
   * This does not affect upstream memory resources.
   */
  virtual void reset() = 0;

  /**
   * @brief Get the total size (in bytes) of memory currently managed.
   * This value may represent all memory owned by the resource including
   * pooled or cached blocks.
   *
   * @return The total size in bytes of allocated or retained memory.
   */
  [[nodiscard]] virtual std::size_t size() const noexcept = 0;
};

/// @} end group
