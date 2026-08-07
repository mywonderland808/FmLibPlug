#pragma once

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <string>

inline std::filesystem::path fixturesDir()
{
#ifdef FMLIBPLUG_FIXTURES_DIR
    return std::filesystem::path (FMLIBPLUG_FIXTURES_DIR);
#else
    // Fallback: relative to cwd when running from build/
    auto candidates = {
        std::filesystem::path ("Tests/fixtures"),
        std::filesystem::path ("../Tests/fixtures"),
        std::filesystem::path ("../../Tests/fixtures")
    };
    for (const auto& c : candidates)
        if (std::filesystem::exists (c / "single_163.syx"))
            return c;
    return std::filesystem::path ("Tests/fixtures");
#endif
}

inline std::filesystem::path fixturePath (const std::string& name)
{
    return fixturesDir() / name;
}
