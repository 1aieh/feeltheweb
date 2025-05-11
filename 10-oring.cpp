#include "chai3d.h"
#include "GLFW/glfw3.h"
#include <iostream>

using namespace chai3d;
using namespace std;

// Global variables
cWorld* world;
cCamera* camera;
cShapeBox* button; // This will be the VISIBLE button
cShapeBox* hapticShell; // This is the INVISIBLE haptic boundary
cToolCursor* tool;
cHapticDeviceHandler* handler;
cGenericHapticDevicePtr hapticDevice;
GLFWwindow* window;
cThread* hapticsThread;
bool simulationRunning = false;
bool simulationFinished = true;

// Function declarations
void close(void);
void renderGraphics(void);
void updateHaptics(void);

// Main function
int main() {
    cout << "CHAI3D Button Demo with Manual Pull Force\n";

    // Initialize GLFW
    if (!glfwInit()) {
        cout << "Failed to initialize GLFW" << endl;
        return -1;
    }

    // Create window
    window = glfwCreateWindow(800, 600, "Button Demo", NULL, NULL);
    if (!window) {
        cout << "Failed to create window" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Create a world
    world = new cWorld();
    world->m_backgroundColor.setGrayLevel(0.8); // Light gray background

    // Create camera
    camera = new cCamera(world);
    world->addChild(camera);
    camera->set(cVector3d(0.0, -0.5, 0.1),   // Position - in front of the button
        cVector3d(0.0, 0.0, 0.0),    // Look at - center where button is
        cVector3d(0.0, 0.0, 1.0));   // Up vector - Z axis is up
    camera->setClippingPlanes(0.01, 10.0);


    // Create light source
    cDirectionalLight* light = new cDirectionalLight(world);
    light->setEnabled(true);
    light->setLocalPos(1.0, 1.0, 1.0);
    light->setDir(-1.0, -1.0, -1.0);

    // Setup haptic device
    handler = new cHapticDeviceHandler();
    if (!handler->getDevice(hapticDevice, 0)) {
        cout << "No haptic device found" << endl;
        glfwTerminate();
        return -1;
    }

    // Create tool cursor
    tool = new cToolCursor(world);
    world->addChild(tool);
    tool->setHapticDevice(hapticDevice);
    tool->setRadius(0.01);  // Display size of the tool

    //fixing axes mapping
    cMatrix3d rotMatrix;
    rotMatrix.identity();
    rotMatrix.rotateAboutGlobalAxisDeg(cVector3d(0, 0, 1), 270); // Correct rotation for axis alignment
    tool->setLocalRot(rotMatrix);

    tool->setShowContactPoints(true);
    tool->start(); // Start the tool AFTER setting up rotation and device

    // --- START: Two-Layer Button Implementation ---

    // 1. Create the VISIBLE button (inner button)
    button = new cShapeBox(0.1, 0.05, 0.02);  // Your desired visual size
    world->addChild(button);
    button->setLocalPos(0.0, 0.0, 0.0);      // Center position
    button->m_material->setRedCrimson();     // Make it visible red
    // NO stiffness or haptic effect needed on the purely visual part

    // 2. Create the INVISIBLE HAPTIC shell (slightly larger)
    double shellOffset = 0.001; // How much larger the haptic shell is (e.g., 1mm) - Adjust as needed for sync
    hapticShell = new cShapeBox(
        button->getSizeX() + shellOffset * 2.0, // Base size on visual button
        button->getSizeY() + shellOffset * 2.0,
        button->getSizeZ() + shellOffset * 2.0
    );
    world->addChild(hapticShell);
    hapticShell->setLocalPos(0.0, 0.0, 0.0); // Center it with the visual button

    // 3. Configure the HAPTIC shell
    hapticShell->setShowEnabled(false); // Make it invisible

    cHapticDeviceInfo info = hapticDevice->getSpecifications(); // Get max stiffness
    double maxStiffness = info.m_maxLinearStiffness;
    double workspaceScaleFactor = tool->getWorkspaceScaleFactor();
    if (workspaceScaleFactor == 0) workspaceScaleFactor = 1.0; // Avoid division by zero if not set yet
    double adjustedStiffness = maxStiffness / workspaceScaleFactor;

    hapticShell->m_material->setStiffness(adjustedStiffness); // Apply max stiffness
    hapticShell->m_material->setDynamicFriction(0.5); // Optional: Add friction
    hapticShell->m_material->setStaticFriction(0.9);  // Optional: Add friction
    hapticShell->createEffectSurface(); // Enable haptic interaction ONLY on the shell

    // --- END: Two-Layer Button Implementation ---


    // Start haptics thread
    hapticsThread = new cThread();
    hapticsThread->start(updateHaptics, CTHREAD_PRIORITY_HAPTICS);

    // Main graphic loop
    while (!glfwWindowShouldClose(window)) {
        renderGraphics();
        glfwPollEvents();

        // Press ESC to exit
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
    }

    // Close everything
    close();
    return 0;
}

// Clean up function
void close() {
    // Stop haptics
    simulationRunning = false;
    // Wait for haptics thread to finish
    while (!simulationFinished) { cSleepMs(10); }

    // Stop the tool explicitely before closing device
    if (tool) {
        tool->stop();
    }
    // Close haptic device connection
    if (hapticDevice) {
        hapticDevice->close();
    }

    // Delete resources in reverse order of creation (roughly)
    delete hapticsThread;
    // World owns its children, so deleting world should delete camera, button, shell, tool, light etc.
    delete world;
    // Handler is separate
    delete handler;

    // Close GLFW window and terminate GLFW
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
    cout << "Application closed." << endl;
}

// Graphics rendering function
void renderGraphics() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height); // Get current framebuffer size

    // Set viewport to cover the entire window
    glViewport(0, 0, width, height);

    // Clear screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render world using the camera perspective
    // Pass the actual framebuffer dimensions to renderView
    camera->renderView(width, height);

    // Swap buffers
    glfwSwapBuffers(window);

    // Check for OpenGL errors (optional but good for debugging)
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        cerr << "OpenGL Error: " << err << endl;
    }
}

// Haptics update function
void updateHaptics() {
    simulationRunning = true;
    simulationFinished = false;

    // Define parameters for your custom pull force
    const double maxPullDistance = 0.08; // Max distance for the pull effect (8cm) - Adjust
    const double pullStrength = 50.0;   // Strength factor for the pull - Adjust significantly based on feel
    const double forceLimit = 3.0;      // Safety limit for the pull force (N) - Adjust

    while (simulationRunning) {
        // Ensure haptic device and objects are valid
        if (!hapticDevice || !tool || !hapticShell || !world) {
            cSleepMs(1); // Avoid busy-waiting if something is null
            continue;
        }

        // Compute global positions of objects (important for collision detection)
        world->computeGlobalPositions(true);

        // Update position and orientation of tool from the haptic device
        tool->updateFromDevice();

        // Calculate interaction forces between the tool and objects (CONTACT FORCES)
        // This calculates forces based on stiffness, friction etc. from createEffectSurface()
        tool->computeInteractionForces();

        // --- START: Manual Pull Force Calculation & Combination (Using Documented Methods - Corrected Rotation) ---

        // Get the force calculated by CHAI3D's collision detection in LOCAL tool coordinates
        // (Still using getDeviceLocalForce based on previous documentation snippet [[1]] from cGenericTool)
        cVector3d contactForceLocal = tool->getDeviceLocalForce();

        // Get current position of the haptic device/tool in GLOBAL coordinates
        cVector3d toolPosGlobal = tool->getDeviceGlobalPos();

        // Get the center position of the haptic shell in GLOBAL coordinates
        cVector3d shellPosGlobal = hapticShell->getGlobalPos();

        // Calculate vector from tool to shell center in GLOBAL coordinates
        cVector3d vecToShellGlobal = shellPosGlobal - toolPosGlobal;

        // Calculate distance
        double distance = vecToShellGlobal.length();

        // Initialize pull force vector (start in GLOBAL coordinates)
        cVector3d pullForceGlobal(0.0, 0.0, 0.0);

        // Check if the tool is within the pull distance
        if (distance > 0.001 && distance < maxPullDistance) // Avoid division by zero and check range
        {
            // Calculate force magnitude (e.g., linear falloff)
            double forceMagnitude = pullStrength * (1.0 - (distance / maxPullDistance));

            // Clamp the force magnitude to the safety limit
            if (forceMagnitude > forceLimit) {
                forceMagnitude = forceLimit;
            }
            // Ensure force is not negative if calculation goes below zero
            if (forceMagnitude < 0) {
                forceMagnitude = 0;
            }

            // Calculate the force vector in GLOBAL coordinates (direction * magnitude)
            pullForceGlobal = cNormalize(vecToShellGlobal) * forceMagnitude;
        }

        // Convert the GLOBAL pull force into the tool's LOCAL coordinate system
        cMatrix3d toolRot = tool->getDeviceGlobalRot(); // Get tool's current rotation
        cMatrix3d toolRotTransposed;                    // Create matrix to store transpose
        toolRot.transr(toolRotTransposed);              // Compute transpose using transr() [[1]]

        // Use matrix multiplication with the computed transpose
        cVector3d pullForceLocal = toolRotTransposed * pullForceGlobal; // CORRECTED ROTATION

        // Combine the LOCAL contact force and the LOCAL pull force
        cVector3d totalForceLocal = contactForceLocal + pullForceLocal;

        // Set the total force to be applied to the device in LOCAL coordinates
        // (Still using setDeviceLocalForce based on previous documentation snippet [[1]] from cGenericTool)
        tool->setDeviceLocalForce(totalForceLocal);

        // --- END: Manual Pull Force Calculation & Combination ---


        // Send the TOTAL calculated force (set by setDeviceLocalForce) back to the haptic device
        tool->applyToDevice();

        // Small sleep to avoid hogging CPU (optional)
        // cSleepMs(1);
    }

    // Signal that the haptics loop has finished
    simulationFinished = true;
}
