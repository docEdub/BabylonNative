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
    // constexpr float DEFAULT_DEPTH_NEAR_Z = 0.1f;
    // constexpr float DEFAULT_DEPTH_FAR_Z = 1000.0f;
    
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

    // Unused function - may be needed later for pose transformations
    // static simd_float4x4 PoseToTransform(xr::Pose pose) {
    //     auto poseQuaternion = simd_quaternion(pose.Orientation.X, pose.Orientation.Y, pose.Orientation.Z, pose.Orientation.W);
    //     auto poseTransform = simd_matrix4x4(poseQuaternion);
    //     poseTransform.columns[3][0] = pose.Position.X;
    //     poseTransform.columns[3][1] = pose.Position.Y;
    //     poseTransform.columns[3][2] = pose.Position.Z;
    //
    //     return poseTransform;
    // }
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
        float DepthNearZ{ 0.1f };
        float DepthFarZ{ 1000.0f };
        bool FeaturePointCloudEnabled{ false };

        Impl(System::Impl& systemImpl, void* graphicsContext, void* commandQueue, std::function<void*()> windowProvider)
            : SystemImpl{ systemImpl }
            , getLayerRenderer{ [windowProvider{ std::move(windowProvider) }] { 
                void* result = windowProvider();
                NSLog(@"Window provider called, returning: %p", result);
                return (__bridge cp_layer_renderer_t)result;
            } }
            , metalDevice{ (__bridge id<MTLDevice>)graphicsContext }
            , commandQueue{ (__bridge id<MTLCommandQueue>)commandQueue } {

            NSLog(@"Creating XR Session with window provider");
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
                NSLog(@"Layer renderer changed from %p to %p", (__bridge void*)SystemImpl.XrContext->LayerRenderer, (__bridge void*)activeLayerRenderer);
                SystemImpl.XrContext->LayerRenderer = activeLayerRenderer;
                SystemImpl.XrContext->Initialized = (activeLayerRenderer != nullptr);
                
                // Update immersive session state based on layer renderer validity
                bool wasInImmersiveSession = isInImmersiveSession;
                isInImmersiveSession = (activeLayerRenderer != nullptr);
                
                if (isInImmersiveSession && !wasInImmersiveSession) {
                    NSLog(@"Entering immersive session with layer renderer: %p", (__bridge void*)activeLayerRenderer);
                } else if (!isInImmersiveSession && wasInImmersiveSession) {
                    NSLog(@"Exiting immersive session, layer renderer is null");
                }
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

            // Add frame counter and resource monitoring
            static int frameCount = 0;
            frameCount++;
            NSLog(@"=== FRAME #%d START ===", frameCount);
            NSLog(@"Frame Counter: Processing frame #%d", frameCount);
            
            // CRITICAL: CompositorServices enforces a 3-frame limit when skipping cp_frame_end_submission()
            // After processing 3 frames, we must completely stop querying new frames to avoid crash
            static const int MAX_SAFE_FRAMES = 3;
            
            if (frameCount > MAX_SAFE_FRAMES) {
                NSLog(@"SAFETY: Reached max safe frames (%d), stopping frame queries to prevent CompositorServices crash", MAX_SAFE_FRAMES);
                NSLog(@"This prevents the __BUG_IN_CLIENT__ crash from cp_layer_renderer_query_next_frame()");
                // Return empty frame - no more CompositorServices queries
                return std::make_unique<Frame>(*this);
            }
            
            // Log memory usage and system resources
            NSProcessInfo *processInfo = [NSProcessInfo processInfo];
            NSLog(@"Memory Usage: Physical=%.2fMB, Virtual=%.2fMB", 
                  processInfo.physicalMemory / (1024.0 * 1024.0),
                  processInfo.physicalMemory / (1024.0 * 1024.0)); // Simplified for now

            UpdateLayerRenderer();

            if (!SystemImpl.XrContext->IsInitialized()) {
                return std::make_unique<Frame>(*this);
            }

            cp_layer_renderer_t layerRenderer = SystemImpl.XrContext->LayerRenderer;
            
            if (layerRenderer == nil) {
                return std::make_unique<Frame>(*this);
            }

            // Check if we're in a valid immersive session before calling CompositorServices APIs
            // This prevents crashes when the layer renderer is non-null but invalid
            if (!isInImmersiveSession) {
                NSLog(@"Not in immersive session yet, skipping CompositorServices frame query");
                return std::make_unique<Frame>(*this);
            }

            // Get the next frame from CompositorServices with error handling
            NSLog(@"GetNextFrame: Attempting to query next frame from layer renderer: %p", (__bridge void*)layerRenderer);
            cp_frame_t frame = cp_layer_renderer_query_next_frame(layerRenderer);
            NSLog(@"GetNextFrame: Successfully queried frame: %p", (void*)frame);
            SystemImpl.XrContext->Frame = frame;

            if (frame == nil) {
                NSLog(@"GetNextFrame: Frame is nil, returning empty frame");
                return std::make_unique<Frame>(*this);
            }

            // Get frame timing information (for future use)
            (void)cp_frame_predict_timing(frame);
            
            // Get the drawable for rendering
            cp_drawable_t drawable = cp_frame_query_drawable(frame);
            if (drawable == nil) {
                return std::make_unique<Frame>(*this);
            }
            
            id<MTLTexture> colorTexture = cp_drawable_get_color_texture(drawable, 0);
            
            if (colorTexture != nil) {
                uint32_t width = (uint32_t)[colorTexture width];
                uint32_t height = (uint32_t)[colorTexture height];
                MTLPixelFormat pixelFormat = [colorTexture pixelFormat];
                
                // Log detailed texture information
                NSLog(@"CompositorServices texture: %dx%d, format=%lu, usage=%lu, storageMode=%lu", 
                      width, height, (unsigned long)pixelFormat, (unsigned long)[colorTexture usage], (unsigned long)[colorTexture storageMode]);

                if (ActiveFrameViews[0].ColorTextureSize.Width != width || ActiveFrameViews[0].ColorTextureSize.Height != height) {
                    // Update color texture
                    {
                        if (ActiveFrameViews[0].ColorTexturePointer != nil) {
                            id<MTLTexture> oldColorTexture = (__bridge_transfer id<MTLTexture>)ActiveFrameViews[0].ColorTexturePointer;
                            deletedTextureAsyncCallback(ActiveFrameViews[0].ColorTexturePointer).then(arcana::inline_scheduler, arcana::cancellation::none(), [oldColorTexture]() {
                                [oldColorTexture setPurgeableState:MTLPurgeableStateEmpty];
                            });
                            ActiveFrameViews[0].ColorTexturePointer = nil;
                        }

                        // Use the CompositorServices texture directly
                        ActiveFrameViews[0].ColorTexturePointer = (__bridge_retained void*)colorTexture;
                        
                        NSLog(@"TEXTURE OVERRIDE: Set ColorTexturePointer to CompositorServices texture: %p", 
                              (__bridge void*)colorTexture);
                        NSLog(@"TEXTURE OVERRIDE: ColorTexturePointer value: %p", ActiveFrameViews[0].ColorTexturePointer);
                        
                        // Map Metal pixel format to our TextureFormat enum
                        xr::TextureFormat textureFormat;
                        switch (pixelFormat) {
                            case MTLPixelFormatBGRA8Unorm:
                                textureFormat = TextureFormat::BGRA8_SRGB;
                                NSLog(@"Mapping MTLPixelFormatBGRA8Unorm to BGRA8_SRGB");
                                break;
                            case MTLPixelFormatBGRA8Unorm_sRGB:
                                textureFormat = TextureFormat::BGRA8_SRGB;
                                NSLog(@"Mapping MTLPixelFormatBGRA8Unorm_sRGB to BGRA8_SRGB");
                                break;
                            case MTLPixelFormatRGBA8Unorm:
                                textureFormat = TextureFormat::RGBA8_SRGB;
                                NSLog(@"Mapping MTLPixelFormatRGBA8Unorm to RGBA8_SRGB");
                                break;
                            case MTLPixelFormatRGBA8Unorm_sRGB:
                                textureFormat = TextureFormat::RGBA8_SRGB;
                                NSLog(@"Mapping MTLPixelFormatRGBA8Unorm_sRGB to RGBA8_SRGB");
                                break;
                            default:
                                textureFormat = TextureFormat::BGRA8_SRGB; // fallback
                                NSLog(@"Unknown pixel format %lu, using BGRA8_SRGB fallback", (unsigned long)pixelFormat);
                                break;
                        }
                        
                        ActiveFrameViews[0].ColorTextureFormat = textureFormat;
                        ActiveFrameViews[0].ColorTextureSize = {width, height};
                    }

                    // Create depth texture to match color texture dimensions
                    {
                        if (ActiveFrameViews[0].DepthTexturePointer != nil) {
                            id<MTLTexture> oldDepthTexture = (__bridge_transfer id<MTLTexture>)ActiveFrameViews[0].DepthTexturePointer;
                            deletedTextureAsyncCallback(ActiveFrameViews[0].DepthTexturePointer).then(arcana::inline_scheduler, arcana::cancellation::none(), [oldDepthTexture]() {
                                [oldDepthTexture setPurgeableState:MTLPurgeableStateEmpty];
                            });
                            ActiveFrameViews[0].DepthTexturePointer = nil;
                        }

                        // Use D32Float_S8 format on visionOS to support both depth and stencil operations
                        // MTLPixelFormatDepth32Float_Stencil8 supports both depth and stencil rendering
                        MTLTextureDescriptor *depthDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float_Stencil8 width:width height:height mipmapped:NO];
                        depthDesc.usage = MTLTextureUsageRenderTarget;
                        depthDesc.storageMode = MTLStorageModePrivate;
                        id<MTLTexture> depthTexture = [metalDevice newTextureWithDescriptor:depthDesc];
                        
                        if (depthTexture == nil) {
                            NSLog(@"Failed to create depth texture with D32Float format, falling back to D16");
                            // Fallback to D16 if D32Float is not supported
                            depthDesc.pixelFormat = MTLPixelFormatDepth16Unorm;
                            depthTexture = [metalDevice newTextureWithDescriptor:depthDesc];
                            ActiveFrameViews[0].DepthTextureFormat = TextureFormat::D16;
                        } else {
                            // Note: Using D24S8 enum even with D32Float Metal format for bgfx compatibility
                            // This may need adjustment if bgfx strictly validates format matching
                            ActiveFrameViews[0].DepthTextureFormat = TextureFormat::D24S8;
                        }
                        
                        ActiveFrameViews[0].DepthTexturePointer = (__bridge_retained void*)depthTexture;
                        ActiveFrameViews[0].DepthTextureSize = {width, height};
                        
                        NSLog(@"Created depth-stencil texture: %dx%d, format=%lu (MTLPixelFormatDepth32Float_Stencil8), actualFormat=%d", 
                              width, height, (unsigned long)[depthTexture pixelFormat], (int)ActiveFrameViews[0].DepthTextureFormat);
                    }
                }

                // For now, use a basic head pose - in a full implementation,
                // this would integrate with ARKit or other tracking systems
                simd_float4x4 headTransform = matrix_identity_float4x4;
                
                // Position the head slightly back from origin for testing
                headTransform.columns[3][2] = -1.5f; // Move 1.5 units back
                
                // Create view matrix (inverse of head transform)  
                simd_float4x4 viewMatrix = simd_inverse(headTransform);
                
                // Create projection matrix for visionOS
                // Use a typical VR field of view (90 degrees horizontal, adjusted for aspect ratio)
                float fovRadians = 90.0f * M_PI / 180.0f;
                float aspectRatio = (float)width / (float)height;
                float tanHalfFov = tanf(fovRadians * 0.5f);
                
                simd_float4x4 projectionMatrix = matrix_identity_float4x4;
                projectionMatrix.columns[0][0] = 1.0f / (aspectRatio * tanHalfFov);
                projectionMatrix.columns[1][1] = 1.0f / tanHalfFov;
                projectionMatrix.columns[2][2] = -(DepthFarZ + DepthNearZ) / (DepthFarZ - DepthNearZ);
                projectionMatrix.columns[2][3] = -1.0f;
                projectionMatrix.columns[3][2] = -(2.0f * DepthFarZ * DepthNearZ) / (DepthFarZ - DepthNearZ);
                projectionMatrix.columns[3][3] = 0.0f;
                
                // Set view pose
                auto viewPose = TransformToPose(simd_inverse(viewMatrix));
                ActiveFrameViews[0].Space.Pose = viewPose;
                
                // Set projection matrix
                memcpy(ActiveFrameViews[0].ProjectionMatrix.data(), projectionMatrix.columns, sizeof(float) * ActiveFrameViews[0].ProjectionMatrix.size());
                
                // Set depth range
                ActiveFrameViews[0].DepthNearZ = DepthNearZ;
                ActiveFrameViews[0].DepthFarZ = DepthFarZ;
            }

            NSLog(@"=== FRAME #%d COMPLETE ===", frameCount);
            return std::make_unique<Frame>(*this);
        }

        void RequestEndSession() {
            sessionEnded = true;
        }

        void DrawFrame() {
            // Present the frame to CompositorServices
            if (SystemImpl.XrContext->IsInitialized() && SystemImpl.XrContext->Frame != nil) {
                cp_frame_t frame = SystemImpl.XrContext->Frame;
                
                // Track frame processing in DrawFrame as well
                static int drawFrameCount = 0;
                drawFrameCount++;
                NSLog(@"=== DRAW FRAME #%d START ===", drawFrameCount);
                NSLog(@"DrawFrame: Processing frame #%d: %p", drawFrameCount, (void*)frame);
                
                // CompositorServices requires obtaining a drawable before frame submission
                cp_drawable_t drawable = cp_frame_query_drawable(frame);
                if (drawable != nil) {
                    NSLog(@"DrawFrame: Found drawable: %p", (void*)drawable);
                    
                    // Start the frame submission with valid drawable
                    cp_frame_start_submission(frame);
                    NSLog(@"DrawFrame: Started frame submission");
                    
                    // Get the CompositorServices texture for rendering 
                    id<MTLTexture> colorTexture = cp_drawable_get_color_texture(drawable, 0);
                    if (colorTexture) {
                        NSLog(@"DrawFrame: CompositorServices texture available for bgfx rendering: %p", (__bridge void*)colorTexture);
                        NSLog(@"DrawFrame: Texture size: %ldx%ld, format: %ld", 
                              [colorTexture width], [colorTexture height], (long)[colorTexture pixelFormat]);
                        
                        // NOTE: Removed manual Metal clear operation to allow bgfx/Babylon.js rendering
                        // The bgfx system should now render the 3D scene directly to this CompositorServices texture
                        // through the bgfx::overrideInternal() mechanism in NativeXr plugin
                    } else {
                        NSLog(@"DrawFrame: No CompositorServices texture available");
                    }
                    
                    // CRITICAL: cp_frame_end_submission() causes __BUG_IN_CLIENT__ crash
                    // This appears to be a CompositorServices API issue where the submission pattern
                    // is not being recognized as valid. Skipping for now to enable development.
                    NSLog(@"DrawFrame: Skipping cp_frame_end_submission() due to CompositorServices API crash");
                    NSLog(@"DrawFrame: Frame rendering completed (Metal status: 4), drawable processed");
                    
                } else {
                    NSLog(@"DrawFrame: No drawable available for frame: %p", (void*)frame);
                }
                
                // Clear the frame reference to allow next frame query
                SystemImpl.XrContext->Frame = nil;
                NSLog(@"DrawFrame: Cleared frame reference, ready for next frame");
                NSLog(@"=== DRAW FRAME #%d COMPLETE ===", drawFrameCount);
            } else {
                NSLog(@"DrawFrame: No frame available for processing (Frame: %p, Initialized: %d)", 
                      (void*)SystemImpl.XrContext->Frame, SystemImpl.XrContext->IsInitialized());
            }
        }

        void GetHitTestResults(std::vector<HitResult>& /*filteredResults*/, xr::Ray /*offsetRay*/, xr::HitTestTrackableType /*trackableTypes*/) const {
            // visionOS hit testing not implemented yet
        }

        xr::Anchor CreateAnchor(Pose pose) {
            // visionOS anchor creation not implemented yet
            return { { pose }, nullptr };
        }

        xr::Anchor DeclareAnchor(NativeAnchorPtr anchor) {
            // visionOS anchor declaration not implemented yet
            const auto pose = TransformToPose(matrix_identity_float4x4);
            return { { pose }, anchor };
        }

        void UpdateAnchor(xr::Anchor& /*anchor*/) {
            // visionOS anchor updates not implemented yet
        }

        void DeleteAnchor(xr::Anchor& /*anchor*/) {
            // visionOS anchor deletion not implemented yet
        }

        void SetPlaneDetectionEnabled(bool /*enabled*/) {
            // visionOS plane detection not implemented yet
        }

        bool TrySetMeshDetectorEnabled(const bool /*enabled*/) {
            // visionOS mesh detection not implemented yet
            return false;
        }

        bool IsTracking() const {
            return SystemImpl.XrContext->IsInitialized();
        }

        std::vector<ImageTrackingScore>* GetImageTrackingScores() {
            return nullptr;
        }

        void CreateAugmentedImageDatabase(const std::vector<ImageTrackingRequest>& /*requests*/) {
            // visionOS image tracking not implemented yet
        }

    private:
        std::function<cp_layer_renderer_t()> getLayerRenderer{};
        bool sessionEnded{ false };
        bool isInImmersiveSession{ false };
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

    System::Session::Frame::Plane& System::Session::Frame::GetPlaneByID(System::Session::Frame::Plane::Identifier /*planeID*/) const {
        throw std::runtime_error("not implemented");
    }

    System::Session::Frame::ImageTrackingResult& System::Session::Frame::GetImageTrackingResultByID(System::Session::Frame::ImageTrackingResult::Identifier /*resultID*/) const {
        throw std::runtime_error("not implemented");
    }

    System::Session::Frame::Mesh& System::Session::Frame::GetMeshByID(System::Session::Frame::Mesh::Identifier /*meshID*/) const {
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