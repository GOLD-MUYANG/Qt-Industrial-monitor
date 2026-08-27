#include "VisionTypes.h"

namespace industrial::monitor::vision {

void registerVisionMetaTypes()
{
    qRegisterMetaType<VisionPlaybackState>();
    qRegisterMetaType<VisionSourceInfo>();
    qRegisterMetaType<VisionFrameResult>();
}

} // namespace industrial::monitor::vision
