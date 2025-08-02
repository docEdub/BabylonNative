# WebXR Android Development with Meta Quest 3

## Project Overview
Working on adding WebXR support to the Android Playground app for BabylonNative. Testing is done on a Meta Quest 3 device that is always connected and running.

## Development Workflow

### Build Commands
- `build_android.bat` - Builds the Android application
- `run_android.bat` - Deploys and runs the app on the connected Meta Quest 3

### Logging
- Output from `run_android.bat` is saved to the `.logs` folder
- Check logs for debugging WebXR functionality and device interactions
- Screenshots are automatically captured when the app is stopped (Ctrl+C)
- Screenshots show the Meta Quest 3's stereoscopic display with both left and right eye views at an angle
- These angled dual-eye screenshots can be used to verify if the app successfully entered WebXR immersive mode
- Screenshot files use the same naming as log files: `.logs\android_logs_YYYY-MM-DD_HH-MM-SS.png`

### Screenshot Examples
- **Non-immersive (Windowed) View**: See `.example-screenshots/example-windowed-view.png` - shows a single centered window view instead of a fully immersed WebXR view, with the angled left and right eye displays side by side.
- **Immersive WebXR View**: When successfully entering WebXR immersive mode, the screenshot will show the angled left and right eye displays side by side

### Testing Device
- **Device**: Meta Quest 3
- **Status**: Always connected and running
- **Use**: Primary testing target for WebXR features

## Key Areas
- Android Playground app integration
- WebXR API implementation
- Meta Quest 3 compatibility
- Performance optimization for VR

## Development Tips
- Always test on the actual Meta Quest 3 device
- Monitor logs in the `log` folder for debugging
- Use the provided batch files for consistent build/deploy workflow

## Common Issues & Solutions

### "BABYLON is not defined" Error
**Issue**: ReferenceError: BABYLON is not defined at app:///Scripts/experience.js:44:16

**Root Cause**: Babylon.js library files are missing from the `Apps/Playground/Scripts/` directory. The C++ code in `BabylonNativeJNI.cpp` (lines 133-138) tries to load:
- `babylon.max.js`
- `babylonjs.loaders.js` 
- `babylonjs.materials.js`
- `babylon.gui.js`

But these files exist only in `node_modules` and need to be copied to the Scripts directory. Do this in the CLI without creating a script to copy the files.
