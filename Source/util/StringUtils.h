#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#if __has_include(<juce_core/juce_core.h>)
#include <juce_core/juce_core.h>
#define FMLIB_HAS_JUCE_STRING 1
#else
#define FMLIB_HAS_JUCE_STRING 0
#endif

namespace fmlib
{

inline std::string asciiLower (std::string s)
{
    std::transform (s.begin(), s.end(), s.begin(),
                    [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    return s;
}

inline void asciiLowerInPlace (std::string& s)
{
    std::transform (s.begin(), s.end(), s.begin(),
                    [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
}

#if FMLIB_HAS_JUCE_STRING
inline juce::String toJuce (std::string_view s)
{
    return juce::String::fromUTF8 (s.data(), static_cast<int> (s.size()));
}

inline std::string fromJuce (const juce::String& s)
{
    return s.toStdString();
}
#endif

} // namespace fmlib
