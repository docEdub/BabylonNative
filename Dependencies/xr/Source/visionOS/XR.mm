#if ! __has_feature(objc_arc)
#error "ARC is off"
#endif

#import <XR.h>
#import "../Shared/XRHelpers.h"

#import <UIKit/UIKit.h>
#import <CompositorServices/CompositorServices.h>
#import <MetalKit/MetalKit.h>

#import "Include/IXrContextvisionOS.h"

namespace {
    constexpr float DEFAULT_DEPTH_NEAR_Z = 0.1f;
    constexpr float DEFAULT_DEPTH_FAR_Z = 1000.0f;
    
    static xr::Pose TransformToPose(simd_float4x4 transform) {
        xr::Pose pose{};
        auto orientation = simd_quaternion(transform);
        pose.Orientation = { orientation.vector.x
            , orientation.vector.y
            , orientation.vector.z
            , orientation.vector.w };

        pose.Position = { transform.columns[3][0]
            , transform.columns[3][1]
            , transform.columns[3][2] };

        return pose;
    }

    static simd_float4x4 PoseToTransform(xr::Pose pose) {
        auto poseQuaternion = simd_quaternion(pose.Orientation.X, pose.Orientation.Y, pose.Orientation.Z, pose.Orientation.W);
        auto poseTransform = simd_matrix4x4(poseQuaternion);
        poseTransform.columns[3][0] = pose.Position.X;
        poseTransform.columns[3][1] = pose.Position.Y;
        poseTransform.columns[3][2] = pose.Position.Z;

        return poseTransform;
    }
}

namespace xr {
    struct XrContextvisionOS : public IXrContextvisionOS {
        bool Initialized{false};
        cp_layer_renderer_t LayerRenderer{nullptr};
        cp_frame_t Frame{nullptr};

        bool IsInitialized() const override
        {
            return Initialized;
        }

        cp_layer_renderer_t XrLayerRenderer() const override
        {
            return LayerRenderer;
        }

        cp_frame_t XrFrame() const override
        {
            return Frame;
        }

        virtual ~XrContextvisionOS() = default;
    };

    struct System::Impl {
    public:
        std::unique_ptr<XrContextvisionOS> XrContext{std::make_unique<XrContextvisionOS>()};

        Impl(const std::string&) {}

        bool IsInitialized() const {
            return XrContext->IsInitialized();
        }

        bool TryInitialize() {
            return true;
        }
    };

    struct System::Session::Impl {
    public:
        const System::Impl& SystemImpl;
        std::vector<Frame::View> ActiveFrameViews{ {} };
        std::vector<Frame::InputSource> InputSources;
        std::vector<Frame::Plane> Planes{};
        std::vector<Frame::Mesh> Meshes{};
        std::vector<std::unique_ptr<Frame::ImageTrackingResult>> ImageTrackingResults{};
        std::vector<FeaturePoint> FeaturePointCloud{};
        std::optional<Space> EyeTrackerSpace{};
        float DepthNearZ{ DEFAULT_DEPTH_NEAR_Z };
        float DepthFarZ{ DEFAULT_DEPTH_FAR_Z };
        bool FeaturePointCloudEnabled{ false };

        Impl(System::Impl& systemImpl, void* graphicsContext, void* commandQueue, std::function<void*()> windowProvider)
            : SystemImpl{ systemImpl }
            , metalDevice{ (__bridge id<MTLDevice>)graphicsContext }
            , commandQueue{ (__bridge id<MTLCommandQueue>)commandQueue }
            , getLayerRenderer{ [windowProvider{ std::move(windowProvider) }] { return (__bridge cp_layer_renderer_t)windowProvider(); } } {

            UpdateLayerRenderer();
        }

        ~Impl() {
            if (currentCommandBuffer != nil) {
                [currentCommandBuffer waitUntilCompleted];
            }

            if (ActiveFrameViews[0].ColorTexturePointer != nil) {
                id<MTLTexture> oldColorTexture = (__bridge_transfer id<MTLTexture>)ActiveFrameViews[0].ColorTexturePointer;
                [oldColorTexture setPurgeableState:MTLPurgeableStateEmpty];
                ActiveFrameViews[0].ColorTexturePointer = nil;
            }

            if (ActiveFrameViews[0].DepthTexturePointer != nil) {
                id<MTLTexture> oldDepthTexture = (__bridge_transfer id<MTLTexture>)ActiveFrameViews[0].DepthTexturePointer;
                [oldDepthTexture setPurgeableState:MTLPurgeableStateEmpty];
                ActiveFrameViews[0].DepthTexturePointer = nil;
            }

            Planes.clear();
            Meshes.clear();
        }

        void UpdateLayerRenderer() {
            cp_layer_renderer_t activeLayerRenderer = getLayerRenderer();
            if (activeLayerRenderer != SystemImpl.XrContext->LayerRenderer) {
                SystemImpl.XrContext->LayerRenderer = activeLayerRenderer;
                SystemImpl.XrContext->Initialized = (activeLayerRenderer != nullptr);
            }
        }

        arcana::task<void, std::exception_ptr> WhenReady() {
            __block arcana::task_completion_source<void, std::exception_ptr> tcs;
            CFRunLoopRef mainRunLoop = CFRunLoopGetMain();
            const auto intervalInSeconds = 0.033;
            CFRunLoopTimerRef timer = CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent(), intervalInSeconds, 0, 0, ^(CFRunLoopTimerRef timer){
                UpdateLayerRenderer();
                if (SystemImpl.XrContext->IsInitialized()) {
                    CFRunLoopRemoveTimer(mainRunLoop, timer, kCFRunLoopCommonModes);
                    CFRelease(timer);
                    tcs.complete();
                }
            });
            CFRunLoopAddTimer(mainRunLoop, timer, kCFRunLoopCommonModes);
            return tcs.as_task();
        }

        std::unique_ptr<System::Session::Frame> GetNextFrame(bool& shouldEndSession, bool& shouldRestartSession, std::function<arcana::task<void, std::exception_ptr>(void*)> deletedTextureAsyncCallback) {
            shouldEndSession = sessionEnded;
            shouldRestartSession = false;

            UpdateLayerRenderer();

            if (!SystemImpl.XrContext->IsInitialized()) {
                return std::make_unique<Frame>(*this);
            }

            cp_layer_renderer_t layerRenderer = SystemImpl.XrContext->LayerRenderer;
            cp_frame_t frame = [layerRenderer queryNextFrame];
            SystemImpl.XrContext->Frame = frame;

            if (frame == nil) {
                return std::make_unique<Frame>(*this);
            }

            // Simplified frame handling for visionOS (CompositorServices integration)
            // TODO: Implement proper cp_frame_timing_t integration when API is stable
            
            if (layerRenderer != nil) {
                // Use default texture size for now
                uint32_t width = 1920;
                uint32_t height = 1080;

                if (ActiveFrameViews[0].ColorTextureSize.Width != width || ActiveFrameViews[0].ColorTextureSize.Height != height) {
                    // Color texture
                    {
                        if (ActiveFrameViews[0].ColorTexturePointer != nil) {
                            id<MTLTexture> oldColorTexture = (__bridge_transfer id<MTLTexture>)ActiveFrameViews[0].ColorTexturePointer;
                            deletedTextureAsyncCallback(ActiveFrameViews[0].ColorTexturePointer).then(arcana::inline_scheduler, arcana::cancellation::none(), [oldColorTexture]() {
                                [oldColorTexture setPurgeableState:MTLPurgeableStateEmpty];
                            });
                            ActiveFrameViews[0].ColorTexturePointer = nil;
                        }

                        // Create a basic metal texture for now
                        MTLTextureDescriptor *colorDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
                        colorDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
                        id<MTLTexture> colorTexture = [metalDevice newTextureWithDescriptor:colorDesc];
                        ActiveFrameViews[0].ColorTexturePointer = (__bridge_retained void*)colorTexture;
                        ActiveFrameViews[0].ColorTextureFormat = TextureFormat::BGRA8_SRGB;
                        ActiveFrameViews[0].ColorTextureSize = {width, height};
                    }

                    // Depth texture
                    {
                        if (ActiveFrameViews[0].DepthTexturePointer != nil) {
                            id<MTLTexture> oldDepthTexture = (__bridge_transfer id<MTLTexture>)ActiveFrameViews[0].DepthTexturePointer;
                            deletedTextureAsyncCallback(ActiveFrameViews[0].DepthTexturePointer).then(arcana::inline_scheduler, arcana::cancellation::none(), [oldDepthTexture]() {
                                [oldDepthTexture setPurgeableState:MTLPurgeableStateEmpty];
                            });
                            ActiveFrameViews[0].DepthTexturePointer = nil;
                        }

                        // Create a basic depth texture for now  
                        MTLTextureDescriptor *depthDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:width height:height mipmapped:NO];
                        depthDesc.usage = MTLTextureUsageRenderTarget;
                        depthDesc.storageMode = MTLStorageModePrivate;
                        id<MTLTexture> depthTexture = [metalDevice newTextureWithDescriptor:depthDesc];
                        ActiveFrameViews[0].DepthTexturePointer = (__bridge_retained void*)depthTexture;
                        ActiveFrameViews[0].DepthTextureFormat = TextureFormat::D24S8;
                        ActiveFrameViews[0].DepthTextureSize = {width, height};
                    }
                }

                // Create basic view matrices for visionOS
                simd_float4x4 viewMatrix = matrix_identity_float4x4;
                simd_float4x4 projectionMatrix = matrix_identity_float4x4;
                
                // Set view pose
                auto viewPose = TransformToPose(simd_inverse(viewMatrix));
                ActiveFrameViews[0].Space.Pose = viewPose;
                
                // Set projection matrix
                memcpy(ActiveFrameViews[0].ProjectionMatrix.data(), projectionMatrix.columns, sizeof(float) * ActiveFrameViews[0].ProjectionMatrix.size());
                
                // Set depth range
                ActiveFrameViews[0].DepthNearZ = DepthNearZ;
                ActiveFrameViews[0].DepthFarZ = DepthFarZ;
            }

            return std::make_unique<Frame>(*this);
        }

        void RequestEndSession() {
            sessionEnded = true;
        }

        void DrawFrame() {
            // Simplified frame drawing for visionOS
            // TODO: Implement proper CompositorServices frame submission
        }

        void GetHitTestResults(std::vector<HitResult>& filteredResults, xr::Ray offsetRay, xr::HitTestTrackableType trackableTypes) const {
            // visionOS hit testing not implemented yet
        }

        xr::Anchor CreateAnchor(Pose pose) {
            // visionOS anchor creation not implemented yet
            return { { pose }, nullptr };
        }

        xr::Anchor DeclareAnchor(NativeAnchorPtr anchor) {
            // visionOS anchor declaration not implemented yet
            const auto pose = TransformToPose(simd_float4x4{});
            return { { pose }, anchor };
        }

        void UpdateAnchor(xr::Anchor& anchor) {
            // visionOS anchor updates not implemented yet
        }

        void DeleteAnchor(xr::Anchor& anchor) {
            // visionOS anchor deletion not implemented yet
        }

        void SetPlaneDetectionEnabled(bool enabled) {
            // visionOS plane detection not implemented yet
        }

        bool TrySetMeshDetectorEnabled(const bool enabled) {
            // visionOS mesh detection not implemented yet
            return false;
        }

        bool IsTracking() const {
            return SystemImpl.XrContext->IsInitialized();
        }

        std::vector<ImageTrackingScore>* GetImageTrackingScores() {
            return nullptr;
        }

        void CreateAugmentedImageDatabase(const std::vector<ImageTrackingRequest>& requests) {
            // visionOS image tracking not implemented yet
        }

    private:
        std::function<cp_layer_renderer_t()> getLayerRenderer{};
        bool sessionEnded{ false };
        id<MTLDevice> metalDevice{};
        id<MTLCommandQueue> commandQueue;
        id<MTLCommandBuffer> currentCommandBuffer;
    };

    struct System::Session::Frame::Impl {
    public:
        Impl(Session::Impl& sessionImpl)
            : sessionImpl{sessionImpl} { }

        Session::Impl& sessionImpl;
    };

    System::Session::Frame::Frame(Session::Impl& sessionImpl)
        : Views{ sessionImpl.ActiveFrameViews }
        , InputSources{ sessionImpl.InputSources}
        , FeaturePointCloud{ sessionImpl.FeaturePointCloud }
        , EyeTrackerSpace{ sessionImpl.EyeTrackerSpace }
        , UpdatedPlanes{}
        , RemovedPlanes{}
        , UpdatedMeshes{}
        , RemovedMeshes{}
        , UpdatedImageTrackingResults{}
        , IsTracking{sessionImpl.IsTracking()}
        , m_impl{ std::make_unique<System::Session::Frame::Impl>(sessionImpl) } {
        Views[0].DepthNearZ = sessionImpl.DepthNearZ;
        Views[0].DepthFarZ = sessionImpl.DepthFarZ;
    }

    System::Session::Frame::~Frame() {
    }

    void System::Session::Frame::Render() {
        m_impl->sessionImpl.DrawFrame();
    }

    void System::Session::Frame::GetHitTestResults(std::vector<HitResult>& filteredResults, xr::Ray offsetRay, xr::HitTestTrackableType trackableTypes) const {
        m_impl->sessionImpl.GetHitTestResults(filteredResults, offsetRay, trackableTypes);
    }

    Anchor System::Session::Frame::CreateAnchor(Pose pose, NativeTrackablePtr) const {
        return m_impl->sessionImpl.CreateAnchor(pose);
    }

    Anchor System::Session::Frame::DeclareAnchor(NativeAnchorPtr anchor) const {
        return m_impl->sessionImpl.DeclareAnchor(anchor);
    }

    void System::Session::Frame::UpdateAnchor(xr::Anchor& anchor) const {
        m_impl->sessionImpl.UpdateAnchor(anchor);
    }

    void System::Session::Frame::DeleteAnchor(xr::Anchor& anchor) const {
        m_impl->sessionImpl.DeleteAnchor(anchor);
    }

    System::Session::Frame::SceneObject& System::Session::Frame::GetSceneObjectByID(System::Session::Frame::SceneObject::Identifier) const {
        throw std::runtime_error("not implemented");
    }

    System::Session::Frame::Plane& System::Session::Frame::GetPlaneByID(System::Session::Frame::Plane::Identifier planeID) const {
        throw std::runtime_error("not implemented");
    }

    System::Session::Frame::ImageTrackingResult& System::Session::Frame::GetImageTrackingResultByID(System::Session::Frame::ImageTrackingResult::Identifier resultID) const {
        throw std::runtime_error("not implemented");
    }

    System::Session::Frame::Mesh& System::Session::Frame::GetMeshByID(System::Session::Frame::Mesh::Identifier meshID) const {
        throw std::runtime_error("not implemented");
    }

    System::System(const char* appName)
        : m_impl{ std::make_unique<System::Impl>(appName) } {}

    System::~System() {}

    bool System::IsInitialized() const {
        return m_impl->IsInitialized();
    }

    bool System::TryInitialize() {
        return m_impl->TryInitialize();
    }

    arcana::task<bool, std::exception_ptr> System::IsSessionSupportedAsync(SessionType sessionType) {
        return arcana::task_from_result<std::exception_ptr>(sessionType == SessionType::IMMERSIVE_VR);
    }

    uintptr_t System::GetNativeXrContext()
    {
        return reinterpret_cast<uintptr_t>(m_impl->XrContext.get());
    }

    std::string System::GetNativeXrContextType()
    {
        return "visionOS";
    }

    arcana::task<std::shared_ptr<System::Session>, std::exception_ptr> System::Session::CreateAsync(System& system, void* graphicsDevice, void* commandQueue, std::function<void*()> windowProvider) {
        auto session = std::make_shared<System::Session>(system, graphicsDevice, commandQueue, std::move(windowProvider));
        return session->m_impl->WhenReady().then(arcana::inline_scheduler, arcana::cancellation::none(), [session] {
            return session;
        });
    }

    System::Session::Session(System& system, void* graphicsDevice, void* commandQueue, std::function<void*()> windowProvider)
        : m_impl{ std::make_unique<System::Session::Impl>(*system.m_impl, graphicsDevice, commandQueue, std::move(windowProvider)) } {}

    System::Session::~Session() {
    }

    std::unique_ptr<System::Session::Frame> System::Session::GetNextFrame(bool& shouldEndSession, bool& shouldRestartSession, std::function<arcana::task<void, std::exception_ptr>(void*)> deletedTextureAsyncCallback) {
        return m_impl->GetNextFrame(shouldEndSession, shouldRestartSession, deletedTextureAsyncCallback);
    }

    void System::Session::RequestEndSession() {
        m_impl->RequestEndSession();
    }

    void System::Session::SetDepthsNearFar(float depthNear, float depthFar) {
        m_impl->DepthNearZ = depthNear;
        m_impl->DepthFarZ = depthFar;
    }

    void System::Session::SetPlaneDetectionEnabled(bool enabled) const
    {
        m_impl->SetPlaneDetectionEnabled(enabled);
    }

    bool System::Session::TrySetFeaturePointCloudEnabled(bool enabled) const
    {
        m_impl->FeaturePointCloudEnabled = enabled;
        return enabled;
    }

    bool System::Session::TrySetPreferredPlaneDetectorOptions(const GeometryDetectorOptions&)
    {
        return false;
    }

    bool System::Session::TrySetMeshDetectorEnabled(const bool enabled)
    {
        return m_impl->TrySetMeshDetectorEnabled(enabled);
    }

    bool System::Session::TrySetPreferredMeshDetectorOptions(const GeometryDetectorOptions&)
    {
        return false;
    }

    std::vector<ImageTrackingScore>* System::Session::GetImageTrackingScores() const
    {
        return m_impl->GetImageTrackingScores();
    }

    void System::Session::CreateAugmentedImageDatabase(const std::vector<System::Session::ImageTrackingRequest>& requests) const
    {
        m_impl->CreateAugmentedImageDatabase(requests);
    }
}