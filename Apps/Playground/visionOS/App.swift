import SwiftUI
import CompositorServices

class MetalView: UIView {
  override init(frame: CGRect) {
    super.init(frame: frame)
    // Set bright purple background to verify it's hidden in immersive mode
    self.backgroundColor = .systemPurple
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
    MetalView(frame: .zero)
  }
  
  func updateUIView(_ uiView: MetalView, context: Context) {}
}


@main
struct ExampleApp: App {
  @State private var showImmersiveView = false
  
  var body: some Scene {
    WindowGroup {
      MetalViewRepresentable()
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
    .windowStyle(.plain)
    
    ImmersiveSpace(id: "ImmersiveSpace") {
      // For now, simple immersive space that uses the same rendering
      MetalViewRepresentable()
        .onAppear {
          // Hide the main window when entering immersive mode
          // The purple border should not be visible in immersive mode
        }
    }
    .immersionStyle(selection: .constant(.full), in: .full)
  }
}
