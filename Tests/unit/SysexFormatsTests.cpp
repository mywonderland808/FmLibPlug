#include "TestHelpers.h"
#include "sysex/Dx7Formats.h"

using namespace fmlib;

TEST_CASE ("yamahaChecksum round-trip", "[sysex][formats]")
{
    const uint8_t payload[] = { 1, 2, 3, 4, 5 };
    const auto cs = yamahaChecksum (payload, sizeof (payload));
    REQUIRE (yamahaChecksumOk (payload, sizeof (payload), cs));
    REQUIRE_FALSE (yamahaChecksumOk (payload, sizeof (payload), static_cast<uint8_t> (cs ^ 1)));
}

TEST_CASE ("voiceNameFromData trims and unnamed", "[sysex][formats]")
{
    VoiceData v {};
    REQUIRE (voiceNameFromData (v) == "(unnamed)");

    const char* name = "Brass   ";
    for (int i = 0; i < 8; ++i)
        v[static_cast<size_t> (145 + i)] = static_cast<uint8_t> (name[i]);
    REQUIRE (voiceNameFromData (v) == "Brass");
}

TEST_CASE ("contentIdFromVoice is stable and sensitive", "[sysex][formats]")
{
    VoiceData a {};
    a[0] = 10;
    VoiceData b = a;
    const auto idA = contentIdFromVoice (a);
    REQUIRE (contentIdFromVoice (a) == idA);
    b[0] = 11;
    REQUIRE (contentIdFromVoice (b) != idA);
}

TEST_CASE ("contentIdFromVoice ignores name and unused VCED bits", "[sysex][formats]")
{
    VoiceData a {};
    a[0] = 10;
    a[11] = 1; // left curve
    a[145] = 'A';
    a[146] = 'B';
    const auto id = contentIdFromVoice (a);

    VoiceData renamed = a;
    renamed[145] = 'Z';
    REQUIRE (contentIdFromVoice (renamed) == id);

    VoiceData dirty = a;
    dirty[11] = static_cast<uint8_t> (1 | 0x7c); // unused bits above 2-bit curve
    REQUIRE (contentIdFromVoice (dirty) == id);

    VoiceData highBit = a;
    highBit[0] = static_cast<uint8_t> (10 | 0x80); // unused high bit on EG rate
    REQUIRE (contentIdFromVoice (highBit) == id);
}

TEST_CASE ("packVoice / unpackVoice round-trip on unpacked image", "[sysex][formats]")
{
    PackedVoice seed {};
    for (int i = 0; i < kPackedVoiceBytes; ++i)
        seed[static_cast<size_t> (i)] = static_cast<uint8_t> ((i * 3) & 0x7f);
    // Clamp packed bitfields to legal ranges so pack(unpack) can round-trip the packed form.
    for (int op = 0; op < 6; ++op)
    {
        const int base = op * 17;
        seed[static_cast<size_t> (base + 11)] &= 0x0f;
        seed[static_cast<size_t> (base + 12)] &= 0x7f;
        seed[static_cast<size_t> (base + 13)] &= 0x1f;
        seed[static_cast<size_t> (base + 15)] &= 0x3f;
    }
    seed[110] &= 0x1f;
    seed[111] &= 0x0f;
    seed[116] &= 0x7f;
    const char* n = "PackTestOK";
    for (int i = 0; i < kNameLength; ++i)
        seed[static_cast<size_t> (118 + i)] = static_cast<uint8_t> (n[i]);

    const auto voice = unpackVoice (seed);
    REQUIRE (voiceNameFromData (voice) == "PackTestOK");
    REQUIRE (unpackVoice (packVoice (voice)) == voice);
    REQUIRE (packVoice (voice) == seed);
}

TEST_CASE ("unpackVoice places detune after fine and feedback from byte 111", "[sysex][formats]")
{
    PackedVoice packed {};
    packed[12] = static_cast<uint8_t> (0x05 | (0x0a << 3)); // RS=5, DET=10 for OP6
    packed[16] = 44;                                        // fine
    packed[110] = 7;                                        // algorithm
    packed[111] = static_cast<uint8_t> (0x03 | (1 << 3));   // FB=3, OKS=1

    const auto v = unpackVoice (packed);
    REQUIRE (v[13] == 5);   // RS
    REQUIRE (v[19] == 44);  // fine
    REQUIRE (v[20] == 10);  // detune last in OP6
    REQUIRE (v[134] == 7);
    REQUIRE (v[135] == 3);
    REQUIRE (v[136] == 1);
}
