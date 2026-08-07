#include "library/PatchEntry.h"
#include "util/StringUtils.h"

namespace fmlib
{

void PatchEntry::refreshSearchCache()
{
    voiceNameLower = asciiLower (voiceName);
    fileNameLower = asciiLower (fileName);
    relativePathLower = asciiLower (relativePath);
}

} // namespace fmlib
