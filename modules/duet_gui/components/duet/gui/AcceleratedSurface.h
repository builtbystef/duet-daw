#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace juce
{
class OpenGLContext;
}

namespace duet::gui
{
/** The rendering escape hatch: one surface, rasterised by the graphics card.

    Duet draws on JUCE's software renderer, which spec 535bbo settled after the
    prototype held 60 tracks on it. This is the way out for a surface that ever
    stops holding: attaching one puts everything that component paints — and
    everything painted under it — through an OpenGL context instead. What is
    drawn is the same JUCE drawing either way, so nothing about the surface
    changes except who rasterises it.

    Attaching before the surface is on screen is fine: the context arrives with
    the window, and never at all if there is no window, which is what makes this
    safe to switch on in a headless run.
*/
class AcceleratedSurface final
{
public:
    AcceleratedSurface();
    ~AcceleratedSurface();

    AcceleratedSurface (const AcceleratedSurface& other) = delete;
    AcceleratedSurface& operator= (const AcceleratedSurface& other) = delete;

    /** Puts a surface on the hardware renderer, or a null one puts whatever was
        attached back on the software renderer.
    */
    void attachTo (juce::Component* surface);

    [[nodiscard]] bool isAttached() const;

private:
    std::unique_ptr<juce::OpenGLContext> context;
};
} // namespace duet::gui
