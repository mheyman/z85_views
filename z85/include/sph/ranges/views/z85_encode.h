#pragma once
#include <algorithm>
#include <array>
#include <ranges>
#include <stdexcept>

namespace sph::ranges::views
{
    namespace detail
    {
        // Custom transform view that filters bytes and then converts every 4 bytes into 5 bytes.
        template<std::ranges::viewable_range R>
            requires std::ranges::input_range<R> && std::is_standard_layout_v<std::ranges::range_value_t<R>>
        class z85_encode_view : public std::ranges::view_interface<z85_encode_view<R>> {
            R input_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        public:
            explicit z85_encode_view(R&& input)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
                : input_(std::forward<R>(input)) {}
            z85_encode_view(z85_encode_view const&) = default;
            z85_encode_view(z85_encode_view&&) = default;
            ~z85_encode_view() noexcept = default;
            auto operator=(z85_encode_view const&)->z85_encode_view & = default;
            auto operator=(z85_encode_view&& o) noexcept -> z85_encode_view &
            {
                // not sure why "= default" doesn't work here...
                input_ = std::move(std::forward<z85_encode_view>(o).input_);
                return *this;
            }


            struct sentinel;
            class iterator
            {
                std::ranges::iterator_t<R> current_;
                std::ranges::iterator_t<R> end_;
                size_t current_pos_{ 0 };
                size_t buffer_pos_;
                std::array<char, 5> buffer_ = {};
            public:
                using iterator_category = std::input_iterator_tag;
                using value_type = char;
                using difference_type = std::ptrdiff_t;
                using input_type = std::remove_cvref_t<std::ranges::range_value_t<R>>;

                iterator(std::ranges::iterator_t<R> begin, std::ranges::iterator_t<R> end)
                    : current_(begin), end_(end), buffer_pos_(0)
                {
                    load_next_chunk();
                }

                auto equals(const iterator& i) const -> bool
                {
                    return current_ == i.current_ && current_pos_ == i.current_pos_ && buffer_pos_ == i.buffer_pos_;
                }

                auto equals(const sentinel&) const -> bool
                {
                    return current_ == end_ && buffer_pos_ == buffer_.size();
                }

                iterator& operator++()
                {
                    ++buffer_pos_;
                    if (buffer_pos_ >= buffer_.size())
                    {
                        load_next_chunk();
                    }

                    return *this;
                }

                iterator& operator++(int)
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

                auto operator==(const iterator& other) const -> bool { return equals(other); }
                auto operator==(const sentinel& s) const -> bool { return equals(s); }
                auto operator!=(const iterator& other) const -> bool { return !equals(other); }
                auto operator!=(const sentinel& s) const -> bool { return !equals(s); }

            private:
                void load_next_chunk()
            	{
                    if (current_ != end_)
                    {
                        uint32_t value{ next_value() };

                        // convert uint32_t elements into 5-char elements
                        static auto constexpr div85 = [](uint32_t v) -> uint32_t
                    	{
                        	static uint64_t constexpr div85_magic_{ 3233857729ULL };
                            return (static_cast<uint32_t>((div85_magic_ * v) >> 32) >> 6);
                        };
                        static std::array<char, 85> constexpr base85
                        { {
                            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                            'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                            'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
                            'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D',
                            'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
                            'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                            'Y', 'Z', '.', '-', ':', '+', '=', '^', '!', '/',
                            '*', '?', '&', '<', '>', '(', ')', '[', ']', '{',
                            '}', '@', '%', '$', '#'
                        } };
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
                    uint32_t value{ static_cast<uint32_t>(reinterpret_cast<uint8_t const*>(&*current_)[current_pos_++]) << 24 };
                    if (current_pos_ == sizeof(input_type))
                    {
                        current_pos_ = 0;
                        ++current_;
                        if (current_ == end_)
                        {
                            throw std::runtime_error("z85_encode requires input to be a multiple of 4 bytes");
                        }
                    }
                    value |= static_cast<uint32_t>(reinterpret_cast<uint8_t const*>(&*current_)[current_pos_++]) << 16;
                    if (current_pos_ == sizeof(input_type))
                    {
                        current_pos_ = 0;
                        ++current_;
                        if (current_ == end_)
                        {
                            throw std::runtime_error("z85_encode requires input to be a multiple of 4 bytes");
                        }
                    }
                    value |= static_cast<uint32_t>(reinterpret_cast<uint8_t const*>(&*current_)[current_pos_++]) << 8;
                    if (current_pos_ == sizeof(input_type))
                    {
                        current_pos_ = 0;
                        ++current_;
                        if (current_ == end_)
                        {
                            throw std::runtime_error("z85_encode requires input to be a multiple of 4 bytes");
                        }
                    }
                    value |= static_cast<uint32_t>(reinterpret_cast<uint8_t const*>(&*current_)[current_pos_++]);
                    if (current_pos_ == sizeof(input_type))
                    {
                        current_pos_ = 0;
                        ++current_;
                    }

                    return value;
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
        z85_encode_view(R&&) -> z85_encode_view<R>;

        struct z85_encode_fn : std::ranges::range_adaptor_closure<z85_encode_fn>
        {
            template <std::ranges::viewable_range R>
            [[nodiscard]] constexpr auto operator()(R&& range) const -> z85_encode_view<R>
            {
                return z85_encode_view(std::forward<R>(range));
            }
        };
    }

    inline auto z85_encode() -> detail::z85_encode_fn
    {
        return {};
    }

}