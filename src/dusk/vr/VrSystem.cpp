#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif

#ifdef _WIN32
#ifndef XR_USE_PLATFORM_WIN32
#define XR_USE_PLATFORM_WIN32
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <openxr/openxr.h>

#include "VrSystem.hpp"
#include "dusk/logging.h"
#include <iostream>
#include <cstring>
#include <cmath>

namespace dusk::vr {

struct VrSystem::Impl {
    XrInstance instance = XR_NULL_HANDLE;
    XrSession session = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    XrSpace appSpace = XR_NULL_HANDLE;
    XrSpace viewSpace = XR_NULL_HANDLE;
    
    struct Swapchain {
        XrSwapchain handle = XR_NULL_HANDLE;
    };
    
    Swapchain swapchains[2];
    XrView views[2] = { {XR_TYPE_VIEW}, {XR_TYPE_VIEW} };
    XrPosef headPose = { {0, 0, 0, 1}, {0, 0, 0} };
};

namespace {
    void XrPoseToMtx(const XrPosef& pose, Mtx m, float scale) {
        const auto& q = pose.orientation;
        const auto& p = pose.position;

        float x2 = q.x + q.x;
        float y2 = q.y + q.y;
        float z2 = q.z + q.z;
        float xx = q.x * x2;
        float xy = q.x * y2;
        float xz = q.x * z2;
        float yy = q.y * y2;
        float yz = q.y * z2;
        float zz = q.z * z2;
        float wx = q.w * x2;
        float wy = q.w * y2;
        float wz = q.w * z2;

        m[0][0] = 1.0f - (yy + zz);
        m[0][1] = xy - wz;
        m[0][2] = xz + wy;
        m[0][3] = p.x * scale;

        m[1][0] = xy + wz;
        m[1][1] = 1.0f - (xx + zz);
        m[1][2] = yz - wx;
        m[1][3] = p.y * scale;

        m[2][0] = xz - wy;
        m[2][1] = yz + wx;
        m[2][2] = 1.0f - (xx + yy);
        m[2][3] = p.z * scale;
    }
}

VrSystem::VrSystem() {
    m_impl = new Impl();
}

VrSystem::~VrSystem() {
    delete m_impl;
}

VrSystem& VrSystem::getInstance() {
    static VrSystem instance;
    return instance;
}

bool VrSystem::initialize() {
    if (!createInstance()) {
        return false;
    }

    XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrResult result = xrGetSystem(m_impl->instance, &systemGetInfo, &m_impl->systemId);
    if (result != XR_SUCCESS) {
        DuskLog.error("OpenXR: Failed to get system (Result: {})", (int)result);
        return false;
    }

    m_enabled = true;
    DuskLog.info("OpenXR: Initialized");
    return true;
}

void VrSystem::shutdown() {
    if (m_impl->viewSpace != XR_NULL_HANDLE) xrDestroySpace(m_impl->viewSpace);
    if (m_impl->appSpace != XR_NULL_HANDLE) xrDestroySpace(m_impl->appSpace);
    for (int i = 0; i < 2; ++i) {
        if (m_impl->swapchains[i].handle != XR_NULL_HANDLE) {
            xrDestroySwapchain(m_impl->swapchains[i].handle);
        }
    }
    if (m_impl->session != XR_NULL_HANDLE) xrDestroySession(m_impl->session);
    if (m_impl->instance != XR_NULL_HANDLE) xrDestroyInstance(m_impl->instance);
    m_enabled = false;
}

void VrSystem::pollEvents() {
    if (m_impl->instance == XR_NULL_HANDLE) return;

    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(m_impl->instance, &event) == XR_SUCCESS) {
        switch (event.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                auto* stateChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
                m_impl->sessionState = stateChanged->state;
                if (m_impl->sessionState == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                    beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    xrBeginSession(m_impl->session, &beginInfo);
                    createSpaces();
                    createSwapchains();
                }
                break;
            }
            default: break;
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }

    if (m_impl->sessionState >= XR_SESSION_STATE_SYNCHRONIZED) {
        XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        xrWaitFrame(m_impl->session, &waitInfo, &frameState);

        XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        xrBeginFrame(m_impl->session, &beginInfo);

        updateHeadPose(frameState.predictedDisplayTime);

        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 0;
        xrEndFrame(m_impl->session, &endInfo);
    }
}

void VrSystem::updateHeadPose(uint64_t predictedTime) {
    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
    if (xrLocateSpace(m_impl->viewSpace, m_impl->appSpace, (XrTime)predictedTime, &location) == XR_SUCCESS) {
        m_impl->headPose = location.pose;
    }

    XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
    viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    viewLocateInfo.displayTime = (XrTime)predictedTime;
    viewLocateInfo.space = m_impl->appSpace;

    XrViewState viewState{XR_TYPE_VIEW_STATE};
    uint32_t viewCountOutput;
    xrLocateViews(m_impl->session, &viewLocateInfo, &viewState, 2, &viewCountOutput, m_impl->views);
}

bool VrSystem::createSession(const AuroraInfo& info) {
    XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
    createInfo.systemId = m_impl->systemId;
    XrResult result = xrCreateSession(m_impl->instance, &createInfo, &m_impl->session);
    return result == XR_SUCCESS;
}

bool VrSystem::createSpaces() {
    XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceCreateInfo.poseInReferenceSpace = {{0, 0, 0, 1}, {0, 0, 0}};
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    xrCreateReferenceSpace(m_impl->session, &spaceCreateInfo, &m_impl->appSpace);
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    xrCreateReferenceSpace(m_impl->session, &spaceCreateInfo, &m_impl->viewSpace);
    return true;
}

bool VrSystem::createSwapchains() {
    uint32_t viewCount;
    xrEnumerateViewConfigurationViews(m_impl->instance, m_impl->systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    std::vector<XrViewConfigurationView> views(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    xrEnumerateViewConfigurationViews(m_impl->instance, m_impl->systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, views.data());

    for (uint32_t i = 0; i < 2; ++i) {
        XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        createInfo.arraySize = 1;
        createInfo.format = 43; // VK_FORMAT_R8G8B8A8_SRGB
        createInfo.width = views[i].recommendedImageRectWidth;
        createInfo.height = views[i].recommendedImageRectHeight;
        createInfo.mipCount = 1;
        createInfo.faceCount = 1;
        createInfo.sampleCount = views[i].recommendedSwapchainSampleCount;
        createInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        xrCreateSwapchain(m_impl->session, &createInfo, &m_impl->swapchains[i].handle);
    }
    return true;
}

uint32_t VrSystem::acquireImage(uint32_t eye) {
    uint32_t index;
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    xrAcquireSwapchainImage(m_impl->swapchains[eye].handle, &acquireInfo, &index);
    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(m_impl->swapchains[eye].handle, &waitInfo);
    return index;
}

void VrSystem::releaseImage(uint32_t eye) {
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(m_impl->swapchains[eye].handle, &releaseInfo);
}

void VrSystem::getHeadMatrix(Mtx m) const {
    XrPoseToMtx(m_impl->headPose, m, 100.0f);
}

void VrSystem::getEyeViewMatrix(uint32_t eye, Mtx m) const {
    XrPoseToMtx(m_impl->views[eye].pose, m, 100.0f);
    Mtx temp;
    MTXInverse(m, temp);
    MTXCopy(temp, m);
}

void VrSystem::getEyeProjectionMatrix(uint32_t eye, Mtx44 m, float nearZ, float farZ) const {
    const float tanLeft = std::tan(m_impl->views[eye].fov.angleLeft);
    const float tanRight = std::tan(m_impl->views[eye].fov.angleRight);
    const float tanUp = std::tan(m_impl->views[eye].fov.angleUp);
    const float tanDown = std::tan(m_impl->views[eye].fov.angleDown);
    const float tanWidth = tanRight - tanLeft;
    const float tanHeight = tanUp - tanDown;
    std::memset(m, 0, sizeof(Mtx44));
    m[0][0] = 2.0f / tanWidth;
    m[0][2] = (tanRight + tanLeft) / tanWidth;
    m[1][1] = 2.0f / tanHeight;
    m[1][2] = (tanUp + tanDown) / tanHeight;
    m[2][2] = -farZ / (farZ - nearZ);
    m[2][3] = -(farZ * nearZ) / (farZ - nearZ);
    m[3][2] = -1.0f;
}

bool VrSystem::createInstance() {
    std::vector<const char*> extensions;
    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strncpy(createInfo.applicationInfo.applicationName, "Dusk VR", XR_MAX_APPLICATION_NAME_SIZE);
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    createInfo.enabledExtensionCount = (uint32_t)extensions.size();
    createInfo.enabledExtensionNames = extensions.data();
    XrResult result = xrCreateInstance(&createInfo, &m_impl->instance);
    return result == XR_SUCCESS;
}

} // namespace dusk::vr
