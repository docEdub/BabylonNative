import SwiftUI

class MetalView: UIView {
  override init(frame: CGRect) {
    super.init(frame: frame)
    self.backgroundColor = .clear
    // Add bright purple border for testing
    self.layer.borderColor = UIColor(red: 1.0, green: 0.0, blue: 1.0, alpha: 1.0).cgColor
    self.layer.borderWidth = 5.0
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
  @Binding var isImmersiveModeActive: Bool
  
  func makeUIView(context: Context) -> MetalView {
    MetalView(frame: .zero)
  }
  
  func updateUIView(_ uiView: MetalView, context: Context) {
    // Hide the view when immersive mode is active
    uiView.isHidden = isImmersiveModeActive
  }
}


@main
struct ExampleApp: App {
  @State private var isImmersiveModeActive = false
  
  var body: some Scene {
    WindowGroup {
      MetalViewRepresentable(isImmersiveModeActive: $isImmersiveModeActive)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .onReceive(NotificationCenter.default.publisher(for: .immersiveModeChanged)) { notification in
          if let isActive = notification.object as? Bool {
            isImmersiveModeActive = isActive
          }
        }
    }
    
    ImmersiveSpace(id: "ImmersiveSpace") {
      EmptyView()
    }
  }
}

extension Notification.Name {
  static let immersiveModeChanged = Notification.Name("immersiveModeChanged")
}
