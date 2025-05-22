//------------------------------------------------------------------------------
#include "chai3d.h"
//------------------------------------------------------------------------------
#include <fstream>
#include <iomanip>
#include <random>
#include <GLFW/glfw3.h>
#include <chrono>
//------------------------------------------------------------------------------
using namespace chai3d;
using namespace std;
using ofstream = basic_ofstream<char>;
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// GENERAL SETTINGS
//------------------------------------------------------------------------------

// stereo Mode
/*
    C_STEREO_DISABLED:            Stereo is disabled
    C_STEREO_ACTIVE:              Active stereo for OpenGL NVDIA QUADRO cards
    C_STEREO_PASSIVE_LEFT_RIGHT:  Passive stereo where L/R images are rendered next to each other
    C_STEREO_PASSIVE_TOP_BOTTOM:  Passive stereo where L/R images are rendered above each other
*/
cStereoMode stereoMode = C_STEREO_DISABLED;

// fullscreen mode
bool fullscreen = false;

// mirrored display
bool mirroredDisplay = false;

//------------------------------------------------------------------------------
// DECLARED CONSTANTS
//------------------------------------------------------------------------------

const string PARTICIPANT_NAME = "Bhavya";
constexpr int NUM_BUTTONS = 10;

constexpr double BUTTON_WIDTH = 0.10;
constexpr double BUTTON_HEIGHT = 0.04;

constexpr double GRAVITY_WELL_FORCE_MAGNITUDE = 0.41;     // Newtons
constexpr double GRAVITY_WELL_ACTIVE_RADIUS = BUTTON_WIDTH * 0.75; // Radius for well activation
constexpr double GRAVITY_WELL_CENTER_DEAD_ZONE_RADIUS = 0.01; // Radius for resting in center

//------------------------------------------------------------------------------
// DECLARED VARIABLES
//------------------------------------------------------------------------------

// a world that contains all objects of the virtual environment
cWorld* world;

// a camera to render the world in the window display
cCamera* camera;

// a viewport to display the scene viewed by the camera
cViewport* viewport = nullptr;

// a light source to illuminate the objects in the world
cDirectionalLight* light;

// a haptic device handler
cHapticDeviceHandler* handler;

// a pointer to the current haptic device
cGenericHapticDevicePtr hapticDevice;

// a virtual tool representing the haptic device in the scene
cToolCursor* tool;

struct ButtonState {
    cMesh* mesh;
    bool clicked = false;
    bool active = false;
};

// a few mesh objects
cMesh* startButton;
cMesh* objectBackground;
ButtonState buttons[NUM_BUTTONS];

cNormalMapPtr buttonNormalMap;
cTexture2dPtr buttonTexture;
cMaterialPtr buttonMaterialFriction;
cMaterialPtr buttonMaterialPlain;

// a colored background
cBackground* background;

// a font for rendering text
cFontPtr font;

// a label to display the rate [Hz] at which the simulation is running
cLabel* labelRates;

// a label to display the round count
cLabel* labelRound;

// a label to display the time left in the round
cLabel* labelRoundTime;

// label haptics on
cLabel* labelHapticsOn;

// a flag that indicates if the haptic simulation is currently running
bool simulationRunning = false;

// a flag that indicates if the haptic simulation has terminated
bool simulationFinished = true;

// mouse position
double mouseX, mouseY;

double maxStiffness;

// set radius of tool
double toolRadius = 0.01;

// a frequency counter to measure the simulation graphic rate
cFrequencyCounter freqCounterGraphics;

// a frequency counter to measure the simulation haptic rate
cFrequencyCounter freqCounterHaptics;

// haptic thread
cThread* hapticsThread;

// a handle to window display context
GLFWwindow* window = nullptr;

// current size of GLFW window
int windowW = 0;
int windowH = 0;

// current size of GLFW framebuffer
int framebufferW = 0;
int framebufferH = 0;

// swap interval for the display context (vertical synchronization)
int swapInterval = 1;

//? EXPERIMENT VARIABLES
bool hapticsOn = true; // if false, haptics are disabled

//------------------------------------------------------------------------------
// DECLARED FUNCTIONS
//------------------------------------------------------------------------------

// callback when the window is resized
void onWindowSizeCallback(GLFWwindow* a_window, int a_width, int a_height);

// callback when the window framebuffer is resized
void onFrameBufferSizeCallback(GLFWwindow* a_window, int a_width, int a_height);

// callback when an error GLFW occurs
void onErrorCallback(int a_error, const char* a_description);

// callback when a key is pressed
void onKeyCallback(GLFWwindow* a_window, int a_key, int a_scancode, int a_action, int a_mods);

// callback to handle mouse click
void onMouseButtonCallback(GLFWwindow* a_window, int a_button, int a_action, int a_mods);

// callback when window content scaling is modified
void onWindowContentScaleCallback(GLFWwindow* a_window, float a_xscale, float a_yscale);

// this function renders the scene
void renderGraphics(void);

// this function contains the main haptics simulation loop
void renderHaptics(void);

// this function closes the application
void close(void);

void randomizeButtonPositions();

void createButtons();

void removeAllButtons();

void setupDemo();


//==============================================================================
/*
    DEMO:   14-textures.cpp

    This example illustrates the use of haptic textures projected onto mesh
    surfaces.
*/
//==============================================================================

int main(int argc, char* argv[]) {
    //--------------------------------------------------------------------------
    // INITIALIZATION
    //--------------------------------------------------------------------------

    cout << endl;
    cout << "-----------------------------------" << endl;
    cout << "CHAI3D" << endl;
    cout << "Demo: 14-textures" << endl;
    cout << "Copyright 2003-2024" << endl;
    cout << "-----------------------------------" << endl << endl << endl;
    cout << "Keyboard Options:" << endl << endl;
    cout << "[f] - Enable/Disable full screen mode" << endl;
    cout << "[r] - Reset Demo" << endl;
    cout << "[q] - Exit application" << endl;
    cout << endl << endl;

    // get current path
    bool fileload;
    string currentpath = cGetCurrentPath();


    //--------------------------------------------------------------------------
    // OPEN GL - WINDOW DISPLAY
    //--------------------------------------------------------------------------

    // initialize GLFW library
    if (!glfwInit()) {
        cout << "failed initialization" << endl;
        cSleepMs(1000);
        return 1;
    }

    // set GLFW error callback
    glfwSetErrorCallback(onErrorCallback);

    // compute desired size of window
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    windowW = 0.8 * mode->height;
    windowH = 0.5 * mode->height;

    cout << "Window size: " << windowW << " x " << windowH << endl;

    int x = 0.5 * (mode->width - windowW);
    int y = 0.5 * (mode->height - windowH);

    // set OpenGL version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    // enable double buffering
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

    // set the desired number of samples to use for multisampling
    glfwWindowHint(GLFW_SAMPLES, 4);

    // specify that window should be resized based on monitor content scale
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    // set active stereo mode
    if (stereoMode == C_STEREO_ACTIVE) {
        glfwWindowHint(GLFW_STEREO, GL_TRUE);
    }
    else {
        glfwWindowHint(GLFW_STEREO, GL_FALSE);
    }

    // create display context
    window = glfwCreateWindow(windowW, windowH, "CHAI3D", NULL, NULL);
    if (!window) {
        cout << "failed to create window" << endl;
        cSleepMs(1000);
        glfwTerminate();
        return 1;
    }

    // set GLFW key callback
    glfwSetKeyCallback(window, onKeyCallback);

    // set GLFW mouse button callback
    glfwSetMouseButtonCallback(window, onMouseButtonCallback);

    // set GLFW window size callback
    glfwSetWindowSizeCallback(window, onWindowSizeCallback);

    // set GLFW framebuffer size callback
    glfwSetFramebufferSizeCallback(window, onFrameBufferSizeCallback);

    // set GLFW window content scaling callback
    glfwSetWindowContentScaleCallback(window, onWindowContentScaleCallback);

    // get width and height of window
    glfwGetFramebufferSize(window, &framebufferW, &framebufferH);

    // set position of window
    glfwSetWindowPos(window, x, y);

    // set window size
    glfwSetWindowSize(window, windowW, windowH);

    // set GLFW current display context
    glfwMakeContextCurrent(window);

    // set GLFW swap interval for the current display context
    glfwSwapInterval(swapInterval);


    // initialize GLEW library
#ifdef GLEW_VERSION
    if (glewInit() != GLEW_OK)
    {
        cout << "failed to initialize GLEW library" << endl;
        glfwTerminate();
        return 1;
    }
#endif

    //--------------------------------------------------------------------------
    // WORLD - CAMERA - LIGHTING
    //--------------------------------------------------------------------------

    // create a new world.
    world = new cWorld();

    // set the background color of the environment
    world->m_backgroundColor.setBlack();

    // create a camera and insert it into the virtual world
    camera = new cCamera(world);
    world->addChild(camera);

    // position and orient the camera
    camera->set(cVector3d(0.0, 0.0, 1.0), // camera position (eye)
        cVector3d(0.0, 0.0, 0.0), // lookat position (target)
        cVector3d(0.0, 1.0, 0.0)); // direction of the (up) vector

    // set the near and far clipping planes of the camera
    // anything in front or behind these clipping planes will not be rendered
    camera->setClippingPlanes(0.01, 10.0);

    // set stereo mode
    camera->setStereoMode(stereoMode);

    // set stereo eye separation and focal length (applies only if stereo is enabled)
    camera->setStereoEyeSeparation(0.02);
    camera->setStereoFocalLength(1.0);

    // set vertical mirrored display mode
    camera->setMirrorVertical(mirroredDisplay);

    // create a light source
    //light = new cSpotLight(world);

    //// attach light to camera
    //world->addChild(light);

    //// enable light source
    //light->setEnabled(true);

    //// position the light source
    //light->setLocalPos(0.0, 0.0, 0.7);

    //// define the direction of the light beam
    //light->setDir(0.0, 0.0, -1.0);

    //// enable this light source to generate shadows
    //light->setShadowMapEnabled(true);

    //// set the resolution of the shadow map
    //light->m_shadowMap->setQualityLow();
    //light->m_shadowMap->setQualityMedium();

    // set light cone half angle
    /*light->setCutOffAngleDeg(40);*/

    // create a directional light source
    light = new cDirectionalLight(world);
    // insert light source inside world
    world->addChild(light);
    // enable light source
    light->setEnabled(true);
    // define direction of light beam
    light->setDir(0.0, 0.0, 1.0);


    //--------------------------------------------------------------------------
    // HAPTIC DEVICES / TOOLS
    //--------------------------------------------------------------------------

    // create a haptic device handler
    handler = new cHapticDeviceHandler();

    // get access to the first available haptic device
    handler->getDevice(hapticDevice, 0);

    // retrieve information about the current haptic device
    cHapticDeviceInfo hapticDeviceInfo = hapticDevice->getSpecifications();

    // create a 3D tool and add it to the world
    tool = new cToolCursor(world);
    camera->addChild(tool);

    // position tool in respect to camera
    tool->setLocalPos(-1.0, 0.0, 0.0);

    // connect the haptic device to the tool
    tool->setHapticDevice(hapticDevice);

    // define a radius for the tool
    tool->setRadius(toolRadius);

    // map the physical workspace of the haptic device to a larger virtual workspace.
    tool->setWorkspaceRadius(0.6);

    // haptic forces are enabled only if small forces are first sent to the device;
    // this mode avoids the force spike that occurs when the application starts when
    // the tool is located inside an object for instance.
    tool->setWaitForSmallForce(false);

    // start the haptic tool
    tool->start();

	cMaterialPtr toolMaterial = cMaterial::create();
	toolMaterial->setBlack();
    tool->setMaterial(toolMaterial);


    //--------------------------------------------------------------------------
    // CREATE OBJECTS
    //--------------------------------------------------------------------------

    // read the scale factor between the physical workspace of the haptic
    // device and the virtual workspace defined for the tool
    double workspaceScaleFactor = tool->getWorkspaceScaleFactor();

    // properties
    maxStiffness = hapticDeviceInfo.m_maxLinearStiffness / workspaceScaleFactor;


    buttonTexture = cTexture2d::create();
    fileload = buttonTexture->loadFromFile(currentpath + "../resources/images/sand.jpg");

    if (!fileload) {
        cout << "Error - Texture image failed to load correctly." << endl;
        close();
        return (-1);
    }

    buttonNormalMap = cNormalMap::create();
    buttonNormalMap->createMap(buttonTexture);

    buttonMaterialFriction = cMaterial::create();
    buttonMaterialFriction->setRedLightCoral();
    buttonMaterialFriction->setStiffness(0.5 * maxStiffness);
    buttonMaterialFriction->setStaticFriction(0.3);
    buttonMaterialFriction->setDynamicFriction(0.9);
    buttonMaterialFriction->setTextureLevel(1);
    buttonMaterialFriction->setHapticTriangleSides(true, false);

    buttonMaterialPlain = cMaterial::create();
    buttonMaterialPlain->setRedLightCoral();
    buttonMaterialPlain->setHapticTriangleSides(true, false);




    /////////////////////////////////////////////////////////////////////////
    // CREATE BUTTONS
    /////////////////////////////////////////////////////////////////////////

    setupDemo();

    /////////////////////////////////////////////////////////////////////////
    // OBJECT BACKGROUND:
    ////////////////////////////////////////////////////////////////////////

    // create a mesh
    objectBackground = new cMesh();
    // create plane
    cCreatePlane(objectBackground, 1.5, 1.5);

    // create collision detector

    objectBackground->createAABBCollisionDetector(toolRadius);
    // add object to world

    world->addChild(objectBackground);
    // set the position of the object
    objectBackground->setLocalPos(0.0, 0.0, -0.001);

    // enable texture mapping
    objectBackground->m_material->setWhite();

    //--------------------------------------------------------------------------
    // WIDGETS
    //--------------------------------------------------------------------------

    // create a font
    font = NEW_CFONT_CALIBRI_32();

    // create a label to display the haptic and graphic rate of the simulation
    labelRates = new cLabel(font);
    // camera->m_frontLayer->addChild(labelRates);

    // label to show the round count
    labelRound = new cLabel(font);
    camera->m_frontLayer->addChild(labelRound);

    // label to show the round count
    labelRoundTime = new cLabel(font);
    camera->m_frontLayer->addChild(labelRoundTime);

    labelHapticsOn = new cLabel(NEW_CFONT_CONSOLAS_16());
    camera->m_frontLayer->addChild(labelHapticsOn);

    // create a background
    background = new cBackground();
    camera->m_backLayer->addChild(background);

    // set background properties
    background->setCornerColors(cColorf(0.3, 0.3, 0.3),
        cColorf(0.2, 0.2, 0.2),
        cColorf(0.1, 0.1, 0.1),
        cColorf(0.0, 0.0, 0.0));

    //--------------------------------------------------------------------------
    // VIEWPORT DISPLAY
    //--------------------------------------------------------------------------

    // get content scale factor
    float contentScaleW, contentScaleH;
    glfwGetWindowContentScale(window, &contentScaleW, &contentScaleH);

    // create a viewport to display the scene.
    viewport = new cViewport(camera, contentScaleW, contentScaleH);


    //--------------------------------------------------------------------------
    // START HAPTIC SIMULATION THREAD
    //--------------------------------------------------------------------------

    // create a thread which starts the main haptics rendering loop
    hapticsThread = new cThread();
    hapticsThread->start(renderHaptics, CTHREAD_PRIORITY_HAPTICS);

    // setup callback when application exits
    atexit(close);

    //--------------------------------------------------------------------------
    // MAIN GRAPHIC LOOP
    //--------------------------------------------------------------------------

    // main graphic loop
    while (!glfwWindowShouldClose(window)) {
        // render graphics
        renderGraphics();

        // process events
        glfwPollEvents();
    }

    // close window
    glfwDestroyWindow(window);

    // terminate GLFW library
    glfwTerminate();

    // exit
    return 0;
}

void createButtons() {
    for (auto& button : buttons) {
        button = ButtonState();
        button.mesh = new cMesh();
        cCreatePlane(button.mesh, BUTTON_WIDTH, BUTTON_HEIGHT);

        button.mesh->createAABBCollisionDetector(toolRadius);
        world->addChild(button.mesh);

        button.mesh->setLocalPos(0.0, 0.0, 0.0);

        // enable texture
        if (hapticsOn) {
            button.mesh->m_texture = buttonTexture;
            button.mesh->setUseTexture(false);
            button.mesh->m_normalMap = buttonNormalMap;
        }

        button.active = true;
        button.mesh->setMaterial(buttonMaterialFriction->copy());
    }
}


void randomizeButtonPositions() {
    // Calculate world dimensions based on window aspect ratio
    double aspectRatio = (double)windowW / windowH;
    double worldHeight = 0.8; // Base height in world coordinates
    double worldWidth = worldHeight * aspectRatio; // Width adjusted by aspect ratio

    // Margins in world coordinates
    double topMargin = 0.1; // 30px from top in world coords
    double bottomMargin = 0.1; // 30px from bottom in world coords
    double minDistance = 0.20; // Minimum 50px apart in world coords

    // Area where buttons can be placed
    double minX = -worldWidth / 2 + BUTTON_WIDTH / 2;
    double maxX = worldWidth / 2 - BUTTON_WIDTH / 2;
    double minY = -worldHeight / 2 + BUTTON_HEIGHT / 2 + bottomMargin;
    double maxY = worldHeight / 2 - BUTTON_HEIGHT / 2 - topMargin;

    // Store positions of already placed buttons
    vector<cVector3d> placedPositions;

    // Seed random generator
    srand(static_cast<unsigned int>(time(NULL)));

    // Place each button
    for (int i = 0; i < NUM_BUTTONS; i++) {
        cVector3d newPos;
        bool validPosition = false;
        int attempts = 0;
        const int maxAttempts = 100;

        // Try to find valid position
        while (!validPosition && attempts < maxAttempts) {
            // Generate random position
            double x = minX + ((double)rand() / RAND_MAX) * (maxX - minX);
            double y = minY + ((double)rand() / RAND_MAX) * (maxY - minY);
            newPos = cVector3d(x, y, 0.0);

            // Check if position is valid using Euclidean distance
            validPosition = true;
            for (const auto& pos : placedPositions) {
                double distance = sqrt(pow(pos.x() - newPos.x(), 2) +
                    pow(pos.y() - newPos.y(), 2));
                if (distance < minDistance) {
                    validPosition = false;
                    break;
                }
            }
            attempts++;
        }

        // Position the button
        buttons[i].mesh->setLocalPos(newPos);
        placedPositions.push_back(newPos);
    }
}

void removeAllButtons() {
    for (auto& button : buttons) {
        if (button.mesh == nullptr) continue;

        world->removeChild(button.mesh);
        delete button.mesh;
        button.mesh = nullptr;
    }
}

void setupDemo() {
    removeAllButtons();
    createButtons();
    randomizeButtonPositions();
}

//------------------------------------------------------------------------------

void onWindowSizeCallback(GLFWwindow* a_window, int a_width, int a_height) {
    // update window size
    windowW = a_width;
    windowH = a_height;

    // render scene
    renderGraphics();
}

//------------------------------------------------------------------------------

void onFrameBufferSizeCallback(GLFWwindow* a_window, int a_width, int a_height) {
    // update frame buffer size
    framebufferW = a_width;
    framebufferH = a_height;
}

//------------------------------------------------------------------------------

void onWindowContentScaleCallback(GLFWwindow* a_window, float a_xscale, float a_yscale) {
    // update window content scale factor
    viewport->setContentScale(a_xscale, a_yscale);
}

//------------------------------------------------------------------------------

void onErrorCallback(int a_error, const char* a_description) {
    cout << "Error: " << a_description << endl;
}

//------------------------------------------------------------------------------

void onKeyCallback(GLFWwindow* a_window, int a_key, int a_scancode, int a_action, int a_mods) {
    // filter calls that only include a key press
    if ((a_action != GLFW_PRESS) && (a_action != GLFW_REPEAT)) return;

    // ESC || Q key > option - exit
    if ((a_key == GLFW_KEY_ESCAPE) || (a_key == GLFW_KEY_Q)) {
        glfwSetWindowShouldClose(a_window, GLFW_TRUE);
        return;
    }

    // F key > option - toggle fullscreen
    if (a_key == GLFW_KEY_F) {
        // toggle state variable
        fullscreen = !fullscreen;

        // get handle to monitor
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();

        // get information about monitor
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        // set fullscreen or window mode
        if (fullscreen) {
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        else {
            int w = 0.8 * mode->height;
            int h = 0.5 * mode->height;
            int x = 0.5 * (mode->width - w);
            int y = 0.5 * (mode->height - h);
            glfwSetWindowMonitor(window, NULL, x, y, w, h, mode->refreshRate);
        }

        // set the desired swap interval and number of samples to use for multisampling
        glfwSwapInterval(swapInterval);
        glfwWindowHint(GLFW_SAMPLES, 4);
        return;
    }

    //RESET DEMO
    if (a_key == GLFW_KEY_R) {
        setupDemo();
    }

    //TOGGLE HAPTICS
    if (a_key == GLFW_KEY_T) {
        hapticsOn = !hapticsOn;

        if (!hapticsOn) {
            buttonMaterialFriction = buttonMaterialPlain;
            setupDemo();
            return;
        }

        buttonMaterialFriction = cMaterial::create();
        buttonMaterialFriction->setRedLightCoral();
        buttonMaterialFriction->setStiffness(0.5 * maxStiffness);
        buttonMaterialFriction->setStaticFriction(0.3);
        buttonMaterialFriction->setDynamicFriction(0.9);
        buttonMaterialFriction->setTextureLevel(1);
        buttonMaterialFriction->setHapticTriangleSides(true, false);

        setupDemo();
    }
}

//------------------------------------------------------------------------------

void close(void) {
    // stop the simulation
    simulationRunning = false;

    // wait for graphics and haptics loops to terminate
    while (!simulationFinished) { cSleepMs(100); }

    // close haptic device
    tool->stop();


    // delete resources
    delete hapticsThread;
    delete world;
    delete handler;
}

//------------------------------------------------------------------------------

void renderGraphics(void) {
    // sanity check
    if (viewport == nullptr) { return; }

    /////////////////////////////////////////////////////////////////////
    // UPDATE WIDGETS
    /////////////////////////////////////////////////////////////////////

    // get width and height of CHAI3D internal rendering buffer
    int displayW = viewport->getDisplayWidth();
    int displayH = viewport->getDisplayHeight();

    // update haptic and graphic rate data
    // labelRates->setText(cStr(freqCounterGraphics.getFrequency(), 0) + " Hz / " +
    //     cStr(freqCounterHaptics.getFrequency(), 0) + " Hz");
    //
    // // update position of label
    // labelRates->setLocalPos((int)(0.5 * (displayW - labelRates->getWidth())), 15);

    //? Label Round
    labelRound->setText("DEMO ENVIRONMENT");
    labelRound->setLocalPos(static_cast<int>(0.5 * (displayW - labelRound->getWidth())),
        displayH - labelRound->getHeight() - 10);

    //? Label Round Timing
    labelHapticsOn->setText(hapticsOn ? "." : "");
    labelHapticsOn->setLocalPos(0,0);

    /////////////////////////////////////////////////////////////////////
    // RENDER SCENE
    /////////////////////////////////////////////////////////////////////

    // update shadow maps (if any)
    world->updateShadowMaps(false, mirroredDisplay);

    // render world
    viewport->renderView(framebufferW, framebufferH);

    // wait until all GL commands are completed
    glFinish();

    // check for any OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) cout << "Error: " << gluErrorString(error) << endl;

    // swap buffers
    glfwSwapBuffers(window);

    // signal frequency counter
    freqCounterGraphics.signal(1);
}

//------------------------------------------------------------------------------

enum cMode {
    IDLE,
    SELECTION
};

void renderHaptics(void) {
    cMultiMesh* object;

    cMode state = IDLE;
    cGenericObject* selectedObject = NULL;
    cTransform tool_T_object;

    // simulation in now running
    simulationRunning = true;
    simulationFinished = false;

    const double MIN_TOOL_Z_WORLD = -0.005;
    const double MAX_TOOL_Z_WORLD = 0.001;

    // main haptic simulation loop
    while (simulationRunning) {
        /////////////////////////////////////////////////////////////////////////
        // HAPTIC RENDERING
        /////////////////////////////////////////////////////////////////////////

        // signal frequency counter
        freqCounterHaptics.signal(1);

        // Compute global reference frames for each object in the world
        world->computeGlobalPositions(true);

        // 1. Update position and orientation of the tool from the actual haptic device.
        tool->updateFromDevice();

        // 2. Get the global position of the haptic device, as just read and stored by the tool.
        cVector3d currentDeviceGlobalPos = tool->getDeviceGlobalPos();

        // 3. Clamp the Z-component of the device's position to the desired range.
        // The chai3d::cClamp function ensures the value is within [min, max].
        double clampedDeviceZ = cClamp(currentDeviceGlobalPos.z(), MIN_TOOL_Z_WORLD, MAX_TOOL_Z_WORLD);

        // 4. Create a new "device" position using the device's X and Y, but our clamped Z.
        cVector3d modifiedDeviceGlobalPos(
            currentDeviceGlobalPos.x(), // X from device
            currentDeviceGlobalPos.y(), // Y from device
            clampedDeviceZ // Z is clamped to the defined range
        );

        // 5. Set this modified position back as the tool's understanding of the device's global position.
        // This effectively overrides the physical device's Z-axis input outside the allowed range
        // for the subsequent haptic calculations.
        tool->setDeviceGlobalPos(modifiedDeviceGlobalPos);

        tool->computeInteractionForces();

        // --- START CUSTOM GRAVITY WELL FORCE ---
        cVector3d cumulativeGravityForce(0, 0, 0); // Initialize force for this frame
        if (!hapticsOn) {
            tool->setForcesOFF();
        } else {
            cVector3d toolProxyPos = tool->m_hapticPoint->getGlobalPosProxy(); // Use proxy for distance
            double minQualifyingDistance = GRAVITY_WELL_ACTIVE_RADIUS + 1.0; // Sentinel for closest button
            int bestButtonIdx = -1;

            // Determine which button's well is dominant (closest)
            for (int i = 0; i < NUM_BUTTONS; ++i) {
                if (buttons[i].mesh && buttons[i].active && !buttons[i].clicked) {
                    cVector3d buttonCenterPos = buttons[i].mesh->getGlobalPos();
                    double distanceToCenter = cDistance(toolProxyPos, buttonCenterPos);

                    if (distanceToCenter < GRAVITY_WELL_ACTIVE_RADIUS) {
                        if (distanceToCenter < minQualifyingDistance) {
                            minQualifyingDistance = distanceToCenter;
                            bestButtonIdx = i;
                        }
                    }
                }
            }

            // If a suitable button for the gravity well is found
            if (bestButtonIdx != -1) {
                cVector3d buttonCenterPos = buttons[bestButtonIdx].mesh->getGlobalPos();
                cVector3d vecToButton = buttonCenterPos - toolProxyPos;
                double actualDistanceToCenter = minQualifyingDistance; // This is the distance to the chosen button's center

                double forceMagnitude = 0.0;
                if (actualDistanceToCenter <= GRAVITY_WELL_CENTER_DEAD_ZONE_RADIUS) {
                    // Linearly taper force to zero within the dead zone
                    if (GRAVITY_WELL_CENTER_DEAD_ZONE_RADIUS > 0.0001) { // Avoid division by zero
                        forceMagnitude = GRAVITY_WELL_FORCE_MAGNITUDE * (actualDistanceToCenter / GRAVITY_WELL_CENTER_DEAD_ZONE_RADIUS);
                    }
                    else {
                        forceMagnitude = 0.0; // No force if dead zone is zero (or tool is exactly at center)
                    }
                }
                else {
                    // Constant force outside dead zone but within active radius
                    forceMagnitude = GRAVITY_WELL_FORCE_MAGNITUDE;
                }

                // Check if the vector to button is not zero before normalizing
                if (vecToButton.lengthsq() > 0.00000001 && forceMagnitude > 0.00001) { // Use lengthsq() and a small epsilon
                    cVector3d forceDirection = vecToButton;
                    forceDirection.normalize();
                    cumulativeGravityForce = forceDirection * forceMagnitude;
                    cumulativeGravityForce.z(0.0); // Make the force act only in the XY plane
                }
            }

            if (cumulativeGravityForce.lengthsq() > 0.00000001) { // Check squared length against epsilon
                tool->setForcesON();
                tool->addDeviceGlobalForce(cumulativeGravityForce);
            }
        }
        // --- END CUSTOM GRAVITY WELL FORCE ---


        /////////////////////////////////////////////////////////////////////////
        // MANIPULATION
        /////////////////////////////////////////////////////////////////////////
        bool button = tool->getUserSwitch(0);

        //
        // STATE 1:
        // Idle mode - user presses the user switch
        //
        if ((state == IDLE) && (button == true)) {
            cout << "button clicked" << endl;
            // check if at least one contact has occurred
            if (tool->m_hapticPoint->getNumCollisionEvents() > 0) {
                // get contact event
                cCollisionEvent* collisionEvent = tool->m_hapticPoint->getCollisionEvent(0);

                // get object from contact event
                selectedObject = collisionEvent->m_object;

                for (auto & demoButton : buttons) {
                    if (selectedObject == demoButton.mesh && !demoButton.clicked && demoButton.active) {
                        demoButton.clicked = true;
                        demoButton.mesh->setMaterial(buttonMaterialPlain->copy());
                        demoButton.mesh->m_material->setGreenLight();
                        break;
                    }
                }
            }

            // update state
            state = SELECTION;
        }


        //
        // STATE 2:
        // Selection mode - operator maintains user switch enabled and moves object
        //
        else if ((state == SELECTION) && (button == true)) {
        }

        //
        // STATE 3:
        // Finalize Selection mode - operator releases user switch.
        //
        else {
            state = IDLE;
        }


        /////////////////////////////////////////////////////////////////////////
        // FINALIZE
        /////////////////////////////////////////////////////////////////////////

        // send forces to haptic device
        tool->applyToDevice();
    }

    // exit haptics thread
    simulationFinished = true;
}

void onMouseButtonCallback(GLFWwindow* a_window, int a_button, int a_action, int a_mods) {
    if (a_button == GLFW_MOUSE_BUTTON_LEFT && a_action == GLFW_PRESS) {
        // store mouse position
        glfwGetCursorPos(window, &mouseX, &mouseY);

        // variable for storing collision information
        cCollisionRecorder recorder;
        cCollisionSettings settings;
        cGenericObject* selectedObject = NULL;

        // detect for any collision between mouse and world
        const bool hit = camera->selectWorld(mouseX, (windowH - mouseY), windowW, windowH, recorder, settings);
        if (hit) {
            selectedObject = recorder.m_nearestCollision.m_object;

            for (auto & demoButton : buttons) {
                if (selectedObject == demoButton.mesh && !demoButton.clicked && demoButton.active) {
                    demoButton.clicked = true;
                    demoButton.mesh->setMaterial(buttonMaterialPlain->copy());
                    demoButton.mesh->m_material->setGreenLight();
                    break;
                }
            }
        }
    }
}

//------------------------------------------------------------------------------
