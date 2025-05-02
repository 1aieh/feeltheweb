# Chai3D and Geomagic Touch: Foundational Implementation (Based on 07-mouse-select.cpp)

This document outlines the core concepts and setup procedures for creating haptic applications with Chai3D and a Geomagic Touch device, derived from the provided demo code (`07-mouse-select.cpp`). It covers initialization, fundamental Chai3D components, setting object haptic properties, and the main application structure.

## Prerequisites and Setup

Before diving into the code, ensure you have the following set up:

1.  **Geomagic Touch Device:** Physical access to the device and its drivers installed.
2.  **OpenHaptics Toolkit:** Installed on your system. Chai3D uses this for device communication.
3.  **Chai3D Library:** Downloaded, built, and configured in your development environment (e.g., linked in your C++ project).
4.  **GLFW:** A cross-platform library for creating windows with OpenGL contexts. The demo uses it for window management and input.

**Minimal Setup Steps demonstrated in the code:**

* **Include Headers:** Include necessary Chai3D headers (`chai3d.h`) and external libraries like GLFW (`GLFW/glfw3.h`).
* **Initialize GLFW:**
    ```cpp
    if (!glfwInit()) {
        // Handle initialization error
    }
    glfwSetErrorCallback(onErrorCallback); // Optional: Set error callback
    ```
* **Create GLFW Window and OpenGL Context:** Configure window hints (OpenGL version, stereo, multisampling, etc.) and create the window.
    ```cpp
    GLFWwindow* window = glfwCreateWindow(windowW, windowH, "CHAI3D", NULL, NULL);
    if (!window) {
        // Handle window creation error
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(swapInterval); // Enable V-Sync
    ```
* **Initialize GLEW (if used):** If you are using GLEW for OpenGL extensions.
    ```cpp
    #ifdef GLEW_VERSION
    if (glewInit() != GLEW_OK) {
        // Handle GLEW initialization error
    }
    #endif
    ```
* **Create a Chai3D World:** This is the root of your virtual environment.
    ```cpp
    cWorld* world = new cWorld();
    world->m_backgroundColor.setWhite(); // Set background color
    ```
* **Create a Camera:** Represents the viewpoint. Add it to the world.
    ```cpp
    cCamera* camera = new cCamera(world);
    world->addChild(camera);
    camera->set(...); // Set camera position, lookat, up vectors
    camera->setClippingPlanes(...);
    camera->setStereoMode(...); // Configure stereo if needed
    ```
* **Create Lighting:** Illuminate your scene. Add lights to the world or camera.
    ```cpp
    cDirectionalLight* light = new cDirectionalLight(world);
    camera->addChild(light); // Or world->addChild(light);
    light->setEnabled(true);
    light->setLocalPos(...);
    light->setDir(...);
    ```
* **Create a Haptic Device Handler:** Manages connections to haptic devices.
    ```cpp
    cHapticDeviceHandler* handler = new cHapticDeviceHandler();
    handler->getDevice(hapticDevice, 0); // Get the first available device
    ```
* **Retrieve Device Info:** Get specifications of the connected device.
    ```cpp
    cHapticDeviceInfo hapticDeviceInfo = hapticDevice->getSpecifications();
    ```
* **Create a Haptic Tool:** Represents the haptic device's interaction point in the virtual world. Add it to the world.
    ```cpp
    cToolCursor* tool = new cToolCursor(world);
    world->addChild(tool);
    tool->setHapticDevice(hapticDevice); // Link tool to device
    tool->setWorkspaceRadius(0.9); // Map device workspace to virtual workspace
    tool->setRadius(toolRadius); // Set the size of the tool's interaction sphere
    tool->enableDynamicObjects(true); // Enable if objects move or collide
    tool->start(); // Start the haptic tool's internal processes
    ```
* **Create a Viewport:** Connects the camera view to the GLFW window.
    ```cpp
    cViewport* viewport = new cViewport(camera, contentScaleW, contentScaleH);
    ```
* **Create a Haptic Thread:** Haptic rendering should run in a separate high-priority thread.
    ```cpp
    cThread* hapticsThread = new cThread();
    hapticsThread->start(renderHaptics, CTHREAD_PRIORITY_HAPTICS);
    ```
* **Set Exit Callback:** Ensure resources are cleaned up on exit.
    ```cpp
    atexit(close);
    ```

## Core Chai3D Concepts from the Demo

The code demonstrates several fundamental Chai3D building blocks:

* **`cWorld`**: The container for all virtual objects, cameras, and lights. It manages the scene graph.
* **`cCamera`**: Defines the viewpoint and projection for rendering the scene.
* **`cLight`**: Provides illumination for the scene's objects.
* **`cHapticDeviceHandler`**: Discovers and manages connected haptic devices.
* **`cGenericHapticDevicePtr`**: A smart pointer representing a connection to a specific haptic device.
* **`cToolCursor`**: A visual and haptic representation of the user's interaction point (usually the stylus tip) in the virtual world. It handles collision detection and force computation based on interactions.
* **`cGenericObject`**: The base class for all renderable and potentially haptic objects in Chai3D. Derived classes include `cMesh` and various `cShape` types.
* **Scene Graph:** Objects are organized in a tree structure (`world->addChild(object)`). Transformations (position, rotation, scale) are inherited down the tree.
* **`cMaterial`**: Defines the visual (color, texture) and haptic (stiffness, damping, friction) properties of an object's surface. Each object typically has a material associated with it (`object->m_material`).
* **`cEffect`**: Represents specific haptic interactions or textures (e.g., surface contact, friction, vibration, magnetism). Effects are added to objects to define how they feel. The demo uses `createEffectSurface()` which implicitly adds a `cEffectSurface` for basic stiffness/contact.

## Implementing Haptic Properties (Stiffness and Surface)

The demo shows how to assign haptic properties to objects through their material and by adding effects:

* **Material Haptic Properties:**
    * `object->m_material->setStiffness(value)`: Sets the stiffness (resistance to penetration) of the object's surface. A higher value feels harder. The demo scales stiffness based on device capabilities.
    * Other `cMaterial` properties like `setDamping()`, `setStaticFriction()`, `setDynamicFriction()` can be set here to influence texture, though not explicitly demonstrated for varied textures in this code.
* **Adding Haptic Effects:**
    * `object->createEffectSurface()`: This is a convenience function that adds a `cEffectSurface` to the object. This effect handles the basic contact force based on the material's stiffness and damping.
    * For implementing specific textures like friction or vibration, you would typically create instances of other `cEffect` derived classes (e.g., `cEffectFriction`, `cEffectVibration`) and add them to your objects manually:
        ```cpp
        // Example (not in demo but relevant for texture)
        cEffectFriction* frictionEffect = new cEffectFriction(object);
        object->addEffect(frictionEffect);
        frictionEffect->setStaticFriction(value);
        frictionEffect->setDynamicFriction(value);
        frictionEffect->setFrictionMaxForce(value);
        ```
    * The demo sets `cMaterial` properties (`setStiffness`) and uses `createEffectSurface()`. This means the haptic feel is primarily defined by stiffness when contact occurs.

## Application Structure: Graphics and Haptics Loops

Chai3D applications typically run two main loops in separate threads:

1.  **Graphic Rendering Loop (`renderGraphics`):**
    * Runs at the monitor's refresh rate (or determined by `glfwSwapInterval`).
    * Handles rendering the 3D scene to the window.
    * Updates visual elements (like the position of objects moved by the mouse or updating labels).
    * Calls `world->updateShadowMaps()` and `viewport->renderView()`.
    * Swaps display buffers (`glfwSwapBuffers`).
    * Checks for OpenGL errors.
    * Signals a frequency counter (`freqCounterGraphics`) for performance monitoring.

2.  **Haptic Rendering Loop (`renderHaptics`):**
    * Runs in a high-priority thread to maintain a stable, high update rate (typically 1000 Hz for convincing haptics).
    * Handles interaction with the haptic device and computing forces.
    * Calls `world->computeGlobalPositions(true)`: Updates the global positions of all objects in the scene graph. **Crucial for correct collision detection.**
    * Calls `tool->updateFromDevice()`: Reads the current position, orientation, and button status from the physical haptic device and updates the virtual tool's state.
    * Calls `tool->computeInteractionForces()`: Performs collision detection between the tool and the virtual world objects and calculates the resulting haptic forces (based on object materials, effects, and tool properties).
    * Calls `tool->applyToDevice()`: Sends the computed forces back to the physical haptic device.
    * Signals a frequency counter (`freqCounterHaptics`).
    * This loop continues as long as `simulationRunning` is true.

## Input Handling (Mouse and Keyboard)

The demo integrates GLFW callbacks for user input:

* `onKeyCallback`: Handles keyboard presses (e.g., quitting, toggling fullscreen).
* `onMouseButtonCallback`: Handles mouse button presses. In the demo, this is used for selecting objects or UI widgets by casting rays from the mouse position into the scene (`viewport->selectFrontLayer`, `camera->selectWorld`) and checking for collisions.
* `onMouseMotionCallback`: Handles mouse movement, used here to move the selected object in 3D space based on mouse position.

While this demo uses mouse input for selection and manipulation, in your button project, device button presses (e.g., the gripper button on the Geomagic Touch) will be the primary input mechanism for *selecting* a virtual button. You would read this state using `hapticDevice->getUserSwitch(0)`.

## Adapting for Your Button Project

Based on this understanding, here's how you can build your button haptics project:

1.  **Keep the Core Structure:** Retain the initialization, device/tool setup, separate rendering loops, and the basic world/camera/light setup.
2.  **Create Your Buttons:** Instead of arbitrary shapes, create two specific objects (e.g., `cShapeBox` or `cMesh`) to represent your buttons. Position them at fixed locations in your world.
3.  **Define Haptic Properties for Buttons:** This is where you'll implement your experiment's haptic feedback:
    * For the **Magnetism** condition: You would likely use a custom force effect or manipulate the tool's desired position/force near the button center to simulate attraction/repulsion when the tool is within the button's bounding box. Chai3D might have a dedicated magnetism effect, or you might need to implement the force calculation yourself within the haptic loop when the tool is in the button zone.
    * For the **Texture** condition: You would set the `cMaterial` properties (`setStaticFriction`, `setDynamicFriction`, `setVibration`) for the "rough" and "smooth" buttons. You might also add `cEffectFriction` or `cEffectVibration` instances to the buttons and configure them.
    * For the **Control** condition: Assign magnetism properties opposite to Group A.
4.  **Implement Button Zones and Detection:** Use bounding boxes (`cShapeBox` or `cBounds`) or implement your own spatial checks (e.g., checking if the tool's proxy position is within a specific volume) to determine when the tool enters a button's activation zone.
5.  **Apply Forces within Zones:** Inside the `renderHaptics` loop, *after* `tool->updateFromDevice()` and *before* `tool->computeInteractionForces()`, check which button zone (if any) the tool is in. If in a zone, apply your specific haptic effect (magnetism force or rely on the material/effect properties you set up) **in addition to** the forces computed by `tool->computeInteractionForces()`. You can apply additional forces directly to the tool using `tool->setDeviceGlobalForce()`.
6.  **Implement Selection Logic:** Define how a button is "pressed" (e.g., tool penetration depth into the button's front face, or a device button press while inside the zone).
7.  **Log Data:** When a selection occurs, record the instructed task (Submit/Delete), the user's selection (Button 1/Button 2), accuracy, and the time taken from task start.
8.  **UI for Tasks/Feedback:** Add UI elements (using `cLabel` or other widgets in the front layer) to display the current task instruction ("Press the Submit button") and potentially feedback.

This demo code provides the scaffolding for the window, device interaction, world setup, and the crucial dual graphic/haptic loop structure. Your task will be to build upon this by creating your specific button geometry, defining their unique haptic properties using `cMaterial` and `cEffect` (or custom force calculations), implementing the spatial zone detection, and adding the selection and data logging logic.