@echo off
echo This is a test
echo Current directory: %CD%
echo APK exists: 
if exist "Apps\Playground\Android\app\build\outputs\apk\debug\app-debug.apk" (echo YES) else (echo NO)