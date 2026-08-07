#include "library/PatchEntry.h"
#include <algorithm>
#include <cctype>

namespace fmlib
{

void PatchEntry::refreshSearchCache()
{
    auto lower = [] (std::string s)
    {
        std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
        return s;
    };
    voiceNameLower = lower (voiceName);
    fileNameLower = lower (fileName);
    relativePathLower = lower (relativePath);
}

} // namespace fmlib
