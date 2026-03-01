
 -> Whats Parlel
    "Parlel" is a programming language composed of functions and operators.
  But the best thing about "Parlel" is that it's moddable, because we developers know what modding is.

  ->Installing Parlel
    To install Parlel, you first need to compile the main.cpp and then the parlelModder.cpp files.
   Do not compile the core.hpp and vanillaP.cpp files during this process.
   After compiling the main.cpp and parlelModder.cpp files, run parlelModder and enter the location of the vanillaP.cpp file for installation.
   Run the .prl file you want to use with the main.cpp file you compiled and enter the location of your file.
   If you want a quick setup you can download parlel.exe and parlelModder.exe with core.hpp

Parlel Mod Development Walkthrough
I have implemented two major mods for the Parlel engine as requested: a high-quality GUI Mod and an enhanced VanillaPlus Mod.

What's New
 1. VanillaPlus Mod
  An advanced version of the existing VanillaP mod with several new capabilities:
  
  Math functions: math_sin, math_cos, math_pow, math_sqrt, math_abs, math_rand.
  String utilities: str_upper, str_lower, str_len.
  System utilities: sys_sleep, sys_time.
  Improved Logic: Added for_prl loop support.

 2. GUI Mod
  A modern GUI framework using Windows GDI+ for high-fidelity rendering:

  Smooth Edges: All components (Frames, Buttons, TextBoxes) feature anti-aliased rounded corners.
  RGB Support: Full control over component colors using RGB values.
  Interactive: Support for Buttons, Text, TextBoxes, and Frames.
  Dynamic: Can be updated in a loop with smooth 60 FPS performance.
  3. Cross-Platform Support
  The engine and VanillaPlus mod are now fully cross-platform:

 Core Abstraction: 
  core.hpp
   automatically detects the OS and handles .dll
   (Windows) vs .so (Linux/macOS) loading.
   Portable Code: Replaced Windows-specific API (Sleep, GetTickCount) with standard C++ (std::thread, std::chrono).
   Compiler Support: Added support for g++ on Linux/macOS alongside cl on Windows.
   How to Verify
   Step 1: Compilation
   Windows (MSVC)
   Open "Developer Command Prompt for VS".
   cl /LD /EHsc /std:c++17 VanillaP.cpp
   Linux/macOS (GCC/Clang)
   Open Terminal.
   g++ -shared -fPIC -std=c++17 VanillaP.cpp -o libVanillaP.so


   A glass-morphic style window frame with rounded corners.
  Modern styled buttons with hover colors (can be added in Parlel logic).
  A moving button animated by math_sin and sys_time.
  Text input fields with proper borders.
  Enjoy your new, advanced Parlel environment!


    Note that when installing "Parlel," you need to compile VanillaP.cpp with Modder. This applies to all other mods as well.
  
