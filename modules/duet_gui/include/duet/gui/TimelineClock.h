#pragma once

namespace duet::gui
{
/** The project's musical time, as the surfaces that draw it read it.

    The timeline is drawn in beats and the transport runs in seconds, and this is
    where the two meet: one open project answers it, and every surface drawing
    musical time — the arrangement now, the piano roll and the automation lanes
    later — asks it rather than the engine.

    It is an interface and not a session for two reasons. A surface outlives the
    project open in it, so what the arrangement holds has to be able to be
    nothing at all; and a repaint asks these questions many times a second, so
    what answers them must be values the engine has already published rather than
    anything that takes a lock (spec 535bbo).
*/
class TimelineClock
{
public:
    virtual ~TimelineClock() = default;

    TimelineClock (const TimelineClock& other) = delete;
    TimelineClock& operator= (const TimelineClock& other) = delete;

    /** How many beats a bar is, which is what the project's time signature
        says: four in 4/4, three in 3/4, three in 6/8.
    */
    [[nodiscard]] virtual double beatsPerBar() const = 0;

    /** Where the playhead is, in beats from the start of the project. */
    [[nodiscard]] virtual double playheadBeats() const = 0;

    /** Moves the playhead. Never an Action: where the producer is listening
        from is not a change to the project, so an undo never puts it back
        (ADR 0004).
    */
    virtual void setPlayheadBeats (double beats) = 0;

    [[nodiscard]] virtual bool isPlaying() const = 0;

    /** How far the project's content reaches, in beats. Zero for a project with
        nothing in it. It is what zoom-to-fit fits.
    */
    [[nodiscard]] virtual double contentLengthBeats() const = 0;

protected:
    TimelineClock() = default;
};
} // namespace duet::gui
