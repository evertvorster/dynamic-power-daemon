#pragma once

#include "RootCompositeFeature.h"

namespace dp::features {

class RootFeatureDetector {
public:
    static RootCompositeFeature::State detect();
};

} // namespace dp::features
