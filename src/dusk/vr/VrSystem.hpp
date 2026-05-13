#pragma once

#include <aurora/aurora.h>
#include <dolphin/mtx.h>
#include <vector>
#include <string>
#include <cstdint>

namespace dusk::vr {

class VrSystem {
public:
    static VrSystem& getInstance();

    bool initialize();
    bool createSession(const AuroraInfo& info);
    void shutdown();

    bool isEnabled() const { return m_enabled; }
    
    void pollEvents();
    void updateHeadPose(uint64_t predictedTime);

    void getHeadMatrix(Mtx m) const;
    void getEyeViewMatrix(uint32_t eye, Mtx m) const;
    void getEyeProjectionMatrix(uint32_t eye, Mtx44 m, float nearZ, float farZ) const;

    bool createSwapchains();
    uint32_t acquireImage(uint32_t eye);
    void releaseImage(uint32_t eye);

private:
    VrSystem();
    ~VrSystem();

    bool createInstance();
    bool createSpaces();
    
    bool m_enabled = false;
    
    struct Impl;
    Impl* m_impl;
};

} // namespace dusk::vr
