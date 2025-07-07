import SwiftUI
import CompositorServices

class MetalView: UIView {
  override init(frame: CGRect) {
    super.init(frame: frame)
    self.backgroundColor = .clear
  }
  
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }
  
  func setupMetalLayer() {
    guard let bridge = LibNativeBridge.sharedInstance() else { return }
    
    if bridge.metalLayer != nil {
      return
    }
    
    self.addGestureRecognizer(
      UIBabylonGestureRecognizer(
        target: self,
        onTouchDown: bridge.setTouchDown,
        onTouchMove: bridge.setTouchMove,
        onTouchUp: bridge.setTouchUp
      )
    )
    metalLayer.pixelFormat = .bgra8Unorm
    metalLayer.framebufferOnly = true
    
    bridge.metalLayer = self.metalLayer
    
    let scale = UITraitCollection.current.displayScale
    bridge.initialize(withWidth: Int(self.bounds.width * scale), height: Int(self.bounds.height * scale))
  }
  
  var metalLayer: CAMetalLayer {
    return layer as! CAMetalLayer
  }
  
  override class var layerClass: AnyClass {
    return CAMetalLayer.self
  }
  
  override func layoutSubviews() {
    super.layoutSubviews()
    setupMetalLayer()
    updateDrawableSize()
  }
  
  private func updateDrawableSize() {
    let scale = UITraitCollection.current.displayScale
    LibNativeBridge.sharedInstance().drawableWillChangeSize(withWidth: Int(bounds.width * scale), height: Int(bounds.height * scale))
    metalLayer.drawableSize = CGSize(width: bounds.width * scale, height: bounds.height * scale)
  }
}

struct MetalViewRepresentable: UIViewRepresentable {
  typealias UIViewType = MetalView
  
  func makeUIView(context: Context) -> MetalView {
    let view = MetalView(frame: .zero)
    // Add bright purple border for testing
    view.layer.borderColor = UIColor.systemPurple.cgColor
    view.layer.borderWidth = 5.0
    return view
  }
  
  func updateUIView(_ uiView: MetalView, context: Context) {}
}

struct ImmersiveView: View {
    var body: some View {
        Text("Immersive Mode Active")
            .font(.largeTitle)
            .foregroundColor(.green)
            .onAppear {
                // For now, we'll simulate immersive mode without CompositorLayer
                // This can be expanded once the visionOS CompositorServices API is stable
                if let bridge = LibNativeBridge.sharedInstance() {
                    bridge.setupImmersiveMode(withLayerRenderer: nil)
                    
                    let renderLoop = RenderLoop(bridge: bridge)
                    renderLoop.start()
                }
            }
    }
}

class RenderLoop {
    private var bridge: LibNativeBridge
    private var displayLink: CADisplayLink?
    
    init(bridge: LibNativeBridge) {
        self.bridge = bridge
    }
    
    func start() {
        displayLink = CADisplayLink(target: self, selector: #selector(render))
        displayLink?.add(to: .main, forMode: .default)
    }
    
    func stop() {
        displayLink?.invalidate()
        displayLink = nil
    }
    
    @objc private func render() {
        bridge.renderImmersiveFrame()
    }
    
    deinit {
        stop()
    }
}

@main
struct ExampleApp: App {
  @State private var isImmersive = false
  
  var body: some Scene {
    WindowGroup {
      VStack {
        MetalViewRepresentable()
          .frame(maxWidth: .infinity, maxHeight: .infinity)
        
        Button("Enter Immersive Mode") {
          isImmersive = true
        }
        .padding()
      }
    }
    .windowStyle(.plain)
    
    ImmersiveSpace(id: "ImmersiveSpace") {
      ImmersiveView()
        .onAppear {
          // Hide the main window when entering immersive mode
          if let windowScene = UIApplication.shared.connectedScenes.first as? UIWindowScene {
            windowScene.windows.forEach { window in
              window.isHidden = true
            }
          }
        }
        .onDisappear {
          // Show the main window when exiting immersive mode
          if let windowScene = UIApplication.shared.connectedScenes.first as? UIWindowScene {
            windowScene.windows.forEach { window in
              window.isHidden = false
            }
          }
          isImmersive = false
        }
    }
    .immersionStyle(selection: .constant(.full), in: .full)
    .onChange(of: isImmersive) { _, newValue in
      if newValue {
        Task {
          // await openImmersiveSpace(id: "ImmersiveSpace")
        }
      }
    }
  }
}
