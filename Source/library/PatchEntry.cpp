#include "library/PatchEntry.h"
#include "library/BrowserList.h"
#include "util/StringUtils.h"

namespace fmlib
{

void PatchEntry::refreshSearchCache()
{
    voiceNameLower = asciiLower (voiceName);
    fileNameLower = asciiLower (fileName);
    relativePathLower = asciiLower (relativePath);
    nameSortKey = BrowserList::nameSortKey (voiceName);
}

} // namespace fmlib
