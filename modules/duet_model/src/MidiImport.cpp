#include <duet/model/MidiImport.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace duet::model
{
namespace
{
    constexpr std::uint32_t headerMagic = 0x4D546864; // MThd
    constexpr std::uint32_t trackMagic = 0x4D54726B;  // MTrk
    constexpr std::uint32_t minimumHeaderLength = 6;
    constexpr int maxVlqBytes = 4;
    constexpr int metaEndOfTrack = 0x2F;
    constexpr int smpteDropFrameFps = 29;
    constexpr double smpteDropFrameRate = 30.0 / 1.001;
    constexpr const char* emptyFileMessage = "This MIDI file is empty.";
    constexpr const char* unreadableFileMessage = "This MIDI file could not be read.";
    constexpr const char* noNotesMessage = "This MIDI file has no notes to import.";

    struct Reader
    {
        std::span<const std::uint8_t> bytes;
        std::size_t pos = 0;
        bool ok = true;

        [[nodiscard]] std::size_t remaining() const
        {
            return pos <= bytes.size() ? bytes.size() - pos : 0;
        }

        std::uint8_t u8()
        {
            if (! ok || pos >= bytes.size())
            {
                ok = false;
                return 0;
            }

            const auto value = bytes[pos];
            ++pos;
            return value;
        }

        std::uint16_t u16()
        {
            const auto high = u8();
            const auto low = u8();
            return static_cast<std::uint16_t> ((static_cast<std::uint16_t> (high) << 8) | low);
        }

        std::uint32_t u32()
        {
            const auto high = u16();
            const auto low = u16();
            return (static_cast<std::uint32_t> (high) << 16) | low;
        }

        std::uint32_t vlq()
        {
            std::uint32_t value = 0;

            for (int byte = 0; byte < maxVlqBytes; ++byte)
            {
                const auto next = u8();
                value = (value << 7) | (next & 0x7FU);

                if ((next & 0x80U) == 0)
                    return value;
            }

            ok = false;
            return 0;
        }

        std::span<const std::uint8_t> take (std::uint32_t count)
        {
            const auto wanted = static_cast<std::size_t> (count);

            if (! ok || wanted > remaining())
            {
                ok = false;
                return {};
            }

            const auto slice = bytes.subspan (pos, wanted);
            pos += wanted;
            return slice;
        }
    };

    struct OpenNote
    {
        std::uint64_t tick = 0;
        int pitch = 0;
        int velocity = 0;
        int eventIndex = 0;
    };

    struct ParsedNote
    {
        std::uint64_t startTick = 0;
        std::uint64_t endTick = 0;
        int pitch = 0;
        int velocity = 0;
        int track = 0;
        int channel = 0;
        int eventIndex = 0;
    };

    struct EventStatus
    {
        std::uint8_t status = 0;
        bool haveBuffered = false;
        std::uint8_t buffered = 0;
    };

    using OpenNotes = std::unordered_map<int, std::queue<OpenNote>>;

    std::uint8_t readDataByte (Reader& reader, bool& haveBuffered, std::uint8_t& buffered)
    {
        if (haveBuffered)
        {
            haveBuffered = false;
            return buffered;
        }

        return reader.u8();
    }

    int openKey (int channel, int pitch) { return (channel << 8) | pitch; }

    bool readEventStatus (Reader& reader, std::uint8_t& runningStatus, EventStatus& decoded)
    {
        const auto first = reader.u8();

        if (first < 0x80)
        {
            if (runningStatus == 0)
                return false;

            decoded.status = runningStatus;
            decoded.haveBuffered = true;
            decoded.buffered = first;
            return reader.ok;
        }

        decoded.status = first;
        runningStatus = first < 0xF0 ? first : std::uint8_t { 0 };
        return reader.ok;
    }

    bool skipMeta (Reader& reader, int& unsupportedEvents, bool& ended)
    {
        const auto type = reader.u8();
        reader.take (reader.vlq());

        if (type == metaEndOfTrack)
        {
            ended = true;
            return reader.ok;
        }

        ++unsupportedEvents;
        return reader.ok;
    }

    bool skipSysex (Reader& reader, int& unsupportedEvents)
    {
        reader.take (reader.vlq());
        ++unsupportedEvents;
        return reader.ok;
    }

    void applyNoteOn (OpenNotes& open,
                      std::uint64_t tick,
                      int channel,
                      int pitch,
                      int velocity,
                      int& eventIndex)
    {
        open[openKey (channel, pitch)].push ({ tick, pitch, velocity, eventIndex });
        ++eventIndex;
    }

    void applyNoteOff (OpenNotes& open,
                       std::vector<ParsedNote>& notes,
                       std::uint64_t tick,
                       int track,
                       int channel,
                       int pitch)
    {
        auto found = open.find (openKey (channel, pitch));

        if (found == open.end() || found->second.empty())
            return;

        const auto on = found->second.front();
        found->second.pop();

        if (tick > on.tick)
            notes.push_back (
                { on.tick, tick, on.pitch, on.velocity, track, channel, on.eventIndex });
    }

    bool applyChannelEvent (Reader& reader,
                            EventStatus& decoded,
                            std::uint64_t tick,
                            int track,
                            OpenNotes& open,
                            std::vector<ParsedNote>& notes,
                            int& eventIndex,
                            int& unsupportedEvents)
    {
        const auto command = static_cast<std::uint8_t> (decoded.status & 0xF0);
        const auto channel = static_cast<int> (decoded.status & 0x0F);
        const auto data1 = readDataByte (reader, decoded.haveBuffered, decoded.buffered);
        const auto data2 = (command == 0xC0 || command == 0xD0)
                               ? std::uint8_t { 0 }
                               : readDataByte (reader, decoded.haveBuffered, decoded.buffered);

        if (! reader.ok)
            return false;

        const auto pitch = static_cast<int> (data1);
        const auto velocity = static_cast<int> (data2);

        if (command == 0x90 && velocity > 0)
        {
            applyNoteOn (open, tick, channel, pitch, velocity, eventIndex);
            return true;
        }

        if (command == 0x80 || command == 0x90)
        {
            applyNoteOff (open, notes, tick, track, channel, pitch);
            return true;
        }

        ++unsupportedEvents;
        return true;
    }

    int countOpenNotes (const OpenNotes& open)
    {
        auto unmatched = 0;

        for (const auto& entry : open)
            unmatched += static_cast<int> (entry.second.size());

        return unmatched;
    }

    enum class EventOutcome : std::uint8_t
    {
        next,
        ended,
        failed
    };

    EventOutcome parseOneEvent (Reader& reader,
                                std::uint8_t& runningStatus,
                                std::uint64_t tick,
                                int track,
                                OpenNotes& open,
                                std::vector<ParsedNote>& notes,
                                int& eventIndex,
                                int& unsupportedEvents)
    {
        EventStatus decoded;

        if (! readEventStatus (reader, runningStatus, decoded))
            return EventOutcome::failed;

        if (decoded.status == 0xFF)
        {
            auto ended = false;

            if (! skipMeta (reader, unsupportedEvents, ended))
                return EventOutcome::failed;

            return ended ? EventOutcome::ended : EventOutcome::next;
        }

        if (decoded.status == 0xF0 || decoded.status == 0xF7)
            return skipSysex (reader, unsupportedEvents) ? EventOutcome::next
                                                         : EventOutcome::failed;

        if (decoded.status >= 0xF0)
            return EventOutcome::failed;

        return applyChannelEvent (
                   reader, decoded, tick, track, open, notes, eventIndex, unsupportedEvents)
                   ? EventOutcome::next
                   : EventOutcome::failed;
    }

    bool parseTrack (std::span<const std::uint8_t> bytes,
                     int track,
                     std::vector<ParsedNote>& notes,
                     int& unmatchedNoteOns,
                     int& unsupportedEvents)
    {
        Reader reader { bytes };
        std::uint64_t tick = 0;
        std::uint8_t runningStatus = 0;
        int eventIndex = 0;
        OpenNotes open;

        while (reader.ok && reader.remaining() > 0)
        {
            tick += reader.vlq();

            if (! reader.ok)
                return false;

            const auto outcome = parseOneEvent (
                reader, runningStatus, tick, track, open, notes, eventIndex, unsupportedEvents);

            if (outcome == EventOutcome::failed)
                return false;

            if (outcome == EventOutcome::ended)
                break;
        }

        if (! reader.ok)
            return false;

        unmatchedNoteOns += countOpenNotes (open);
        return true;
    }

    double ticksToBeatsFactor (std::uint16_t division, double projectTempoBpm, bool& ok)
    {
        if ((division & 0x8000U) == 0)
        {
            if (division == 0)
            {
                ok = false;
                return 0.0;
            }

            return 1.0 / static_cast<double> (division);
        }

        if (! std::isfinite (projectTempoBpm) || projectTempoBpm <= 0.0)
        {
            ok = false;
            return 0.0;
        }

        const auto fpsCode = -static_cast<int> (static_cast<std::int8_t> (division >> 8));
        const auto ticksPerFrame = division & 0xFFU;

        if (fpsCode <= 0 || ticksPerFrame == 0)
        {
            ok = false;
            return 0.0;
        }

        const auto fps =
            fpsCode == smpteDropFrameFps ? smpteDropFrameRate : static_cast<double> (fpsCode);
        const auto ticksPerSecond = fps * static_cast<double> (ticksPerFrame);
        return (projectTempoBpm / 60.0) / ticksPerSecond;
    }

    std::string counted (int count, std::string_view singular, std::string_view plural)
    {
        return std::to_string (count) + " " + std::string (count == 1 ? singular : plural);
    }

    std::string summaryText (int noteCount, int unmatchedNoteOns, int unsupportedEvents)
    {
        return counted (noteCount, "note", "notes") + ", "
               + counted (unmatchedNoteOns, "unmatched note-on", "unmatched note-ons") + ", "
               + counted (unsupportedEvents, "unsupported event", "unsupported events") + ".";
    }

    MidiImport failure (const char* message)
    {
        MidiImport imported;
        imported.message = message;
        return imported;
    }
} // namespace

MidiImport parseStandardMidiFile (std::span<const std::uint8_t> bytes, double projectTempoBpm)
{
    if (bytes.empty())
        return failure (emptyFileMessage);

    Reader reader { bytes };

    if (reader.u32() != headerMagic)
        return failure (unreadableFileMessage);

    const auto headerLength = reader.u32();

    if (headerLength < minimumHeaderLength)
        return failure (unreadableFileMessage);

    const auto format = reader.u16();
    const auto trackCount = reader.u16();
    const auto division = reader.u16();
    reader.take (headerLength - minimumHeaderLength);

    if (! reader.ok || format > 1)
        return failure (unreadableFileMessage);

    auto timingOk = true;
    const auto ticksToBeats = ticksToBeatsFactor (division, projectTempoBpm, timingOk);

    if (! timingOk)
        return failure (unreadableFileMessage);

    std::vector<ParsedNote> parsed;
    auto unmatchedNoteOns = 0;
    auto unsupportedEvents = 0;

    for (std::uint16_t track = 0; track < trackCount; ++track)
    {
        if (reader.u32() != trackMagic)
            return failure (unreadableFileMessage);

        const auto trackBytes = reader.take (reader.u32());

        if (! reader.ok
            || ! parseTrack (
                trackBytes, static_cast<int> (track), parsed, unmatchedNoteOns, unsupportedEvents))
            return failure (unreadableFileMessage);
    }

    std::sort (
        parsed.begin(),
        parsed.end(),
        [] (const ParsedNote& first, const ParsedNote& second)
        {
            return std::tie (
                       first.startTick, first.pitch, first.track, first.channel, first.eventIndex)
                   < std::tie (second.startTick,
                               second.pitch,
                               second.track,
                               second.channel,
                               second.eventIndex);
        });

    MidiImport imported;
    imported.unmatchedNoteOns = unmatchedNoteOns;
    imported.unsupportedEvents = unsupportedEvents;
    imported.notes.reserve (parsed.size());

    for (const auto& note : parsed)
    {
        MidiImportedNote made;
        made.startBeats = static_cast<double> (note.startTick) * ticksToBeats;
        made.lengthBeats = static_cast<double> (note.endTick - note.startTick) * ticksToBeats;
        made.pitch = note.pitch;
        made.velocity = note.velocity;
        imported.notes.push_back (made);
    }

    if (imported.notes.empty())
    {
        imported.message = noNotesMessage;
        return imported;
    }

    imported.ok = true;
    imported.message =
        summaryText (static_cast<int> (imported.notes.size()), unmatchedNoteOns, unsupportedEvents);
    return imported;
}
} // namespace duet::model
