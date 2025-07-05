#include "../CameraDevice.h"

// Empty implementation for visionOS since camera access is not supported
namespace Babylon
{
    struct CameraDevice::Impl
    {
        Impl() = default;
    };

    CameraDevice::CameraDevice() : m_impl(std::make_unique<Impl>()) {}
    CameraDevice::~CameraDevice() = default;
    
    void CameraDevice::Open(const std::string&, const CameraConstraints&) {}
    void CameraDevice::UpdateConstraints(const CameraConstraints&) {}
    void CameraDevice::Close() {}
    
    std::vector<CameraInfo> CameraDevice::GetCameraInfos() { return {}; }
    Babylon::CameraPermissionState CameraDevice::GetCameraPermissionState() { return Babylon::CameraPermissionState::Denied; }
    
    arcana::task<bool, std::exception_ptr> CameraDevice::RequestCameraPermissionAsync() {
        return arcana::task_from_result<std::exception_ptr>(false);
    }
}
