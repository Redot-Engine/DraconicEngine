# Draconic Engine

<p align="center">
  <a href="https://draconicengine.org">
    <img src="assets/draconic_logo_text.png" width="400" alt="Draconic Engine Logo">
  </a>
</p>

**Draconic Engine is an open-source, multi-purpose game engine designed to bridge the gap between the accessibility of indie tools and the raw power required for AAA production. Built with a "performance-forward" & "modularity" philosophy, it provides a robust alternative to industry giants like Unreal, Unity, and Godot.**

> [!NOTE]
> The engine is still a W.I.P., including this documentation.
> 
> Help us out by adding new features or information!

## Compiling Draconic Engine

### Linux

In order to build DraconicEngine, one currently needs the following tools:

* CMake 4.x with CTest
* Ninja (`ninja-build`)
* Clang (at least 18, but 21 or newer strongly recommended)

Make sure these tools are installed in your system.

After cloning this repository, call (for configuring a release build): 

```cmake --preset release```

To build, call:

```cmake --build build/release -j$(nproc)```

After the build successfully completed, unit tests can be executed via CTest.

```ctest --test-dir build/release```

## Architecture

Draconic Engine is a two-tiered project, with an inner core written in C++ and
a public interface and editor in C#.

Regarding the native layer, it is written in C++, according to the C++23 standard and using modules.
Documentation regarding how C++ modules are used can be read [here](docs/cxxmodules.md).
