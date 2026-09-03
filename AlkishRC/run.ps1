# Builds the app from the current www/ source and runs it on a connected
# Android device or a running emulator.
#
# Usage:
#   1. Start an emulator first (or plug in a device), e.g.:
#        emulator -avd Medium_Phone_API_35
#   2. From this folder, run:  .\run.ps1

$ErrorActionPreference = "Stop"

nvm use 24.20.0 | Out-Null
$env:JAVA_HOME = "C:\jdk-17"
$env:Path = "C:\Users\Pc\AppData\Local\Android\Sdk\platform-tools;C:\Users\Pc\AppData\Local\Android\Sdk\emulator;$env:Path"

Set-Location $PSScriptRoot

$devices = (adb devices) -split "`n" | Select-String "\tdevice$"
if (-not $devices) {
    Write-Host "No device/emulator connected. Start one first, e.g.:"
    Write-Host "  emulator -avd Medium_Phone_API_35"
    exit 1
}

Write-Host "Syncing www/ into the Android project..."
npx cap sync android

Write-Host "Building debug APK..."
Push-Location android
.\gradlew.bat assembleDebug
Pop-Location

Write-Host "Installing on device..."
adb install -r android\app\build\outputs\apk\debug\app-debug.apk

Write-Host "Launching app..."
adb shell am start -n com.alkish.rccontroller/.SplashActivity

Write-Host "Done."
