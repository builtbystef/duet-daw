#include <duet/gui/AcceleratedSurface.h>

#include <juce_opengl/juce_opengl.h>

namespace duet::gui
{
AcceleratedSurface::AcceleratedSurface() = default;

AcceleratedSurface::~AcceleratedSurface() { attachTo (nullptr); }

void AcceleratedSurface::attachTo (juce::Component* surface)
{
    if (context != nullptr)
    {
        context->detach();
        context.reset();
    }

    if (surface == nullptr)
        return;

    context = std::make_unique<juce::OpenGLContext>();

    // Continuous repainting is what a context is for when something animates;
    // a workstation surface repaints when something changes, and asking for
    // frames nothing has changed would spend a core on redrawing a still
    // picture.
    context->setContinuousRepainting (false);
    context->attachTo (*surface);
}

bool AcceleratedSurface::isAttached() const { return context != nullptr; }
} // namespace duet::gui
