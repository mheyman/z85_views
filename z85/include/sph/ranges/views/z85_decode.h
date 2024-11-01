#pragma once
#include <array>
#include <format>
#include <optional>
#include <ranges>
#include <stdexcept>

namespace sph::ranges::views
{
    namespace detail
    {
        // Custom transform view that filters bytes and then converts every 4 bytes into 5 bytes.
        template<std::ranges::viewable_range R, typename T>
            requires std::ranges::input_range<R> && std::is_standard_layout_v<T>
        class z85_decode_view : public std::ranges::view_interface<z85_decode_view<R, T>> {
            R input_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        public:
            explicit z85_decode_view(R&& input)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
                : input_(std::forward<R>(input)) {}

            z85_decode_view(z85_decode_view const&) = default;
            z85_decode_view(z85_decode_view&&) = default;
            ~z85_decode_view() noexcept = default;
            auto operator=(z85_decode_view const&) -> z85_decode_view& = default;
            auto operator=(z85_decode_view&&o) noexcept -> z85_decode_view&
            {
                // not sure why "= default" doesn't work here...
                input_ = std::move(std::forward<z85_decode_view>(o).input_);
                return *this;
            }

            struct sentinel;
            class iterator
            {
            public:
                using iterator_category = std::input_iterator_tag;
                using value_type = T;
                using difference_type = std::ptrdiff_t;
                using input_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;
            private:
                std::ranges::iterator_t<R> current_;
                std::ranges::iterator_t<R> end_;
                size_t current_pos_{ 0 };
                std::array<uint8_t, 4> buffer_ = {};
                size_t buffer_pos_{ buffer_.size() };
                value_type value_;
                bool at_end_{ false };
            public:

                iterator(std::ranges::iterator_t<R> begin, std::ranges::iterator_t<R> end)
                    : current_(begin), end_(end)
                {
                    load_next_value();
                }

                auto operator++(int) -> iterator&
                {
                    auto ret{ *this };
                    load_next_value();
                    return ret;
                }

                auto operator++() -> iterator&
                {
                    load_next_value();
                    return *this;
                }

                auto equals(const iterator& i) const -> bool
                {
                    return current_ == i.current_ && current_pos_ == i.current_pos_ && buffer_pos_ == i.buffer_pos_;
                }

            	auto equals(const sentinel&) const -> bool
                {
                    return at_end_;
                }

                auto operator*() const -> value_type
                {
	                return value_;
                }

                auto operator==(const iterator& other) const -> bool { return equals(other); }
                auto operator==(const sentinel&s) const -> bool { return equals(s); }
                auto operator!=(const iterator& other) const -> bool { return !equals(other); }
                auto operator!=(const sentinel&s) const -> bool { return !equals(s); }

            private:
                void load_next_value()
                {
                    std::span<uint8_t> vs{ reinterpret_cast<uint8_t*>(&value_), sizeof(value_type) };
                    for (auto t : std::views::enumerate(vs))
                    {
                        auto [v_count, v] {t};
                        if (buffer_pos_ >= buffer_.size())
                        {
                            load_next_chunk();
                            if (buffer_pos_ != 0)
                            {
                                if (v_count > 0)
                                {
                                    throw std::runtime_error(std::format("Partial type at end of data. Required {} bytes, received {}.", sizeof(value_type), v_count));
                                }

                                at_end_ = true;
                                return;
                            }
                        }

                        v = buffer_[buffer_pos_];
                        ++buffer_pos_;
                    }
                }
                void load_next_chunk()
                {
	                if (std::optional<std::array<char, 5>> const chunk{ next_value() })
                    {
                        static std::array<unsigned char, 96> constexpr base256{ {
                            0x00, 0x44, 0x00, 0x54, 0x53, 0x52, 0x48, 0x00,
                            0x4B, 0x4C, 0x46, 0x41, 0x00, 0x3F, 0x3E, 0x45,
                            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                            0x08, 0x09, 0x40, 0x00, 0x49, 0x42, 0x4A, 0x47,
                            0x51, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
                            0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
                            0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
                            0x3B, 0x3C, 0x3D, 0x4D, 0x00, 0x4E, 0x43, 0x00,
                            0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                            0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                            0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
                            0x21, 0x22, 0x23, 0x4F, 0x00, 0x50, 0x00, 0x00
                        } };
                        
                        uint32_t value{ base256[(static_cast<size_t>((*chunk)[0]) - 32) & 127]};
                        value = (value * 85) + base256[(static_cast<size_t>((*chunk)[1]) - 32) & 127];
                        value = (value * 85) + base256[(static_cast<size_t>((*chunk)[2]) - 32) & 127];
                        value = (value * 85) + base256[(static_cast<size_t>((*chunk)[3]) - 32) & 127];
                        value = (value * 85) + base256[(static_cast<size_t>((*chunk)[4]) - 32) & 127];
                        buffer_[0] = static_cast<unsigned char>(value >> 24);
                        buffer_[1] = static_cast<unsigned char>(value >> 16);
                        buffer_[2] = static_cast<unsigned char>(value >> 8);
                        buffer_[3] = static_cast<unsigned char>(value);
                        buffer_pos_ = 0;
                    }
                }

                static auto is_okay(uint8_t c, size_t i) -> bool
                {
                    // filter invalid z85 characters - this allows splitting strings with newlines or any other odd thing.
                    std::array<uint8_t, 256> constexpr valid{ {
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x1f, 0x00, 0x1e, 0x1e, 0x1f, 0x1f, 0x00, 0x1f, 0x1f, 0x1f, 0x1f, 0x00, 0x1f, 0x1f, 0x1f,
                            0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x00, 0x1f, 0x1f, 0x1f, 0x1f,
                            0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
                            0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x00, 0x1f, 0x1f, 0x00,
                            0x00, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
                            0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x00, 0x1f, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                        } };
                    uint8_t const mask{ static_cast<uint8_t>(1 << i) };
                    return (valid[c] & mask) == mask;
                }

                auto next_value() -> std::optional<std::array<char, 5>>
                {
                    if (current_ == end_)
                    {
                        return std::optional<std::array<char, 5>>{};
                    }

                    std::array<char, 5> ret;
                    size_t i{ 0 };
                    while(true)
                    {
                        uint8_t const c{ reinterpret_cast<uint8_t const*>(&*current_)[current_pos_++] };
                        if (is_okay(c, i))
                        {
                            ret[i++] = static_cast<char>(c);
                            if (i == ret.size())
                            {
                                if (current_pos_ == sizeof(input_type))
                                {
                                    ++current_;
                                    current_pos_ = 0;
                                }

                                return ret;
                            }
                        }

                        if (current_pos_ == sizeof(input_type))
                        {
                            ++current_;
                            if (current_ == end_)
                            {
                                if (i == 0)
                                {
                                    // filtered out last byte(s).
                                    return std::optional<std::array<char, 5>>{};
                                }

                                throw std::runtime_error("z85_decode requires input to be a multiple of 5 characters");
                            }

                            current_pos_ = 0;
                        }
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

            iterator begin() { return iterator(std::ranges::begin(input_), std::ranges::end(input_)); }

            sentinel end() { return sentinel{}; }
        };

        struct z85_decode_tag
        {
	        
        };

        template<std::ranges::viewable_range R, typename T = uint8_t>
        z85_decode_view(R&&) -> z85_decode_view<R, T>;

        template <typename T>
        struct z85_decode_fn : std::ranges::range_adaptor_closure<z85_decode_fn<T>>
        {
            template <std::ranges::viewable_range R>
            [[nodiscard]] constexpr auto operator()(R&& range) const -> z85_decode_view<R, T>
            {
                return z85_decode_view<R, T>(std::forward<R>(range));
            }
        };
    }

}

namespace sph::views
{
    template<typename T = uint8_t>
    auto z85_decode(detail::z85_decode_tag = {}) -> sph::ranges::views::detail::z85_decode_fn<T>
    {
        return {};
    }
}