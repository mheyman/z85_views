# z85_stream
z85 encoding via a range adapter.

Peforms encoding according to https://rfc.zeromq.org/spec/32/

Decoding skips invalid characters, however, which allows spaces and newlines within the text.

# Using

```cpp
#include <iostream>
#include <ranges>
#include <sph/ranges/views/z85_encode.h>
#include <sph/ranges/views/z85_decode.h>

int main()
{
    std::array<unsigned char, 8> hello_data{ {0x86, 0x4F, 0xD2, 0x6F, 0xB5, 0x59, 0xF7, 0x5B} };
    std::string encoded_str;
    std::ranges::copy(hello_data | sph::ranges::views::z85_encode, std::back_inserter(encoded_str));
    std::cout << encoded_str << '\n'; // "HelloWorld"_
    
    # example that skips invalid characters during decode
    auto decoded{ std::string{"Hello World\n" | sph::ranges::views::z85_decode | std::ranges::to<std::vector>() };
    // decoded = {0x86, 0x4F, 0xD2, 0x6F, 0xB5, 0x59, 0xF7, 0x5B}
}

```

The elements of the input stream can be odd sizes as long as the total bytes is a multiple of 4.

```cpp
    std::array<char, 5> constexpr h{ {'h', 'e', 'l', 'l', 'o'} };
    std::array<char, 5> constexpr w{{'w', 'o', 'r', 'l', 'd'}};
    std::array< std::array<char, 5>, 4> constexpr a{{ h, w, h, w}};
    // encode range of 4 5-byte elements
    auto const encoded{ a | sph::ranges::views::z85_encode | std::ranges::to<std::vector>()};
    // --> "xK#0@z*cbuy?di<y&13lz/PV8"
```

# Building

```sh
cmake -B build --preset={preset}
cmake --install build 
```
Where `{preset}` is one of
 * clang-debug
 * clang-debug-develop
 * clang-release
 * clang-release-develop
 * clang-test-debug
 * clang-test-release
 * clang-test-release
 * gcc-debug
 * gcc-debug-develop
 * gcc-release
 * gcc-release-develop
 * gcc-test-debug
 * gcc-test-release
 * clang-win-debug
 * clang-win-debug-develop
 * clang-win-release
 * clang-win-release-develop
 * clang-win-test-debug
 * clang-win-test-release
 * msvc-debug
 * msvc-debug-develop
 * msvc-release
 * msvc-release-develop
 * msvc-test-debug-develop
 * msvc-test-release-develop

The develop versions built the tests and include sanitizers. The test version 
also set up CMake testing. All these options are more aspirational then 
operational.