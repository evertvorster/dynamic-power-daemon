// File: src/userctl/features/FeatureRegistry.cpp
#include "FeatureRegistry.h"
#include "PanelAutohideFeature.h"

namespace dp::features {

void FeatureRegistry::applyAll(bool onBattery) const {
    if (m_screenRefresh.readState().enabled) {
        m_screenRefresh.applyForPowerState(onBattery);
    }
    if (m_panelAutohide.readState().enabled) {
        m_panelAutohide.applyForPowerState(onBattery);
    }
}

void FeatureRegistry::refreshAll() const {
    m_screenRefresh.refreshStatus();
    m_panelAutohide.refreshStatus();
}

} // namespace dp::features
