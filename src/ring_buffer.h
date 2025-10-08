#include <array>
#include <bit>
#include <cstdint>
#include <iterator>
#include <type_traits> // For std::remove_const_t
#include <types.h>

template<class T, std::size_t count>
class RingBuffer
{
  struct Header
  {
    SmallestType<count> head{ 0 }; // increment to remove
    SmallestType<count> tail{ 0 }; // increment to insert
    SmallestType<count> free{ count };
  } _header;
  static constexpr auto _header_size = sizeof(Header);

  std::array<T, count> _storage{};

public:
  using value_type = T;
  using size_type = SmallestType<count>;
  static constexpr auto storage_size = count;

  class iterator
  {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    constexpr iterator() noexcept = default;
    constexpr iterator(RingBuffer* buffer, size_type pos) noexcept
      : _buffer(buffer)
      , _pos(pos)
    {
    }

    constexpr reference operator*() const noexcept
    {
      return _buffer->_storage[_pos];
    }

    constexpr pointer operator->() const noexcept
    {
      return &(_buffer->_storage[_pos]);
    }

    constexpr iterator& operator++() noexcept
    {
      _pos = (_pos + 1) % count;
      return *this;
    }

    constexpr iterator operator++(int) noexcept
    {
      iterator temp = *this;
      ++(*this);
      return temp;
    }

    constexpr iterator& operator--() noexcept
    {
      _pos = (_pos == 0) ? (count - 1) : (_pos - 1);
      return *this;
    }

    constexpr iterator operator--(int) noexcept
    {
      iterator temp = *this;
      --(*this);
      return temp;
    }

    constexpr iterator& operator+=(difference_type n) noexcept
    {
      auto new_pos = (static_cast<difference_type>(_pos) + n);
      new_pos %= static_cast<difference_type>(count);
      if (new_pos < 0) {
        new_pos += count;
      }
      _pos = static_cast<size_type>(new_pos);
      return *this;
    }

    constexpr iterator operator+(difference_type n) const noexcept
    {
      iterator temp = *this;
      return temp += n;
    }

    friend constexpr iterator operator+(difference_type n,
                                        const iterator& it) noexcept
    {
      return it + n;
    }

    constexpr iterator& operator-=(difference_type n) noexcept
    {
      return *this += -n;
    }

    constexpr iterator operator-(difference_type n) const noexcept
    {
      iterator temp = *this;
      return temp -= n;
    }

    constexpr difference_type operator-(const iterator& other) const noexcept
    {
      return rank() - other.rank();
    }

    constexpr reference operator[](difference_type n) const noexcept
    {
      return *(*this + n);
    }

    constexpr bool operator==(const iterator& other) const noexcept
    {
      return _pos == other._pos && _buffer == other._buffer;
    }
    constexpr bool operator!=(const iterator& other) const noexcept
    {
      return !(*this == other);
    }
    constexpr bool operator<(const iterator& other) const noexcept
    {
      return rank() < other.rank();
    }
    constexpr bool operator>(const iterator& other) const noexcept
    {
      return rank() > other.rank();
    }
    constexpr bool operator<=(const iterator& other) const noexcept
    {
      return rank() <= other.rank();
    }
    constexpr bool operator>=(const iterator& other) const noexcept
    {
      return rank() >= other.rank();
    }

  private:
    constexpr difference_type rank() const noexcept
    {
      if (_buffer == nullptr)
        return 0;
      return (_pos - _buffer->_header.head + count) % count;
    }

    RingBuffer* _buffer = nullptr;
    size_type _pos = 0;
  };

  using const_iterator = std::const_iterator<iterator>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::const_iterator<reverse_iterator>;

  constexpr iterator begin() noexcept { return iterator(this, _header.head); }
  constexpr iterator end() noexcept { return iterator(this, _header.tail); }
  constexpr const_iterator cbegin() const noexcept { return begin(); }
  constexpr const_iterator cend() const noexcept { return end(); }
  constexpr reverse_iterator rbegin() noexcept
  {
    return reverse_iterator(end());
  }
  constexpr reverse_iterator rend() noexcept
  {
    return reverse_iterator(begin());
  }
  constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
  constexpr const_reverse_iterator crend() const noexcept { return rend(); }

  void push(T value); // incremens tail
  T& pop();           // increments head
  void print();
  [[nodiscard]] bool is_full() const { return _header.free == 0; }
  [[nodiscard]] bool is_empty() const { return _header.free == count; }
  [[nodiscard]] size_type get_free() const;
};
