#pragma once

#include <openxr/openxr.h>
#include <aurora/aurora.h>
#include <dolphin/mtx.h>
#include <vector>
#include <string>

namespace dusk::vr {

class VrSystem {
public:
    static VrSystem& getInstance();

    bool initialize();
    bool createSession(const AuroraInfo& info);
    void shutdown();

    bool isEnabled() const { return m_enabled; }
    XrInstance getXrInstance() const { return m_instance; }
    XrSession getXrSession() const { return m_session; }

    void pollEvents();
    void updateHeadPose(XrTime predictedTime);

    XrVector3f getHeadPosition() const { return m_headPose.position; }
    XrQuaternionf getHeadOrientation() const { return m_headPose.orientation; }
    
    void getHeadMatrix(Mtx m) const;

private:
    VrSystem() = default;
    ~VrSystem() = default;

    bool createInstance();
    bool createSession();
    bool createSpaces();
    
    bool m_enabled = false;
    XrInstance m_instance = XR_NULL_HANDLE;
    XrSystemId m_systemId = XR_NULL_SYSTEM_ID;
    XrSession m_session = XR_NULL_HANDLE;
    XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;
    XrSpace m_appSpace = XR_NULL_HANDLE;
    XrSpace m_viewSpace = XR_NULL_HANDLE;
    XrPosef m_headPose = {{0, 0, 0, 1}, {0, 0, 0}};
};

} // namespace dusk::vr
