//------------------------------------------------------------------------------
#include "chai3d.h"
//------------------------------------------------------------------------------
#include <fstream>
#include <iomanip>
#include <random>
#include <GLFW/glfw3.h>
#include <__random/random_device.h>
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

const string PARTICIPANT_NAME = "Daniel";
const double ROUND_TIME = 15.0; // seconds
const int NUMBER_OF_ROUNDS = 3; // rounds
const int NUM_BUTTONS = 5;

const double BUTTON_WIDTH = 0.15;
const double BUTTON_HEIGHT = 0.05;

//------------------------------------------------------------------------------
// DECLARED VARIABLES
//------------------------------------------------------------------------------

// a world that contains all objects of the virtual environment
cWorld *world;

// a camera to render the world in the window display
cCamera *camera;

// a viewport to display the scene viewed by the camera
cViewport *viewport = nullptr;

// a light source to illuminate the objects in the world
cDirectionalLight *light;

// a haptic device handler
cHapticDeviceHandler *handler;

// a pointer to the current haptic device
cGenericHapticDevicePtr hapticDevice;

// a virtual tool representing the haptic device in the scene
cToolCursor *tool;

struct ButtonState {
    cMesh *mesh;
    bool clicked = false;
    double clickTime = 0.0;
    int clickSequence = -1; // -1 means not clicked yet
    bool active = false;
};

// a few mesh objects
cMesh *startButton;
cMesh *objectBackground;
ButtonState buttons[NUM_BUTTONS];

cNormalMapPtr buttonNormalMap;
cTexture2dPtr buttonTexture;
cMaterialPtr buttonMaterial;

// a colored background
cBackground *background;

// a font for rendering text
cFontPtr font;

// a label to display the rate [Hz] at which the simulation is running
cLabel *labelRates;

// a label to display the round count
cLabel *labelRound;

// a label to display the time left in the round
cLabel *labelRoundTime;

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
cThread *hapticsThread;

// a handle to window display context
GLFWwindow *window = nullptr;

// current size of GLFW window
int windowW = 0;
int windowH = 0;

// current size of GLFW framebuffer
int framebufferW = 0;
int framebufferH = 0;

// swap interval for the display context (vertical synchronization)
int swapInterval = 1;

//? EXPERIMENT VARIABLES
string executionId;
int experimentNumber = 0; // 0 = all buttons light up, 1 = one button at a time lights up
int roundCount = 0;
int maxRounds = NUMBER_OF_ROUNDS;
bool timerActive;
double timeLeft;
double previousTime;

int clickCount; //number of clicks the user makes x round
int clickCountError; //number of clicks where the user didn't click on the correct button

int lastClickedButtonIndex; // -1 means no button clicked yet
int buttonsClicked;
double timerStartTime; // Time when timer was started (seconds)
double lastClickTime; // Time of last button click (seconds)

std::ofstream dataFile; // File for logging experiment data

//------------------------------------------------------------------------------
// DECLARED FUNCTIONS
//------------------------------------------------------------------------------

// callback when the window is resized
void onWindowSizeCallback(GLFWwindow *a_window, int a_width, int a_height);

// callback when the window framebuffer is resized
void onFrameBufferSizeCallback(GLFWwindow *a_window, int a_width, int a_height);

// callback when an error GLFW occurs
void onErrorCallback(int a_error, const char *a_description);

// callback when a key is pressed
void onKeyCallback(GLFWwindow *a_window, int a_key, int a_scancode, int a_action, int a_mods);

// callback to handle mouse click
void onMouseButtonCallback(GLFWwindow *a_window, int a_button, int a_action, int a_mods);

// callback when window content scaling is modified
void onWindowContentScaleCallback(GLFWwindow *a_window, float a_xscale, float a_yscale);

// this function renders the scene
void renderGraphics(void);

// this function contains the main haptics simulation loop
void renderHaptics(void);

// this function closes the application
void close(void);

void randomizeButtonPositions();

void createButtons();

void setupNewRound();

void removeAllButtons();

void destroyStartButton();

void createStartButton();

void startExperiment();

void writeRoundToFile();

string generateRunId();

void activateNewRandomButton();


//==============================================================================
/*
    DEMO:   14-textures.cpp

    This example illustrates the use of haptic textures projected onto mesh
    surfaces.
*/
//==============================================================================

int main(int argc, char *argv[]) {
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
    cout << "[s] - Start Task" << endl;
    cout << "[r] - Randomize Button Position" << endl;
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
    const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
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
    } else {
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
    tool->setWorkspaceRadius(1.0);

    // haptic forces are enabled only if small forces are first sent to the device;
    // this mode avoids the force spike that occurs when the application starts when
    // the tool is located inside an object for instance.
    tool->setWaitForSmallForce(true);

    // start the haptic tool
    tool->start();


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

    buttonMaterial = cMaterial::create();
    buttonMaterial->setRedDark();
    buttonMaterial->setStiffness(0.5 * maxStiffness);
    buttonMaterial->setStaticFriction(0.3);
    buttonMaterial->setDynamicFriction(0.6);
    buttonMaterial->setTextureLevel(1);
    buttonMaterial->setHapticTriangleSides(true, false);


    /////////////////////////////////////////////////////////////////////////
    // CREATE BUTTONS
    /////////////////////////////////////////////////////////////////////////

    setupNewRound();

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

    // create a background
    background = new cBackground();
    camera->m_backLayer->addChild(background);

    // set background properties
    background->setCornerColors(cColorf(0.3, 0.3, 0.3),
                                cColorf(0.2, 0.2, 0.2),
                                cColorf(0.1, 0.1, 0.1),
                                cColorf(0.0, 0.0, 0.0));

    //--------------------------------------------------------------------------
    // DATA FILE SETUP
    //--------------------------------------------------------------------------
    // Open data file in append mode
    executionId = generateRunId();
    dataFile.open(currentpath + "../resources/experiment_data.csv", std::ios_base::app);
    if (!dataFile.is_open()) {
        cout << "Failed to open data file for writing." << endl;
    }


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

string generateRunId() {
    // Get current time as seed enhancer
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000;

    // Create random generator with multiple entropy sources
    std::random_device rd;
    std::mt19937 gen(rd() ^
                    static_cast<unsigned int>(time_t_now) ^
                    static_cast<unsigned int>(millis));
    std::uniform_int_distribution<> dis(0, 15);

    // Generate a 16-character hex string
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (int i = 0; i < 16; ++i) {
        ss << dis(gen);
    }

    return ss.str();
}

void createButtons() {
    for (auto &button: buttons) {
        button = ButtonState();
        button.mesh = new cMesh();
        cCreatePlane(button.mesh, BUTTON_WIDTH, BUTTON_HEIGHT);

        button.mesh->createAABBCollisionDetector(toolRadius);
        world->addChild(button.mesh);

        button.mesh->setLocalPos(0.0, 0.0, 0.0);

        // enable texture mapping
        button.mesh->m_texture = buttonTexture;
        button.mesh->setUseTexture(false);
        button.mesh->m_normalMap = buttonNormalMap;

        // set haptic properties
        button.mesh->setMaterial(buttonMaterial->copy());

        if (experimentNumber == 0) {
            button.active = true;
            continue;
        }

        // experiment 1 setup
        button.mesh->m_material->setGrayLight();
        button.active = false;
    }

    // EXPERIMENT 1 > first button activation
    if (experimentNumber == 1) activateNewRandomButton();
}

void activateNewRandomButton() {
    // Randomly select a button to activate
    int randomIndex = rand() % NUM_BUTTONS;
    while (buttons[randomIndex].active) {
        randomIndex = rand() % NUM_BUTTONS;
    }
    buttons[randomIndex].active = true;
    buttons[randomIndex].mesh->m_material->setRedDark();
}

void removeAllButtons() {
    for (auto &button: buttons) {
        if (button.mesh == nullptr) continue;

        world->removeChild(button.mesh);
        delete button.mesh;
        button.mesh = nullptr;
    }
}

void randomizeButtonPositions() {
    // Calculate world dimensions based on window aspect ratio
    double aspectRatio = (double) windowW / windowH;
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
            double x = minX + ((double) rand() / RAND_MAX) * (maxX - minX);
            double y = minY + ((double) rand() / RAND_MAX) * (maxY - minY);
            newPos = cVector3d(x, y, 0.0);

            // Check if position is valid using Euclidean distance
            validPosition = true;
            for (const auto &pos: placedPositions) {
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

void createStartButton() {
    if (startButton != nullptr) return;
    startButton = new cMesh();
    cCreatePlane(startButton, BUTTON_WIDTH, BUTTON_HEIGHT);

    startButton->createAABBCollisionDetector(toolRadius);
    world->addChild(startButton);

    startButton->setLocalPos(0.0, 0.0, 0.0);

    // enable texture mapping
    startButton->m_texture = buttonTexture;
    startButton->setUseTexture(false);
    startButton->m_normalMap = buttonNormalMap;

    // set haptic properties
    startButton->setMaterial(buttonMaterial->copy());
}

void destroyStartButton() {
    if (startButton == nullptr) return;
    world->removeChild(startButton);
    delete startButton;
    startButton = nullptr;
}


void setupNewRound() {
    removeAllButtons();
    cout << "buttons removed" << endl;

    // Write data to file for the completed round (except for first round setup)
    if (roundCount > 0 && dataFile.is_open()) {
        writeRoundToFile();
        cout << "round saved" << endl;
    }


    timerActive = false;
    timeLeft = ROUND_TIME;
    previousTime = 0.0;

    lastClickedButtonIndex = -1; // -1 means no button clicked yet
    buttonsClicked = 0;
    timerStartTime = 0.0; // Time when timer was started (seconds)
    lastClickTime = 0.0; // Time of last button click (seconds)

    clickCount = 0;
    clickCountError = 0;

    roundCount++;
    cout << "variables initialized" << endl;

    createStartButton();

    if (roundCount <= maxRounds) return;

    if (experimentNumber == 0) {
        experimentNumber = 1;
        roundCount = 0;
        setupNewRound();
        cout << "reset everything for new experiment" << endl;
        return;
    }

    // End of experiment
    cout << "Experiments completed!" << endl;
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void writeRoundToFile() {
    dataFile << executionId << "," << PARTICIPANT_NAME << "," << experimentNumber << "," << NUM_BUTTONS << "," << NUMBER_OF_ROUNDS << "," << ROUND_TIME << "," << roundCount << ","
                << clickCount << "," << clickCountError;

    // Create a vector of button indices
    vector<int> clickedButtons;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (buttons[i].clicked) clickedButtons.push_back(i);
    }

    // Sort the indices by click sequence
    sort(clickedButtons.begin(), clickedButtons.end(),
         [](int a, int b) { return buttons[a].clickSequence < buttons[b].clickSequence; });

    // Print the information in order
    for (int idx: clickedButtons) dataFile << "," << buttons[idx].clickTime;

    dataFile << endl;
    dataFile.flush(); // Ensure data is written immediately
}

void startExperiment() {
    // Remove start button
    destroyStartButton();

    // Create buttons
    createButtons();

    // Randomize button positions
    randomizeButtonPositions();

    timerActive = true;
    timerStartTime = glfwGetTime(); // Store when timer started
    previousTime = timerStartTime; // For timer countdown
    cout << "Timer started!" << endl;
}

//------------------------------------------------------------------------------

void onWindowSizeCallback(GLFWwindow *a_window, int a_width, int a_height) {
    // update window size
    windowW = a_width;
    windowH = a_height;

    // render scene
    renderGraphics();
}

//------------------------------------------------------------------------------

void onFrameBufferSizeCallback(GLFWwindow *a_window, int a_width, int a_height) {
    // update frame buffer size
    framebufferW = a_width;
    framebufferH = a_height;
}

//------------------------------------------------------------------------------

void onWindowContentScaleCallback(GLFWwindow *a_window, float a_xscale, float a_yscale) {
    // update window content scale factor
    viewport->setContentScale(a_xscale, a_yscale);
}

//------------------------------------------------------------------------------

void onErrorCallback(int a_error, const char *a_description) {
    cout << "Error: " << a_description << endl;
}

//------------------------------------------------------------------------------

void onKeyCallback(GLFWwindow *a_window, int a_key, int a_scancode, int a_action, int a_mods) {
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
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();

        // get information about monitor
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);

        // set fullscreen or window mode
        if (fullscreen) {
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        } else {
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

    // S Key > start timer
    if (a_key == GLFW_KEY_S) {
        setupNewRound();
        timerActive = true;
        timerStartTime = glfwGetTime(); // Store when timer started
        previousTime = timerStartTime; // For timer countdown
        cout << "Timer started!" << endl;
    }


    if (a_key == GLFW_KEY_R) {
        randomizeButtonPositions();
    }
}

//------------------------------------------------------------------------------

void close(void) {
    // Close data file
    if (dataFile.is_open()) dataFile.close();

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

    // Update the timer if it's active
    if (timerActive) {
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - previousTime;
        previousTime = currentTime;

        if (timeLeft > 0.0) {
            timeLeft -= deltaTime;
            if (timeLeft < 0.0) {
                timeLeft = 0.0;
                timerActive = false;
                // Handle timer completion
                setupNewRound();
                cout << "Time's up for round " << roundCount << endl;
            }
        }
    }

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
    labelRound->setText(PARTICIPANT_NAME + " | Experiment " + cStr(experimentNumber) +  " | Round " + cStr(roundCount));
    labelRound->setLocalPos(static_cast<int>(0.5 * (displayW - labelRound->getWidth())),
                            displayH - labelRound->getHeight() - 10);

    //? Label Round Timing
    labelRoundTime->setText("Time left: " + cStr(timeLeft, 1) + " seconds");
    labelRoundTime->setLocalPos(static_cast<int>(0.5 * (displayW - labelRoundTime->getWidth())), 15);

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
    cMultiMesh *object;

    cMode state = IDLE;
    cGenericObject *selectedObject = NULL;
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
        chai3d::cVector3d currentDeviceGlobalPos = tool->getDeviceGlobalPos();

        // 3. Clamp the Z-component of the device's position to the desired range.
        // The chai3d::cClamp function ensures the value is within [min, max].
        double clampedDeviceZ = chai3d::cClamp(currentDeviceGlobalPos.z(), MIN_TOOL_Z_WORLD, MAX_TOOL_Z_WORLD);

        // 4. Create a new "device" position using the device's X and Y, but our clamped Z.
        chai3d::cVector3d modifiedDeviceGlobalPos(
            currentDeviceGlobalPos.x(), // X from device
            currentDeviceGlobalPos.y(), // Y from device
            clampedDeviceZ // Z is clamped to the defined range
        );

        // 5. Set this modified position back as the tool's understanding of the device's global position.
        // This effectively overrides the physical device's Z-axis input outside the allowed range
        // for the subsequent haptic calculations.
        tool->setDeviceGlobalPos(modifiedDeviceGlobalPos);

        tool->computeInteractionForces();


        /////////////////////////////////////////////////////////////////////////
        // MANIPULATION
        /////////////////////////////////////////////////////////////////////////

        // compute transformation from world to tool (haptic device)
        cTransform world_T_tool = tool->getDeviceGlobalTransform();

        // get status of user switch
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
                cCollisionEvent *collisionEvent = tool->m_hapticPoint->getCollisionEvent(0);

                // get object from contact event
                selectedObject = collisionEvent->m_object;

                if (!timerActive) {
                    if (selectedObject == startButton) {
                        startExperiment();
                    }
                } else {
                    clickCount++;
                    cout << "Click detected! Click count: " << clickCount << endl;

                    bool isCorrectButton = false;
                    for (int i = 0; i < NUM_BUTTONS; i++) {
                        if (selectedObject == buttons[i].mesh && !buttons[i].clicked && buttons[i].active) {
                            isCorrectButton = true;

                            buttons[i].clicked = true;
                            buttons[i].mesh->m_material->setGreenDark();

                            // Record click timing
                            const double currentTime = glfwGetTime();

                            buttons[i].clickTime = lastClickedButtonIndex == -1
                                                       ? (currentTime - timerStartTime) * 1000.0 // first button clicked
                                                       : (currentTime - lastClickTime) * 1000.0;
                            // subsequent button clicked

                            // Record sequence
                            buttons[i].clickSequence = buttonsClicked++;
                            lastClickTime = currentTime;
                            lastClickedButtonIndex = i;

                            if (buttonsClicked >= NUM_BUTTONS) setupNewRound();
                            else if (experimentNumber == 1) activateNewRandomButton();

                            break;
                        }
                    }

                    if (!isCorrectButton) {
                        clickCountError++;
                        cout << "Incorrect object clicked! Error count: " << clickCountError << endl;
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

void onMouseButtonCallback(GLFWwindow *a_window, int a_button, int a_action, int a_mods) {
    if (a_button == GLFW_MOUSE_BUTTON_LEFT && a_action == GLFW_PRESS) {
        // store mouse position
        glfwGetCursorPos(window, &mouseX, &mouseY);

        // variable for storing collision information
        cCollisionRecorder recorder;
        cCollisionSettings settings;
        cGenericObject *selectedObject = NULL;

        // detect for any collision between mouse and world
        const bool hit = camera->selectWorld(mouseX, (windowH - mouseY), windowW, windowH, recorder, settings);
        if (hit) {
            selectedObject = recorder.m_nearestCollision.m_object;

            if (!timerActive) {
                if (selectedObject == startButton) {
                    startExperiment();
                }
            } else {
                clickCount++;
                cout << "Click detected! Click count: " << clickCount << endl;

                bool isCorrectButton = false;
                for (int i = 0; i < NUM_BUTTONS; i++) {
                    if (selectedObject == buttons[i].mesh && !buttons[i].clicked && buttons[i].active) {
                        isCorrectButton = true;

                        buttons[i].clicked = true;
                        buttons[i].mesh->m_material->setGreenDark();

                        // Record click timing
                        const double currentTime = glfwGetTime();

                        buttons[i].clickTime = lastClickedButtonIndex == -1
                                                   ? (currentTime - timerStartTime) * 1000.0 // first button clicked
                                                   : (currentTime - lastClickTime) * 1000.0;
                        // subsequent button clicked

                        // Record sequence
                        buttons[i].clickSequence = buttonsClicked++;
                        lastClickTime = currentTime;
                        lastClickedButtonIndex = i;

                        if (buttonsClicked >= NUM_BUTTONS) setupNewRound();
                        else if (experimentNumber == 1) activateNewRandomButton();

                        break;
                    }
                }

                if (!isCorrectButton) {
                    clickCountError++;
                    cout << "Incorrect object clicked! Error count: " << clickCountError << endl;
                }
            }
        }
    }
}

//------------------------------------------------------------------------------
