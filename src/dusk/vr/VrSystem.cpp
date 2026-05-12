#include "VrSystem.hpp"
#include <iostream>
#include <cstring>

namespace dusk::vr {

VrSystem& VrSystem::getInstance() {
    static VrSystem instance;
    return instance;
}

bool VrSystem::initialize() {
    if (!createInstance()) {
        return false;
    }

    // Get the system for the form factor
    XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrResult result = xrGetSystem(m_instance, &systemGetInfo, &m_systemId);
    if (result != XR_SUCCESS) {
        std::cerr << "OpenXR: Failed to get system for HMD form factor" << std::endl;
        return false;
    }

    m_enabled = true;
    std::cout << "OpenXR: Initialized successfully" << std::endl;
    return true;
}

void VrSystem::shutdown() {
    if (m_viewSpace != XR_NULL_HANDLE) {
        xrDestroySpace(m_viewSpace);
        m_viewSpace = XR_NULL_HANDLE;
    }
    if (m_appSpace != XR_NULL_HANDLE) {
        xrDestroySpace(m_appSpace);
        m_appSpace = XR_NULL_HANDLE;
    }
    if (m_session != XR_NULL_HANDLE) {
        xrDestroySession(m_session);
        m_session = XR_NULL_HANDLE;
    }
    if (m_instance != XR_NULL_HANDLE) {
        xrDestroyInstance(m_instance);
        m_instance = XR_NULL_HANDLE;
    }
    m_enabled = false;
}

void VrSystem::pollEvents() {
    if (m_instance == XR_NULL_HANDLE) return;

    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(m_instance, &event) == XR_SUCCESS) {
        switch (event.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                auto* stateChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
                m_sessionState = stateChanged->state;
                if (m_sessionState == XR_SESSION_STATE_READY) {
                    // Start session
                    XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                    beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    xrBeginSession(m_session, &beginInfo);
                    createSpaces();
                } else if (m_sessionState == XR_SESSION_STATE_STOPPING) {
                    xrEndSession(m_session);
                }
                break;
            }
            default:
                break;
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }

    if (m_sessionState == XR_SESSION_STATE_SYNCHRONIZED ||
        m_sessionState == XR_SESSION_STATE_VISIBLE ||
        m_sessionState == XR_SESSION_STATE_FOCUSED) {
        updateHeadPose();
    }
}

void VrSystem::updateHeadPose() {
    if (m_session == XR_NULL_HANDLE || m_appSpace == XR_NULL_HANDLE || m_viewSpace == XR_NULL_HANDLE) {
        return;
    }

    XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    xrWaitFrame(m_session, &frameWaitInfo, &frameState);

    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
    XrResult result = xrLocateSpace(m_viewSpace, m_appSpace, frameState.predictedDisplayTime, &location);
    if (result == XR_SUCCESS && (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
        m_headPose = location.pose;
    }
}

bool VrSystem::createSession(const AuroraInfo& info) {
    if (m_instance == XR_NULL_HANDLE) return false;

    XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
    createInfo.next = nullptr; 
    createInfo.systemId = m_systemId;

    XrResult result = xrCreateSession(m_instance, &createInfo, &m_session);
    if (result != XR_SUCCESS) {
        std::cerr << "OpenXR: Failed to create session (Result: " << result << ")" << std::endl;
        return false;
    }

    std::cout << "OpenXR: Session created successfully with backend " << info.backend << std::endl;
    return true;
}

bool VrSystem::createSpaces() {
    XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceCreateInfo.poseInReferenceSpace = {{0, 0, 0, 1}, {0, 0, 0}};
    
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_appSpace);

    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_viewSpace);

    return true;
}

void VrSystem::getHeadMatrix(Mtx m) const {
    const auto& q = m_headPose.orientation;
    const auto& p = m_headPose.position;

    // Quaternion to Rotation Matrix
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
    m[0][3] = p.x;

    m[1][0] = xy + wz;
    m[1][1] = 1.0f - (xx + zz);
    m[1][2] = yz - wx;
    m[1][3] = p.y;

    m[2][0] = xz - wy;
    m[2][1] = yz + wx;
    m[2][2] = 1.0f - (xx + yy);
    m[2][3] = p.z;
}

bool VrSystem::createInstance() {
    std::vector<const char*> extensions;
    
    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.next = nullptr;
    createInfo.createFlags = 0;
    
    std::strncpy(createInfo.applicationInfo.applicationName, "Dusk VR", XR_MAX_APPLICATION_NAME_SIZE);
    createInfo.applicationInfo.applicationVersion = 1;
    std::strncpy(createInfo.applicationInfo.engineName, "Aurora", XR_MAX_ENGINE_NAME_SIZE);
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.enabledExtensionNames = extensions.data();

    XrResult result = xrCreateInstance(&createInfo, &m_instance);
    if (result != XR_SUCCESS) {
        std::cerr << "OpenXR: Failed to create instance" << std::endl;
        return false;
    }

    return true;
}

} // namespace dusk::vr
