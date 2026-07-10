# DraconicEngine Shell Backends

DraconicEngine utilizes specialized shell backends to manage the application lifecycle and windowing environment depending on the target platform.

## Available Backends

* **Null Backend:** A headless implementation used for environments where no graphical interface is required (e.g., dedicated servers, automated testing).
* **Desktop Backend:** An SDL3-based implementation used for standard graphical desktop environments.

---

## Architectural & Linkage Rules

### 1. Capsule Encapsulation
Neither backend is exported through the main `draconic` module. Instead, each backend independently supplies the `createShell()` factory function, which acts as a compile-time bridge for the engine.

### 2. Implementation Seclusion
The SDL3 implementation details are strictly confined to the desktop backend's own Translation Units (TUs) to prevent API leakage across the engine.

### 3. Dependency Visibility
The SDL3 dependency remains marked as **`PUBLIC`** so that the desktop backend's own SDL-aware tests can properly resolve the necessary SDL headers.

### 4. Runtime Exposure & Consumption
* **Unified API:** The Runtime component exposes the entire engine through a single `draconic` module.
* **Public Linkage:** The Shell backend (specifically the desktop implementation supplying `createShell()`) is linked **`PUBLIC`** to the Runtime.
* **Streamlined Integration:** Because of this setup, an end consumer only needs to link against the Runtime and can simply call `import draconic;` to gain full access to the engine and its underlying shell architecture.
