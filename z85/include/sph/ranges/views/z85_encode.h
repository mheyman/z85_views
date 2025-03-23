#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <fmt/format.h>

namespace sph::ranges::views
{
    namespace detail
    {
        /**
         * @brief A view that encodes binary data into Z85-encoded data into by converting every 4 bytes into 5 characters.
         * @tparam R The input range type
         */
        template<std::ranges::viewable_range R>
            requires std::ranges::input_range<R> && std::is_standard_layout_v<std::ranges::range_value_t<R>>
        class z85_encode_view : public std::ranges::view_interface<z85_encode_view<R>> 
        {
            R input_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        public:
            explicit z85_encode_view(R&& input)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
                : input_(std::forward<R>(input))
            {
                if (std::ranges::empty(input_))
                {
                    return;
                }

                if (std::ranges::size(input_) % 4 != 0)
                {
                    throw std::invalid_argument(
                        fmt::format("Z85 encode requires input size to be multiple of 4, got {}",
                            std::ranges::size(input_))
                    );
                }
            }
            z85_encode_view(z85_encode_view const&) = default;
            z85_encode_view(z85_encode_view&&) = default;
            ~z85_encode_view() noexcept = default;
            auto operator=(z85_encode_view const&)->z85_encode_view & = default;
            auto operator=(z85_encode_view&& o) noexcept -> z85_encode_view &
            {
                // not sure why "= default" doesn't work here...
                if (this != &o)
                {
                    input_ = std::move(o.input_);
                }

                return *this;
            }


            struct sentinel;
            class iterator
            {
            public:
                using iterator_category = std::input_iterator_tag;
                using value_type = char;
                using difference_type = std::ptrdiff_t;
                using input_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;
            private:
                static std::string_view constexpr base85{
					"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#"
                };
                std::ranges::const_iterator_t<R> current_;
                std::ranges::const_sentinel_t<R> end_;
                size_t buffer_pos_;
                std::array<char, 5> buffer_ = {};

                // only need to copy current value if sizeof(input_type) > 1
                struct empty {};
                using current_value_t = std::conditional_t<sizeof(input_type) == 1, empty, input_type>;
                [[no_unique_address]] current_value_t current_value_;
                using current_value_pos_t = std::conditional_t<sizeof(input_type) == 1, empty, size_t>;
                [[no_unique_address]] current_value_pos_t current_value_pos_;
            public:
                iterator(std::ranges::const_iterator_t<R> begin, std::ranges::const_sentinel_t<R> end)
					: current_{ begin }, end_{ end }, buffer_pos_{ 0 }, current_value_pos_{ init_current_value_pos() }
	            {
                    load_next_chunk();
                }

            	iterator() : current_{ std::ranges::sentinel_t<R>{}}, end_{}, buffer_pos_{buffer_.size()} {}
                iterator(iterator const&) = default;
                iterator(iterator &&) = default;
                ~iterator() = default;
                iterator &operator=(iterator const&) = default;
                iterator &operator=(iterator&&) = default;

                auto equals(const iterator& i) const noexcept -> bool
                {
                    if constexpr (sizeof(input_type) == 1)
                    {
                        return current_ == i.current_ && buffer_pos_ == i.buffer_pos_;
                    }
                    else
                    {
                        return current_ == i.current_ && current_value_pos_ == i.current_value_pos_ && buffer_pos_ == i.buffer_pos_;
                    }
                }

                auto equals(const sentinel&) const noexcept -> bool
                {
                    return current_ == end_ && buffer_pos_ == buffer_.size();
                }

                auto operator++() -> iterator&
                {
                    ++buffer_pos_;
                    if (buffer_pos_ >= buffer_.size())
                    {
                        load_next_chunk();
                    }

                    return *this;
                }

                auto operator++(int) -> iterator
                {
                    auto ret{ *this };
                    ++buffer_pos_;
                    if (buffer_pos_ >= buffer_.size())
                    {
                        load_next_chunk();
                    }

                    return ret;
                }


                value_type operator*() const { return buffer_[buffer_pos_]; }

                auto operator==(const iterator& other) const noexcept -> bool { return equals(other); }
                auto operator==(const sentinel& s) const noexcept -> bool { return equals(s); }
                auto operator!=(const iterator& other) const noexcept -> bool { return !equals(other); }
                auto operator!=(const sentinel& s) const noexcept -> bool { return !equals(s); }

            private:
                static constexpr auto init_current_value_pos() -> current_value_pos_t
                {
                    if constexpr (sizeof(input_type) == 1)
                    {
                        return empty{};
                    }
                    else
                    {
                        return size_t{ sizeof(input_type) };
                    }
                }

                static constexpr auto div85(uint32_t v) -> uint32_t
                {
                    static uint64_t constexpr div85_magic{ 3233857729ULL };
                    return static_cast<uint32_t>((div85_magic * v) >> 38);
                }

                void load_next_chunk()
            	{
                    if (current_ != end_ || !at_end_of_input_value())
                    {
	                    uint32_t value{ next_value() };

	                    // convert uint32_t elements into 5-char elements
	                    uint32_t value2 = div85(value);
	                    buffer_[4] = base85[value - (value2 * 85)];
	                    value = value2;
	                    value2 = div85(value);
	                    buffer_[3] = base85[value - (value2 * 85)];
	                    value = value2;
	                    value2 = div85(value);
	                    buffer_[2] = base85[value - (value2 * 85)];
	                    value = value2;
	                    value2 = div85(value);
	                    buffer_[1] = base85[value - (value2 * 85)];
	                    buffer_[0] = base85[value2];
	                    buffer_pos_ = 0;
                    }
                }

                auto next_value() -> uint32_t
                {
                    uint32_t value{};
                    std::ranges::for_each(std::array<size_t, 4>{{24, 16, 8, 0}}, [this, &value](size_t shift) {
                        auto const v{next_byte()};
                        value |= v << shift;
                        if (shift > 0 && at_end_of_input_value() && current_ == end_)
                        {
                            throw std::runtime_error(
                                fmt::format("Z85 encoding error: Input length {} is not a multiple of 4 bytes",
                                    std::distance(std::ranges::begin(input_), std::ranges::end(input_)))
                            );
                        }
                    });
                    return value;
                }

                /**
                 * \brief Gets the next byte (as a uint32_t) and a value indicating whether the byte is the last byte of the current input value.
                 * \return The next byte (as a uint32_t) and a value indicating whether the byte is the last byte of the current input value.
                 */
                auto constexpr next_byte() -> uint32_t
                {
                    if constexpr (sizeof(input_type) == 1)
                    {
                        return static_cast<uint32_t>(*current_++);
                    }
                    else
                    {
                        if (current_value_pos_ == sizeof(input_type))
                        {
                            current_value_ = *current_++;
                            current_value_pos_ = 0;
                        }

                        uint32_t const ret{ static_cast<uint32_t>(reinterpret_cast<uint8_t const*>(&current_value_)[current_value_pos_]) };
                        ++current_value_pos_;
                        return ret;
                    }
                }

                auto at_end_of_input_value() -> bool
                {
                    if constexpr (sizeof(input_type) == 1)
                    {
                        return true;
                    }
                    else
                    {
                        return current_value_pos_ == sizeof(input_type);
                    }
                }
            };

            struct sentinel
            {
                auto operator==(const sentinel& other) const -> bool { return true; }
                auto operator==(const iterator& i) const -> bool { return i.equals(*this); }
                auto operator!=(const sentinel& other) const -> bool { return false; }
                auto operator!=(const iterator& i) const -> bool { return !i.equals(*this); }
            };

            iterator begin() const { return iterator(std::ranges::begin(input_), std::ranges::end(input_)); }
            sentinel end() const { return sentinel{}; }
        };

        template<std::ranges::viewable_range R>
        z85_encode_view(R&&) -> z85_encode_view<std::views::all_t<R>>;

        struct z85_encode_fn : std::ranges::range_adaptor_closure<z85_encode_fn>
        {
            template <std::ranges::viewable_range R>
            [[nodiscard]] constexpr auto operator()(R&& range) const -> z85_encode_view<std::views::all_t<R>>
            {
                return z85_encode_view(std::views::all(std::forward<R>(range)));
            }
        };
    }
}

namespace sph::views
{
	/**
	 * @brief A view adaptor that encodes binary data into Z85-encoded data by converting every 4 bytes into 5 characters.
	 */
    inline auto z85_encode() -> sph::ranges::views::detail::z85_encode_fn
    {
        return {};
    }
}