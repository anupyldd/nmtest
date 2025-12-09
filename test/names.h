#pragma once

#include <filesystem>
#include <format>
#include <source_location>

namespace names
{
    inline auto suite1 = "Suite 1";
    inline auto suite2 = "Suite 2";
    inline auto tag1   = "Tag 1";
    inline auto tag2   = "Tag 2";

    inline auto AddLoc(
        const char* name,
        const std::source_location loc = std::source_location::current()) -> std::string
    {
        auto filename = std::filesystem::path(std::string(loc.file_name())).filename().string();
        return std::format("{} ({}:{})", name, filename, loc.line());
    }
}
