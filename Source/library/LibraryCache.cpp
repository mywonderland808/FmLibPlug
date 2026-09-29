#include "library/LibraryCache.h"
#include <fstream>
#include <system_error>

namespace fmlib
{

namespace
{
template <typename T>
bool writePod (std::ostream& out, const T& v)
{
    out.write (reinterpret_cast<const char*> (&v), static_cast<std::streamsize> (sizeof (T)));
    return static_cast<bool> (out);
}

template <typename T>
bool readPod (std::istream& in, T& v)
{
    in.read (reinterpret_cast<char*> (&v), static_cast<std::streamsize> (sizeof (T)));
    return static_cast<bool> (in);
}

bool writeString (std::ostream& out, const std::string& s)
{
    const auto n = static_cast<std::uint32_t> (s.size());
    if (! writePod (out, n))
        return false;
    if (n == 0)
        return true;
    out.write (s.data(), static_cast<std::streamsize> (n));
    return static_cast<bool> (out);
}

bool readString (std::istream& in, std::string& s)
{
    std::uint32_t n = 0;
    if (! readPod (in, n) || n > 1u << 20)
        return false;
    s.resize (n);
    if (n == 0)
        return true;
    in.read (s.data(), static_cast<std::streamsize> (n));
    return static_cast<bool> (in);
}

std::string pathKey (const std::filesystem::path& p)
{
    return p.generic_string();
}

bool pathUnderBase (const std::filesystem::path& abs, const std::filesystem::path& base)
{
    std::error_code ec;
    const auto rel = std::filesystem::relative (abs, base, ec);
    if (ec || rel.empty())
        return abs == base;
    for (const auto& part : rel)
        if (part == "..")
            return false;
    return true;
}
} // namespace

void LibraryCache::clear()
{
    byPath.clear();
}

LibraryFileFingerprint LibraryCache::fingerprintOf (const std::filesystem::path& file)
{
    LibraryFileFingerprint fp;
    std::error_code ec;
    const auto sz = std::filesystem::file_size (file, ec);
    if (! ec)
        fp.sizeBytes = static_cast<std::uint64_t> (sz);
    const auto mt = std::filesystem::last_write_time (file, ec);
    if (! ec)
    {
        fp.mtimeNs = static_cast<std::int64_t> (mt.time_since_epoch().count());
    }
    return fp;
}

const LibraryCacheFileEntry* LibraryCache::find (const std::filesystem::path& absolutePath) const
{
    const auto it = byPath.find (pathKey (absolutePath));
    return it == byPath.end() ? nullptr : &it->second;
}

void LibraryCache::upsert (const std::filesystem::path& absolutePath, LibraryCacheFileEntry entry)
{
    byPath[pathKey (absolutePath)] = std::move (entry);
}

void LibraryCache::retainUnderBases (const std::vector<std::filesystem::path>& baseFolders)
{
    if (baseFolders.empty())
    {
        byPath.clear();
        return;
    }

    for (auto it = byPath.begin(); it != byPath.end();)
    {
        const std::filesystem::path abs = it->first;
        bool keep = false;
        for (const auto& base : baseFolders)
        {
            if (pathUnderBase (abs, base))
            {
                keep = true;
                break;
            }
        }
        if (keep)
            ++it;
        else
            it = byPath.erase (it);
    }
}

bool LibraryCache::loadFromFile (const std::filesystem::path& path)
{
    clear();
    std::ifstream in (path, std::ios::binary);
    if (! in)
        return false;

    char magic[4] {};
    in.read (magic, 4);
    if (! in || magic[0] != 'F' || magic[1] != 'M' || magic[2] != 'L' || magic[3] != 'C')
        return false;

    std::uint32_t version = 0;
    if (! readPod (in, version) || version != kFormatVersion)
        return false;

    std::uint32_t numFiles = 0;
    if (! readPod (in, numFiles) || numFiles > 2'000'000u)
        return false;

    byPath.reserve (numFiles);
    for (std::uint32_t f = 0; f < numFiles; ++f)
    {
        std::string abs;
        if (! readString (in, abs))
        {
            clear();
            return false;
        }

        LibraryCacheFileEntry entry;
        if (! readPod (in, entry.fingerprint.mtimeNs)
            || ! readPod (in, entry.fingerprint.sizeBytes))
        {
            clear();
            return false;
        }

        std::uint32_t numVoices = 0;
        if (! readPod (in, numVoices) || numVoices > 10'000u)
        {
            clear();
            return false;
        }

        entry.voices.reserve (numVoices);
        for (std::uint32_t v = 0; v < numVoices; ++v)
        {
            PatchEntry e;
            std::int32_t slot = 0;
            if (! readPod (in, slot) || ! readPod (in, e.contentId))
            {
                clear();
                return false;
            }
            e.bankSlot = slot;

            std::string baseStr;
            if (! readString (in, e.voiceName)
                || ! readString (in, e.fileName)
                || ! readString (in, e.relativePath)
                || ! readString (in, baseStr))
            {
                clear();
                return false;
            }
            e.baseFolder = baseStr;
            e.absolutePath = abs;
            in.read (reinterpret_cast<char*> (e.voice.data()),
                     static_cast<std::streamsize> (e.voice.size()));
            if (! in)
            {
                clear();
                return false;
            }
            e.refreshSearchCache();
            entry.voices.push_back (std::move (e));
        }

        byPath.emplace (std::move (abs), std::move (entry));
    }

    return true;
}

bool LibraryCache::saveToFile (const std::filesystem::path& path) const
{
    std::error_code ec;
    std::filesystem::create_directories (path.parent_path(), ec);

    const auto tmp = path.string() + ".tmp";
    auto removeTmp = [&]
    {
        std::error_code ignored;
        std::filesystem::remove (tmp, ignored);
    };

    std::ofstream out (tmp, std::ios::binary | std::ios::trunc);
    if (! out)
        return false;

    out.write ("FMLC", 4);
    if (! writePod (out, kFormatVersion))
    {
        removeTmp();
        return false;
    }
    const auto numFiles = static_cast<std::uint32_t> (byPath.size());
    if (! writePod (out, numFiles))
    {
        removeTmp();
        return false;
    }

    for (const auto& [abs, entry] : byPath)
    {
        if (! writeString (out, abs))
        {
            removeTmp();
            return false;
        }
        if (! writePod (out, entry.fingerprint.mtimeNs)
            || ! writePod (out, entry.fingerprint.sizeBytes))
        {
            removeTmp();
            return false;
        }
        const auto numVoices = static_cast<std::uint32_t> (entry.voices.size());
        if (! writePod (out, numVoices))
        {
            removeTmp();
            return false;
        }
        for (const auto& e : entry.voices)
        {
            const auto slot = static_cast<std::int32_t> (e.bankSlot);
            if (! writePod (out, slot) || ! writePod (out, e.contentId))
            {
                removeTmp();
                return false;
            }
            if (! writeString (out, e.voiceName)
                || ! writeString (out, e.fileName)
                || ! writeString (out, e.relativePath)
                || ! writeString (out, e.baseFolder.generic_string()))
            {
                removeTmp();
                return false;
            }
            out.write (reinterpret_cast<const char*> (e.voice.data()),
                       static_cast<std::streamsize> (e.voice.size()));
            if (! out)
            {
                removeTmp();
                return false;
            }
        }
    }

    out.close();
    if (! out)
    {
        removeTmp();
        return false;
    }

    std::filesystem::rename (tmp, path, ec);
    if (ec)
    {
        std::filesystem::remove (path, ec);
        std::filesystem::rename (tmp, path, ec);
    }
    if (ec)
        removeTmp();
    return ! ec;
}

} // namespace fmlib
