@echo off
echo Copying Babylon.js files from node_modules to Scripts directory...

copy "Apps\node_modules\babylonjs\babylon.max.js" "Apps\Playground\Scripts\babylon.max.js"
copy "Apps\node_modules\babylonjs-loaders\babylonjs.loaders.js" "Apps\Playground\Scripts\babylonjs.loaders.js"
copy "Apps\node_modules\babylonjs-materials\babylonjs.materials.js" "Apps\Playground\Scripts\babylonjs.materials.js"
copy "Apps\node_modules\babylonjs-gui\babylon.gui.js" "Apps\Playground\Scripts\babylon.gui.js"

echo Done! Babylon.js files have been copied to Scripts directory.