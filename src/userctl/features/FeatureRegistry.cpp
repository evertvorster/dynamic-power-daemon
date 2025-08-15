// File: src/userctl/features/FeatureRegistry.cpp
#include "FeatureRegistry.h"

namespace dp::features {

void FeatureRegistry::applyAll(bool onBattery) const {
    if (m_screenRefresh.readState().enabled) {
        m_screenRefresh.applyForPowerState(onBattery);
    }
}

void FeatureRegistry::refreshAll() const {
    m_screenRefresh.refreshStatus();
}

} // namespace dp::features
