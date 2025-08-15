#pragma once
#include <QString>

namespace dp::features {

// Future-proof interface. It's fine if nothing uses it yet.
class Feature {
public:
    virtual ~Feature() = default;

    // Metadata
    virtual QString id() const = 0;
    virtual QString title() const = 0;

    // Availability / toggle
    virtual bool isSupported() const = 0;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) const = 0;

    // UI helpers
    virtual QString statusText() const = 0;

    // Actions
    virtual void applyForPowerState(bool onBattery) const = 0;
    virtual void refreshStatus() const = 0;
};

} // namespace dp::features
