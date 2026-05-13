# Dusk VR - Phase 3 (Stereo Rendering) Setup & Testing Guide

This version of Dusk VR now supports **full 3D Stereo Rendering** and **Head Tracking**.

## 1. Required Software
*   **Vulkan SDK:** [Download here](https://vulkan.lunarg.com/sdk/home#windows). **Install this before building.**
*   **Visual Studio 2022:** With "Desktop development with C++" workload.
*   **OpenXR Runtime:** SteamVR or Oculus App.

## 2. Getting the Latest Changes
On your Windows machine, open a terminal in your project folder and run:
```powershell
# Reset to match the GitHub version exactly
git fetch origin
git reset --hard origin/main
git submodule update --init --recursive
```

## 3. Building for VR
1.  Open **Visual Studio 2022**.
2.  Ensure the configuration is set to **`windows-msvc-debug`**.
3.  **Wait for CMake to Finish:** Look at the "Output" window at the bottom. It should say "CMake generation finished."
4.  Go to **Build > Build All**.

## 4. Starting in VR
1.  **IMPORTANT:** Ensure your headset is connected and your OpenXR runtime (SteamVR or Oculus) is already running before launching the game.
2.  Open a terminal and navigate to:
    `cd build\windows-msvc-debug`
3.  Run the game with the ISO path:
    `.\dusk.exe "C:\Path\To\Your\TwilightPrincess.iso"`

---
**Status:** Phase 3 (Stereo Rendering) Complete.
**Next Phase:** Phase 4 (VR Motion Controls / Input).
