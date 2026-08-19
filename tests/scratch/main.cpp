#include <duet/model/Session.h>

#include <duet/testing/TestSupport.h>

#include <iostream>

/** A disposable engine probe.

    Replace this with whatever question you have. The ordinary build never
    reaches this file.
*/
int main()
{
    const duet::testing::TempProject project;
    duet::model::Session session { project.editFile() };

    session.loadDemoContent();
    session.useNoAudioDevice();
    const auto played = session.playWithoutAudioDevice (session.editLengthSeconds());

    std::cout << "tracks " << session.audioTrackCount() << ", tempo " << session.tempoBpm()
              << " bpm, length " << session.editLengthSeconds() << " s, played "
              << (played ? "yes" : "no") << '\n';

    return played ? 0 : 1;
}
