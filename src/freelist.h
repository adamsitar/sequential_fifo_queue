#include <array>
#include <ranges>
#include <types.h>

namespace mbb {

/**
 * @brief Chain of blocks, each pointing to the next one, in contigous memory
 */
template <std::size_t block_size, std::size_t block_count>
  requires(block_size > 0 && is_power_of_two<block_size> && block_count > 0)
class Freelist : std::array<::Block<block_size>, block_count> {
public:
  using Block = ::Block<block_size>;
  using Base = std::array<Block, block_count>;

private:
  Block *_head{nullptr};
  size_t _count{0};

  void insert(Block &elem) {
    elem.next_free = _head;
    _head = &elem;
    _count++;
  }

public:
  expose_iterators(Base);
  using Base::size;

  bool is_full() { return _count == this->max_size(); }
  bool is_empty() { return _head == nullptr; }

  /**
   * @brief Initialize the freelist, link all elements together
   * @time n
   */
  Freelist() { reset(); }

  /**
   * @brief Re-links elements.
   * Claims ownership of everything in array.
   * @tme n
   */
  void reset() {
    for (auto &current : std::ranges::reverse_view{*this}) {
      insert(current);
    }
  }

  /**
   * @brief Returns head for reading
   * @time 1
   */
  const Block &head() {
    if (_head == nullptr) {
      throw std::bad_alloc();
    }
    return *_head;
  }

  /**
   * @brief Removes head from the list
   * Caller now owns return value
   * @time 1
   *
   * @throws when head is nullptr
   */
  Block *pop() {
    if (_head == nullptr) {
      throw std::bad_alloc();
    }
    auto tmp_head = _head;
    _head = _head->next_free;
    _count--;
    return tmp_head;
  };

  /**
   * @brief Inserts element at head.
   * @time 1
   * @throws bad_alloc -> when list is full & when elem does not belong to list
   */
  void push(Block &elem) {
    if (_count >= this->max_size()) {
      throw std::bad_alloc();
    }

    auto belongs_to_buffer =
        &elem >= this->data() && &elem < this->data() + this->size();
    if (!belongs_to_buffer) {
      throw std::bad_alloc();
    }

    insert(elem);
  }
};

} // namespace mbb
