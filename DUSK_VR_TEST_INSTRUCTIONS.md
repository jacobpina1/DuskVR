# Dusk VR - Windows Testing Instructions

You have successfully pulled the initial VR integration for Dusk!
This version includes **OpenXR Head Tracking** and **Camera Injection**.

### 1. Build Instructions
1.  Open **Visual Studio 2022**.
2.  Select **"Open a local folder"** and navigate to your `DuskVR` repository.
3.  In the top toolbar, set the Configuration to **`windows-msvc-debug`** (or `windows-msvc-relwithdebinfo` for better performance).
4.  Wait for CMake generation to complete (it will automatically download the OpenXR SDK).
5.  Go to **Build > Build All**.

### 2. Setup VR Runtime
Ensure your VR headset is connected and your preferred OpenXR runtime is active:
*   **SteamVR:** Settings > Advanced > Developer > Set SteamVR as active OpenXR Runtime.
*   **Oculus/Meta:** Settings > General > OpenXR Runtime > Set as active.

### 3. How to Test
1.  Open a terminal (PowerShell or Command Prompt).
2.  Navigate to the build output directory:
    `cd build\windows-msvc-debug`
3.  Run the game with your Twilight Princess ISO:
    `.\dusk.exe "C:\Path\To\Your\TwilightPrincess.iso"`

### 4. What to expect in this version
*   **Headset View:** You will **NOT** see the game in the headset lenses yet.
*   **Monitor View:** The game window on your monitor will respond to your headset's physical movement.
*   **Rotation & Position:** If you tilt your head or move around, the in-game camera should follow exactly.

---
**Status:** Phase 1 (OpenXR Instance) and Phase 2 (Head Tracking) complete.
**Next Up:** Phase 3 (Stereo Rendering to Headset Lenses).
