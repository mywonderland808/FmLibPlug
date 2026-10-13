#include "midi/RtMidiRing.h"
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

using namespace fmlib;

TEST_CASE ("RtMidiRing short push/pop preserves order", "[midi][ring]")
{
    RtMidiRing<64, 4096> ring;
    const uint8_t a[] = { 0x90, 60, 100 };
    const uint8_t b[] = { 0x80, 60, 0 };
    REQUIRE (ring.tryPushShort (a, 3));
    REQUIRE (ring.tryPushShort (b, 3));

    RtMidiEvent ev;
    uint8_t scratch[16];
    REQUIRE (ring.popCopy (ev, scratch, 16));
    REQUIRE (ev.kind == RtMidiKind::shortMsg);
    REQUIRE (ev.size == 3);
    REQUIRE (ev.shortData[1] == 60);

    REQUIRE (ring.popCopy (ev, scratch, 16));
    REQUIRE (ev.shortData[0] == 0x80);
    REQUIRE (ring.empty());
}

TEST_CASE ("RtMidiRing SysEx round-trip including 4104 bulk", "[midi][ring]")
{
    HostMidiRing ring;
    std::vector<uint8_t> bulk (4104, 0x10);
    bulk[0] = 0xf0;
    bulk.back() = 0xf7;
    REQUIRE (ring.tryPushSysex (bulk.data(), (int) bulk.size()));

    RtMidiEvent ev;
    std::vector<uint8_t> out (4104);
    REQUIRE (ring.popCopy (ev, out.data(), (int) out.size()));
    REQUIRE (ev.kind == RtMidiKind::sysex);
    REQUIRE (ev.size == 4104);
    REQUIRE (out[0] == 0xf0);
    REQUIRE (out.back() == 0xf7);
}

TEST_CASE ("RtMidiRing arena wraparound keeps SysEx intact", "[midi][ring]")
{
    using SmallRing = RtMidiRing<32, 256>;
    SmallRing ring;
    std::vector<uint8_t> pad (180, 0x01);
    pad[0] = 0xf0;
    pad.back() = 0xf7;
    REQUIRE (ring.tryPushSysex (pad.data(), (int) pad.size()));

    RtMidiEvent ev;
    std::vector<uint8_t> out (256);
    REQUIRE (ring.popCopy (ev, out.data(), (int) out.size()));
    REQUIRE (ev.size == 180);

    // Write cursor is near the end; 100 bytes must wrap across the arena boundary.
    std::vector<uint8_t> msg (100, 0x22);
    msg[0] = 0xf0;
    msg.back() = 0xf7;
    REQUIRE (ring.tryPushSysex (msg.data(), (int) msg.size()));
    REQUIRE (ring.popCopy (ev, out.data(), (int) out.size()));
    REQUIRE (ev.size == 100);
    REQUIRE (out[0] == 0xf0);
    REQUIRE (out[99] == 0xf7);
}

TEST_CASE ("RtMidiRing drops oversized SysEx and counts", "[midi][ring]")
{
    HostMidiRing ring;
    std::vector<uint8_t> huge (HostMidiRing::kMaxSysexBytes + 1, 0);
    REQUIRE_FALSE (ring.tryPushSysex (huge.data(), (int) huge.size()));
    REQUIRE (ring.droppedSysex() == 1);
}

TEST_CASE ("RtMidiRing preserves SysEx fragment kind", "[midi][ring]")
{
    HostMidiRing ring;
    std::vector<uint8_t> dump (163, 0x11);
    dump[0] = 0xf0;
    dump.back() = 0xf7;
    REQUIRE (ring.tryPushSysex (dump.data(), 40, 0, 0.0, true));
    REQUIRE (ring.tryPushSysex (dump.data() + 40, (int) dump.size() - 40, 0, 0.0, false));

    RtMidiEvent ev;
    std::vector<uint8_t> a (4104), b (4104);
    REQUIRE (ring.popCopy (ev, a.data(), (int) a.size()));
    REQUIRE (ev.kind == RtMidiKind::sysexFragment);
    REQUIRE (ev.size == 40);
    REQUIRE (ring.popCopy (ev, b.data(), (int) b.size()));
    REQUIRE (ev.kind == RtMidiKind::sysex);
}

TEST_CASE ("RtMidiRing SPSC stress 1e5 events", "[midi][ring][stress]")
{
    HostMidiRing ring;
    constexpr int kN = 100000;
    std::atomic<bool> failed { false };

    std::thread producer ([&]
    {
        for (int i = 0; i < kN; ++i)
        {
            const uint8_t bytes[3] = { 0xb0, (uint8_t) (i & 0x7f), (uint8_t) ((i >> 7) & 0x7f) };
            while (! ring.tryPushShort (bytes, 3))
            {
                // backpressure
            }
        }
    });

    std::thread consumer ([&]
    {
        int got = 0;
        RtMidiEvent ev;
        uint8_t scratch[8];
        while (got < kN)
        {
            if (ring.popCopy (ev, scratch, 8))
            {
                if (ev.kind != RtMidiKind::shortMsg || ev.size != 3)
                    failed = true;
                ++got;
            }
        }
    });

    producer.join();
    consumer.join();
    REQUIRE_FALSE (failed.load());
    REQUIRE (ring.empty());
}

TEST_CASE ("RtShortMidiRing push/pop", "[midi][ring]")
{
    HostShortOutRing ring;
    const uint8_t note[] = { 0x90, 64, 90 };
    REQUIRE (ring.tryPush (note, 3));
    HostShortOutRing::Pod pod;
    REQUIRE (ring.pop (pod));
    REQUIRE (pod.size == 3);
    REQUIRE (pod.data[1] == 64);
}
