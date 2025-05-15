//------------------------------------------------------------------------------
#include "chai3d.h"
//------------------------------------------------------------------------------
#include <GLFW/glfw3.h>
//------------------------------------------------------------------------------
using namespace chai3d;
using namespace std;
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

const int NUM_BUTTONS = 4;
const double BUTTON_WIDTH = 0.15;
const double BUTTON_HEIGHT = 0.05;

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

// a few mesh objects
cMesh* object0;
cMesh* object1;
cMesh* object2;
cMesh* object3;
cMesh* objectBackground;
cMesh* buttons[NUM_BUTTONS];

// a colored background
cBackground* background;

// a font for rendering text
cFontPtr font;

// a label to display the rate [Hz] at which the simulation is running
cLabel* labelRates;

// a flag that indicates if the haptic simulation is currently running
bool simulationRunning = false;

// a flag that indicates if the haptic simulation has terminated
bool simulationFinished = true;

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

// callback when window content scaling is modified
void onWindowContentScaleCallback(GLFWwindow* a_window, float a_xscale, float a_yscale);

// this function renders the scene
void renderGraphics(void);

// this function contains the main haptics simulation loop
void renderHaptics(void);

// this function closes the application
void close(void);


//==============================================================================
/*
    DEMO:   14-textures.cpp

    This example illustrates the use of haptic textures projected onto mesh
    surfaces.
*/
//==============================================================================

int main(int argc, char* argv[])
{
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
    cout << "[m] - Enable/Disable vertical mirroring" << endl;
    cout << "[q] - Exit application" << endl;
    cout << endl << endl;

    // get current path
    bool fileload;
    string currentpath = cGetCurrentPath();


    //--------------------------------------------------------------------------
    // OPEN GL - WINDOW DISPLAY
    //--------------------------------------------------------------------------

    // initialize GLFW library
    if (!glfwInit())
    {
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
    if (stereoMode == C_STEREO_ACTIVE)
    {
        glfwWindowHint(GLFW_STEREO, GL_TRUE);
    }
    else
    {
        glfwWindowHint(GLFW_STEREO, GL_FALSE);
    }

    // create display context
    window = glfwCreateWindow(windowW, windowH, "CHAI3D", NULL, NULL);
    if (!window)
    {
        cout << "failed to create window" << endl;
        cSleepMs(1000);
        glfwTerminate();
        return 1;
    }

    // set GLFW key callback
    glfwSetKeyCallback(window, onKeyCallback);

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
    camera->set(cVector3d(0.0, 0.0, 1.0),    // camera position (eye)
        cVector3d(0.0, 0.0, 0.0),    // lookat position (target)
        cVector3d(0.0, 1.0, 0.0));   // direction of the (up) vector

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

    // set radius of tool
    double toolRadius = 0.01;

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
    double maxStiffness = hapticDeviceInfo.m_maxLinearStiffness / workspaceScaleFactor;

    /////////////////////////////////////////////////////////////////////////
    // CREATE BUTTONS
    /////////////////////////////////////////////////////////////////////////

    for (int i = 0; i < NUM_BUTTONS; i++) {
		buttons[i] = new cMesh();
		cCreatePlane(buttons[i], BUTTON_WIDTH, BUTTON_HEIGHT);

		buttons[i]->createAABBCollisionDetector(toolRadius);
		world->addChild(buttons[i]);

		buttons[i]->setLocalPos(0.0, 0.0 + (0.1 * i), 0.0);
		buttons[i]->m_texture = cTexture2d::create();
		fileload = buttons[i]->m_texture->loadFromFile(currentpath + "../resources/images/sand.jpg");

        if (!fileload)
        {
            cout << "Error - Texture image failed to load correctly." << endl;
            close();
            return (-1);
        }

        // enable texture mapping
        buttons[i]->setUseTexture(false);
        buttons[i]->m_material->setPink();

        // create normal map from texture data
        cNormalMapPtr normalMap0 = cNormalMap::create();
        normalMap0->createMap(buttons[i]->m_texture);
        buttons[i]->m_normalMap = normalMap0;

        // set haptic properties
        buttons[i]->m_material->setStiffness(0.5 * maxStiffness);
        buttons[i]->m_material->setStaticFriction(0.3);
        buttons[i]->m_material->setDynamicFriction(0.6);
        buttons[i]->m_material->setTextureLevel(1);
        buttons[i]->m_material->setHapticTriangleSides(true, false);

    }

    /////////////////////////////////////////////////////////////////////////
    // OBJECT 0:
    /////////////////////////////////////////////////////////////////////////

    // create a mesh
    object0 = new cMesh();

    // create plane
    cCreatePlane(object0, 0.15, 0.05);

    // create collision detector
    object0->createAABBCollisionDetector(toolRadius);

    // add object to world
    //world->addChild(object0);

    // set the position of the object
    object0->setLocalPos(-0.2, -0.2, 0.0);

    // set graphic properties
    object0->m_texture = cTexture2d::create();
    fileload = object0->m_texture->loadFromFile(currentpath + "../resources/images/sand.jpg");

    if (!fileload)
    {
        cout << "Error - Texture image failed to load correctly." << endl;
        close();
        return (-1);
    }

    // enable texture mapping
    object0->setUseTexture(false);
    object0->m_material->setPink();

    // create normal map from texture data
    cNormalMapPtr normalMap0 = cNormalMap::create();
    normalMap0->createMap(object0->m_texture);
    object0->m_normalMap = normalMap0;

    // set haptic properties
    object0->m_material->setStiffness(0.5 * maxStiffness);
    object0->m_material->setStaticFriction(0.3);
    object0->m_material->setDynamicFriction(0.3);
    object0->m_material->setTextureLevel(0.6);
    object0->m_material->setHapticTriangleSides(true, false);


    /////////////////////////////////////////////////////////////////////////
    // OBJECT 1:
    ////////////////////////////////////////////////////////////////////////

    // create a mesh
    object1 = new cMesh();

    // create plane
    cCreatePlane(object1, 0.3, 0.3);

    // create collision detector
    object1->createAABBCollisionDetector(toolRadius);

    // add object to world
    //world->addChild(object1);

    // set the position of the object
    object1->setLocalPos(0.2, -0.2, 0.0);

    // set graphic properties
    object1->m_texture = cTexture2d::create();
    fileload = object1->m_texture->loadFromFile(currentpath + "../resources/images/whitefoam.jpg");
    if (!fileload)
    {
        cout << "Error - Texture image failed to load correctly." << endl;
        close();
        return (-1);
    }

    // enable texture mapping
    object1->setUseTexture(false);
    object1->m_material->setPink();

    // create normal map from texture data
    cNormalMapPtr normalMap1 = cNormalMap::create();
    normalMap1->createMap(object1->m_texture);
    object1->m_normalMap = normalMap1;

    // set haptic properties
    object1->m_material->setStiffness(0.1 * maxStiffness);
    object1->m_material->setStaticFriction(0.3);
    object1->m_material->setDynamicFriction(0.3);
    object1->m_material->setTextureLevel(0.8);
    object1->m_material->setHapticTriangleSides(true, false);


    /////////////////////////////////////////////////////////////////////////
    // OBJECT 2:
    /////////////////////////////////////////////////////////////////////////

    // create a mesh
    object2 = new cMesh();

    // create plane
    cCreatePlane(object2, 0.3, 0.3);

    // create collision detector
    object2->createAABBCollisionDetector(toolRadius);

    // add object to world
    //world->addChild(object2);

    // set the position of the object
    object2->setLocalPos(0.2, 0.2, 0.0);

    // set graphic properties
    object2->m_texture = cTexture2d::create();
    fileload = object2->m_texture->loadFromFile(currentpath + "../resources/images/brownboard.jpg");
    if (!fileload)
    {
        cout << "Error - Texture image failed to load correctly." << endl;
        close();
        return (-1);
    }

    // enable texture mapping
    object2->setUseTexture(true);
    object2->m_material->setWhite();

    // create normal map from texture data
    cNormalMapPtr normalMap2 = cNormalMap::create();
    normalMap2->createMap(object2->m_texture);
    object2->m_normalMap = normalMap2;

    // set haptic properties
    object2->m_material->setStiffness(0.2 * maxStiffness);
    object2->m_material->setStaticFriction(0.3);
    object2->m_material->setDynamicFriction(0.3);
    object2->m_material->setTextureLevel(0.2);
    object2->m_material->setHapticTriangleSides(true, false);


    /////////////////////////////////////////////////////////////////////////
    // OBJECT 3:
    ////////////////////////////////////////////////////////////////////////

    // create a mesh
    object3 = new cMesh();

    // create plane
    cCreatePlane(object3, 0.3, 0.3);

    // create collision detector
    object3->createAABBCollisionDetector(toolRadius);

    // add object to world
    //world->addChild(object3);

    // set the position of the object
    object3->setLocalPos(-0.2, 0.2, 0.0);

    // set graphic properties
    object3->m_texture = cTexture2d::create();
    fileload = object3->m_texture->loadFromFile(currentpath + "../resources/images/blackstone.jpg");
    if (!fileload)
    {
        cout << "Error - Texture image failed to load correctly." << endl;
        close();
        return (-1);
    }

    // enable texture mapping
    object3->setUseTexture(true);
    object3->m_material->setWhite();

    // create normal map from texture data
    cNormalMapPtr normalMap3 = cNormalMap::create();
    normalMap3->createMap(object3->m_texture);
    object3->m_normalMap = normalMap3;
    normalMap3->setTextureUnit(GL_TEXTURE0_ARB);

    // set haptic properties
    object3->m_material->setStiffness(0.7 * maxStiffness);
    object3->m_material->setStaticFriction(0.4);
    object3->m_material->setDynamicFriction(0.3);
    object3->m_material->setTextureLevel(0.3);
    object3->m_material->setHapticTriangleSides(true, false);

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

    objectBackground->setUseTexture(true);

    objectBackground->m_material->setWhite();



    // create normal map from texture data

    cNormalMapPtr normalMapBackground = cNormalMap::create();

    normalMapBackground->createMap(objectBackground->m_texture);

    objectBackground->m_normalMap = normalMapBackground;



    // set haptic properties

    objectBackground->m_material->setStiffness(0.1 * maxStiffness);

    objectBackground->m_material->setStaticFriction(0.3);

    objectBackground->m_material->setDynamicFriction(0.3);

    objectBackground->m_material->setTextureLevel(0.8);

    objectBackground->m_material->setHapticTriangleSides(true, false);

    //--------------------------------------------------------------------------
    // WIDGETS
    //--------------------------------------------------------------------------

    // create a font
    font = NEW_CFONT_CALIBRI_20();

    // create a label to display the haptic and graphic rate of the simulation
    labelRates = new cLabel(font);
    camera->m_frontLayer->addChild(labelRates);

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
    while (!glfwWindowShouldClose(window))
    {
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

//------------------------------------------------------------------------------

void onWindowSizeCallback(GLFWwindow* a_window, int a_width, int a_height)
{
    // update window size
    windowW = a_width;
    windowH = a_height;

    // render scene
    renderGraphics();
}

//------------------------------------------------------------------------------

void onFrameBufferSizeCallback(GLFWwindow* a_window, int a_width, int a_height)
{
    // update frame buffer size
    framebufferW = a_width;
    framebufferH = a_height;
}

//------------------------------------------------------------------------------

void onWindowContentScaleCallback(GLFWwindow* a_window, float a_xscale, float a_yscale)
{
    // update window content scale factor
    viewport->setContentScale(a_xscale, a_yscale);
}

//------------------------------------------------------------------------------

void onErrorCallback(int a_error, const char* a_description)
{
    cout << "Error: " << a_description << endl;
}

//------------------------------------------------------------------------------

void onKeyCallback(GLFWwindow* a_window, int a_key, int a_scancode, int a_action, int a_mods)
{
    // filter calls that only include a key press
    if ((a_action != GLFW_PRESS) && (a_action != GLFW_REPEAT))
    {
        return;
    }

    // option - exit
    else if ((a_key == GLFW_KEY_ESCAPE) || (a_key == GLFW_KEY_Q))
    {
        glfwSetWindowShouldClose(a_window, GLFW_TRUE);
    }

    // option - toggle fullscreen
    else if (a_key == GLFW_KEY_F)
    {
        // toggle state variable
        fullscreen = !fullscreen;

        // get handle to monitor
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();

        // get information about monitor
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        // set fullscreen or window mode
        if (fullscreen)
        {
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        else
        {
            int w = 0.8 * mode->height;
            int h = 0.5 * mode->height;
            int x = 0.5 * (mode->width - w);
            int y = 0.5 * (mode->height - h);
            glfwSetWindowMonitor(window, NULL, x, y, w, h, mode->refreshRate);
        }

        // set the desired swap interval and number of samples to use for multisampling
        glfwSwapInterval(swapInterval);
        glfwWindowHint(GLFW_SAMPLES, 4);
    }

    // option - toggle vertical mirroring
    else if (a_key == GLFW_KEY_M)
    {
        mirroredDisplay = !mirroredDisplay;
        camera->setMirrorVertical(mirroredDisplay);
    }
}

//------------------------------------------------------------------------------

void close(void)
{
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

void renderGraphics(void)
{
    // sanity check
    if (viewport == nullptr) { return; }

    /////////////////////////////////////////////////////////////////////
    // UPDATE WIDGETS
    /////////////////////////////////////////////////////////////////////

    // get width and height of CHAI3D internal rendering buffer
    int displayW = viewport->getDisplayWidth();
    int displayH = viewport->getDisplayHeight();

    // update haptic and graphic rate data
    labelRates->setText(cStr(freqCounterGraphics.getFrequency(), 0) + " Hz / " +
        cStr(freqCounterHaptics.getFrequency(), 0) + " Hz");

    // update position of label
    labelRates->setLocalPos((int)(0.5 * (displayW - labelRates->getWidth())), 15);


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

//void renderHaptics(void)
//{
//    // simulation in now running
//    simulationRunning = true;
//    simulationFinished = false;
//
//    // main haptic simulation loop
//    while (simulationRunning)
//    {
//        // compute global reference frames for each object
//        world->computeGlobalPositions(true);
//
//        // update position and orientation of tool
//        tool->updateFromDevice();
//
//        // compute interaction forces
//        tool->computeInteractionForces();
//
//        // send forces to haptic device
//        tool->applyToDevice();
//
//        // signal frequency counter
//        freqCounterHaptics.signal(1);
//    }
//
//    // exit haptics thread
//    simulationFinished = true;
//}

enum cMode
{
	IDLE,
	SELECTION
};

void renderHaptics(void)
{
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
    while (simulationRunning)
    {
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
            clampedDeviceZ              // Z is clamped to the defined range
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
        if ((state == IDLE) && (button == true))
        {
            cout << "button clicked" << endl;
            // check if at least one contact has occurred
            if (tool->m_hapticPoint->getNumCollisionEvents() > 0)
            {
                // get contact event
                cCollisionEvent* collisionEvent = tool->m_hapticPoint->getCollisionEvent(0);

                // get object from contact event
                selectedObject = collisionEvent->m_object;

                // determine which of the 4 objects in the scene was selected

				for (int i = 0; i < NUM_BUTTONS; i++)
				{
					if (selectedObject == buttons[i])
					{
                        cout << "button clicked " << i << endl;
                        buttons[i]->m_material->setBlack();
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
        else if ((state == SELECTION) && (button == true))
        {

        }

        //
        // STATE 3:
        // Finalize Selection mode - operator releases user switch.
        //
        else
        {
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

//------------------------------------------------------------------------------