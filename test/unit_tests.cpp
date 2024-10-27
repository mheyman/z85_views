#include <doctest/doctest.h>
#include <array>
#include <sph/ranges/views/z85_encode.h>
#include <sph/ranges/views/z85_decode.h>
#include <vector>
#include <fmt/format.h>
#include <fmt/ranges.h>

TEST_CASE("z85.hello_world")
{
	std::array<unsigned char, 8> hello_data{ {0x86, 0x4F, 0xD2, 0x6F, 0xB5, 0x59, 0xF7, 0x5B} };
	std::string encoded_str;
	std::ranges::copy(hello_data | sph::ranges::views::z85_encode, std::back_inserter(encoded_str));

	auto decoded_view{ encoded_str | sph::ranges::views::z85_decode };
	//auto decoded_view1{ std::views::all(encoded_str | sph::ranges::views::z85_decode) };
	auto all_decoded_view{ decoded_view | std::views::all };


	CHECK_EQ(encoded_str, "HelloWorld");
	auto decoded{ std::string{"Hello World\n"} | sph::ranges::views::z85_decode | std::ranges::to<std::vector>() };
	size_t count{0};
	std::ranges::for_each(std::views::zip(hello_data, all_decoded_view), [&count](auto&& t)
		{
			auto [gt, rt] {t};
			CHECK_EQ(gt, rt);
			++count;
		});
	CHECK_EQ(count, hello_data.size());

	fmt::print("z85.hello_world: {}\n", encoded_str);
	fmt::print("z85.hello_world: 0x{:02X}\n", fmt::join(encoded_str | sph::ranges::views::z85_decode, ", 0x"));
}

TEST_CASE("z85.size_5")
{
	std::array<char, 5> constexpr h{ {'h', 'e', 'l', 'l', 'o'} };
	std::array<char, 5> constexpr w{{'w', 'o', 'r', 'l', 'd'}};
	std::array< std::array<char, 5>, 4> constexpr a{{ h, w, h, w}};
	auto const s1{ a | std::views::join | sph::ranges::views::z85_encode | std::ranges::to<std::vector>() };
	auto const s5{ a | sph::ranges::views::z85_encode | std::ranges::to<std::vector>()};
	CHECK_EQ(s1.size(), s5.size());
	std::ranges::for_each(std::views::zip(s1, s5), [](auto&& t)
		{
			auto [v1, v5] {t};
			CHECK_EQ(v1, v5);
		});
}
