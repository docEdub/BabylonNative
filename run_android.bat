@echo off
setlocal enabledelayedexpansion

echo ========================================
echo  BabylonNative Android Playground Deployer
echo ========================================

:: Check if local.properties exists to get Android SDK path
if not exist "Apps\Playground\Android\local.properties" (
    echo ERROR: local.properties not found in Apps\Playground\Android\
    echo Please make sure Android SDK is configured properly.
    pause
    exit /b 1
)

:: Extract Android SDK path from local.properties
for /f "tokens=2 delims==" %%i in ('findstr "sdk.dir" Apps\Playground\Android\local.properties') do (
    set "ANDROID_SDK=%%i"
)

:: Remove escaped backslashes (replace double backslash with single)
set "ANDROID_SDK=%ANDROID_SDK:\\=\%"
echo Debug: Raw SDK path: %ANDROID_SDK%
:: set "ADB_PATH=%ANDROID_SDK%\platform-tools\adb.exe"
set "ADB_PATH=C:\Program Files\Meta Quest Developer Hub\resources\bin\adb.exe"

echo Using Android SDK: %ANDROID_SDK%

:: Check if ADB exists
if not exist "%ADB_PATH%" (
    echo ERROR: ADB not found at %ADB_PATH%
    echo Please install Android SDK platform-tools.
    pause
    exit /b 1
)

:: Check if APK exists
set "APK_PATH=Apps\Playground\Android\app\build\outputs\apk\debug\app-debug.apk"
if not exist "%APK_PATH%" (
    echo ERROR: APK not found at %APK_PATH%
    echo Please build the Android project first using build_android.bat
    pause
    exit /b 1
)

echo Checking connected devices...
"%ADB_PATH%" devices

:: Check if any device is connected
for /f "skip=1 tokens=2" %%i in ('"%ADB_PATH%" devices 2^>nul') do (
    if "%%i"=="device" (
        set "DEVICE_FOUND=1"
        goto :device_found
    )
    if "%%i"=="unauthorized" (
        echo WARNING: Device found but unauthorized. Please accept USB debugging on your Meta Quest 3.
        pause
        goto :retry_devices
    )
)

echo ERROR: No authorized devices found.
echo Please ensure:
echo 1. Meta Quest 3 is connected via USB
echo 2. Developer mode is enabled on Meta Quest 3
echo 3. USB debugging is authorized
pause
exit /b 1

:retry_devices
echo Checking devices again...
"%ADB_PATH%" devices
for /f "skip=1 tokens=2" %%i in ('"%ADB_PATH%" devices 2^>nul') do (
    if "%%i"=="device" (
        set "DEVICE_FOUND=1"
        goto :device_found
    )
)
echo Device still not authorized. Please check your Meta Quest 3.
pause
exit /b 1

:device_found
echo Device found and authorized!

:: Check if app is already installed and compare APK sizes
echo Checking if app is already installed...
"%ADB_PATH%" shell pm list packages com.android.babylonnative.playground >nul 2>&1
if errorlevel 1 (
    echo App not installed, proceeding with installation...
    goto :install_apk
)

echo App is installed, checking if APK needs updating...

:: Get installed APK path
for /f "tokens=*" %%i in ('"%ADB_PATH%" shell pm path com.android.babylonnative.playground 2^>nul') do (
    set "DEVICE_APK_PATH=%%i"
)
set "DEVICE_APK_PATH=%DEVICE_APK_PATH:package:=%"

:: Generate hash of local APK
echo Generating hash of local APK...
for /f %%i in ('powershell -Command "Get-FileHash '%APK_PATH%' -Algorithm SHA256 | Select-Object -ExpandProperty Hash"') do set "LOCAL_HASH=%%i"

:: Generate hash of device APK
echo Generating hash of device APK...
"%ADB_PATH%" shell sha256sum "%DEVICE_APK_PATH%" > temp_device_hash.txt 2>nul
for /f "tokens=1" %%i in (temp_device_hash.txt) do set "DEVICE_HASH=%%i"
del temp_device_hash.txt >nul 2>&1

echo Local APK hash:  %LOCAL_HASH%
echo Device APK hash: %DEVICE_HASH%

if "%LOCAL_HASH%"=="%DEVICE_HASH%" (
    echo APK hashes match - skipping installation
    goto :launch_app
) else (
    echo APK hashes differ - updating installation...
    goto :install_apk
)

:install_apk
echo Installing APK...
"%ADB_PATH%" install -r "%APK_PATH%"
if errorlevel 1 (
    echo ERROR: Failed to install APK
    pause
    exit /b 1
)

echo APK installed successfully!

:launch_app

echo Launching Playground app...
"%ADB_PATH%" shell am start -n com.android.babylonnative.playground/.PlaygroundActivity
if errorlevel 1 (
    echo ERROR: Failed to launch app
    pause
    exit /b 1
)

echo ========================================
echo  Playground app launched successfully!
echo  Check your Meta Quest 3 headset.
echo ========================================

:: Show logs and save to file
echo.
echo Starting logcat... Press Ctrl+C to stop.
echo Logs will be saved to android_logs.txt
echo ========================================

:: Create logs directory if it doesn't exist
if not exist "logs" mkdir logs

:: Generate timestamp for log filename
for /f "tokens=2 delims==" %%a in ('wmic OS Get localdatetime /value') do set "dt=%%a"
set "YY=%dt:~2,2%" & set "YYYY=%dt:~0,4%" & set "MM=%dt:~4,2%" & set "DD=%dt:~6,2%"
set "HH=%dt:~8,2%" & set "Min=%dt:~10,2%" & set "Sec=%dt:~12,2%"
set "datestamp=%YYYY%-%MM%-%DD%_%HH%-%Min%-%Sec%"

:: Start logcat with tee-like functionality using PowerShell
powershell -Command "& { $logFile = 'logs\android_logs_%datestamp%.txt'; Write-Host \"Logging to: $logFile\"; & '%ADB_PATH%' logcat -s BabylonNative | Tee-Object -FilePath $logFile }"
