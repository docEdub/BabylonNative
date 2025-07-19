import SwiftUI
import CompositorServices
import _CompositorServices_SwiftUI

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

struct ImmersiveView: ImmersiveSpaceContent {
    var body: some ImmersiveSpaceContent {
        CompositorLayer(configuration: .default) { layerRenderer in
            if let bridge = LibNativeBridge.sharedInstance() {
                bridge.setupImmersiveMode(withLayerRenderer: layerRenderer)
                
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
  @Environment(\.openImmersiveSpace) private var openImmersiveSpace
  @Environment(\.dismissImmersiveSpace) private var dismissImmersiveSpace
  
  var body: some Scene {
    WindowGroup {
      ContentView(isImmersive: $isImmersive, openImmersiveSpace: openImmersiveSpace)
    }
    .windowStyle(.plain)
    
    ImmersiveSpace(id: "ImmersiveSpace") {
      ImmersiveView()
    }
    .immersionStyle(selection: .constant(.full), in: .full)
  }
}

struct ContentView: View {
  @Binding var isImmersive: Bool
  let openImmersiveSpace: OpenImmersiveSpaceAction
  
  var body: some View {
    VStack {
      MetalViewRepresentable()
        .frame(maxWidth: .infinity, maxHeight: .infinity)
      
      Button("Enter Immersive Mode") {
        Task {
          await openImmersiveSpace(id: "ImmersiveSpace")
          isImmersive = true
        }
      }
      .padding()
    }
    .opacity(isImmersive ? 0.0 : 1.0)
    .animation(.easeInOut(duration: 0.3), value: isImmersive)
  }
}
