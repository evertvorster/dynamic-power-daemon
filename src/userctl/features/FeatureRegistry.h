// File: src/userctl/features/FeatureRegistry.h
#pragma once
#include "ScreenRefreshFeature.h"
#include "PanelAutohideFeature.h"

namespace dp::features {

class FeatureRegistry {
public:
    FeatureRegistry() = default;
    ~FeatureRegistry() = default;

    void applyAll(bool onBattery) const;
    void refreshAll() const;

private:
    mutable ScreenRefreshFeature m_screenRefresh;
    mutable PanelAutohideFeature m_panelAutohide;
};

} // namespace dp::features
