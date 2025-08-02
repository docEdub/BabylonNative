@echo off
set JAVA_HOME=C:\Program Files\Microsoft\jdk-17.0.16.8-hotspot
set ANDROID_HOME=C:\Users\andyf\AppData\Local\Android\Sdk
set PATH=%JAVA_HOME%\bin;%ANDROID_HOME%\tools;%ANDROID_HOME%\platform-tools;%PATH%

cd /d "C:\-\code\BabylonNative\Apps\Playground\Android"
gradlew.bat assembleDebug

:: Check if build was successful
if %errorlevel% equ 0 (
    echo Build successful!
) else (
    echo Build failed with error code %errorlevel%
    pause
    exit /b %errorlevel%
)