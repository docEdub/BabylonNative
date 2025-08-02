# WebXR Android Development with Meta Quest 3

## Project Overview
Working on adding WebXR support to the Android Playground app for BabylonNative. Testing is done on a Meta Quest 3 device that is always connected and running.

## Development Workflow

### Build Commands
- `build_android.bat` - Builds the Android application
- `run_android.bat` - Deploys and runs the app on the connected Meta Quest 3

### Logging
- Output from `run_android.bat` is saved to the `log` folder
- Check logs for debugging WebXR functionality and device interactions

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

But these files exist only in `node_modules` and need to be copied to the Scripts directory.

**Solution**: Run `copy_babylon_scripts.bat` from the project root to copy the required files:
```bash
copy_babylon_scripts.bat
```

**Files copied**:
- `Apps/node_modules/babylonjs/babylon.max.js` → `Apps/Playground/Scripts/babylon.max.js`
- `Apps/node_modules/babylonjs-loaders/babylonjs.loaders.js` → `Apps/Playground/Scripts/babylonjs.loaders.js`
- `Apps/node_modules/babylonjs-materials/babylonjs.materials.js` → `Apps/Playground/Scripts/babylonjs.materials.js`
- `Apps/node_modules/babylonjs-gui/babylon.gui.js` → `Apps/Playground/Scripts/babylon.gui.js`

**Prevention**: After running `npm install` in the Apps directory, always run `copy_babylon_scripts.bat` to ensure the files are in the correct location.