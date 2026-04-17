#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include "basic_camera.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


// ?? Constants ?????????????????????????????????????????????????????????????????
const int   SCR_W = 1280, SCR_H = 720;
const float PI = 3.14159265f;

// ?? Camera – using BasicCamera from basic_camera.h ????????????????????????????????
// Extra state that BasicCamera does not store internally
static float  g_fov = 65.f;
static float  g_lastX = SCR_W / 2.f;
static float  g_lastY = SCR_H / 2.f;
static bool   g_firstMouse = true;
static bool  g_houseDoorOpen = false;
static float g_houseDoorFactor = 0.f;
// The main camera instance
BasicCamera camera(
    glm::vec3(0.f, 1.7f, 8.f),   // position
    glm::vec3(0.f, 1.f, 0.f),   // world-up
    -90.f,                         // yaw
    -2.f                           // pitch
);

// Thin compatibility shim so old code using "cam" still compiles
struct CamCompat {
    glm::vec3& pos;   glm::vec3& front;  glm::vec3& worldUp;
    float& yaw;   float& pitch;  float& roll;
    float& fov;   float& lastX;  float& lastY;
    bool& firstMouse;
    float      speed = 5.5f;
    float      sensitivity = 0.10f;

    // Explicit constructor required because member functions prevent aggregate init
    CamCompat(glm::vec3& p, glm::vec3& fr, glm::vec3& wu,
        float& y, float& pi, float& ro,
        float& fv, float& lx, float& ly, bool& fm)
        : pos(p), front(fr), worldUp(wu),
        yaw(y), pitch(pi), roll(ro),
        fov(fv), lastX(lx), lastY(ly),
        firstMouse(fm) {
    }

    void      updateDir() { camera.updateCameraVectors(); }
    glm::vec3 right() { return camera.Right; }
    glm::mat4 view() { return camera.GetViewMatrix(); }
};
static CamCompat cam(
    camera.Position, camera.Front, camera.WorldUp,
    camera.Yaw, camera.Pitch, camera.Roll,
    g_fov, g_lastX, g_lastY,
    g_firstMouse
);

// ?? Collision ?????????????????????????????????????????????????????????????????
struct Cylinder {
    glm::vec3 pos;
    float radius;
};
static std::vector<Cylinder> g_obstacles;
static void addObs(glm::vec3 p, float r) { g_obstacles.push_back({ p, r }); }
static bool checkObs(glm::vec3 p) {
    // Keep the path and doorway corridor traversable so player can enter house.
    // Allow camera through front door corridor (widens when door is open)
    float doorGap = 1.8f + g_houseDoorFactor * 2.5f;
    if (p.z < 8.0f && p.z > -22.0f && fabsf(p.x) < doorGap) return false;

    const float CAM_R = 0.4f; // Camera "body" radius
    for (auto& ob : g_obstacles) {
        float dx = p.x - ob.pos.x, dz = p.z - ob.pos.z;
        if (sqrtf(dx * dx + dz * dz) < (ob.radius + CAM_R)) return true;
    }
    return false;
}

// ?? Globals ???????????????????????????????????????????????????????????????????

static GLuint g_prog;
static GLuint texGrass;
static GLuint texMushroom;
static GLuint texFern;
static GLuint texLeaf;
static GLuint texLeaf2;
static GLuint texTreeBark;
static GLuint texFloor;
static GLuint texRooster = 0;
static bool   g_captured = false;
static bool   g_day = false;
static bool   g_rain = false;
static float  g_fogDensity = 0.0f; // Fog disabled
static float  g_thunderTimer = 0.0f; // Thunder flash duration
static float g_roosterWingAngle = 0.f;   // wing flap animation
static bool  g_roosterCrowing = false;
static float g_roosterCrowTimer = 0.f;
// ── Rooster Walk State ────────────────────────────────────────────────────────
static bool      g_roosterWalking = false;
static bool      g_roosterOnGround = false;
static float     g_roosterParabT = 0.f;    // 0→1 parabolic fall progress
static float     g_roosterParabDur = 1.4f;   // seconds to complete the arc
static glm::vec3 g_roosterWalkPos = { 0.30f, 0.0f, -14.0f };
// Parabolic arc: fixed start (ridge) and end (road in front of house)
static const glm::vec3 ROOSTER_START = { 0.30f, 7.72f, -18.0f };
static const glm::vec3 ROOSTER_LAND = { 0.30f, 0.0f,  -5.5f };
// ── Toy Rocket State ─────────────────────────────────────────────────────────
static glm::vec3 g_rocketBasePos = { 3.5f, 0.f, -5.0f }; // in front of house
static float     g_rocketY = 0.f;   // current lift offset
static float     g_rocketTargetY = 0.f;   // target Y (0 = ground, high = launched)
static bool      g_rocketUp = false; // is U held?
static float     g_rocketFlame = 0.f;   // flame flicker accumulator
static float g_rocketSpin = 0.f;  // current spin angle in radians
// ── Toy Car State ─────────────────────────────────────────────────────────
static glm::vec3 g_carPos = { 3.5f, 0.0f, 3.0f };
static float     g_carYaw = 180.0f;
static float     g_carSpeed = 0.0f;
static bool      g_carLightsOn = false;
static float     g_carWheelRot = 0.0f;
static float     g_carSteer = 0.0f;
static bool      g_carMovingBack = false;
static float     g_wheelSpin = 0.f;
static const float CAR_RADIUS = 0.55f;
static bool   g_orbitHouse = false;
static float  g_orbitAngle = 0.f;
static float  g_orbitRadius = 12.f;
static const glm::vec3 g_orbitTarget{ 0.f, 2.f, 0.f };
static bool   g_birdView = false;
static glm::vec3 g_birdSavedPos{ 0.f };
static float g_birdSavedYaw = -90.f, g_birdSavedPitch = -2.f, g_birdSavedRoll = 0.f;

static bool g_lightDir = true, g_lightPoint = true, g_lightSpot = true;
static bool g_lightAmb = true, g_lightDiff = true, g_lightSpec = true;
// ── Moon & Stars globals ───────────────────────────────────────────────────
static GLuint texMoon = 0;
static GLuint texMoon2 = 0;   // optional second moon texture
static float  g_moonAngle = 3.8f; // starting azimuth (south-ish)
static GLuint g_starVAO = 0;
static GLuint g_starVBO = 0;
static int    g_starCnt = 0;


static const glm::vec3 FIRE_POS = { 10.5f, 0.0f, -5.5f }; // between rock & mushrooms
// Interaction & Animation State

// 0=closed, 1=open (swings outward)// 0=closed, 1=open
static bool  g_ceilingFanOn = false;
static float g_fanBladeAngle = 0.f;
static float g_shutterSwing = 0.f; // 0=open, 1=closed (animated)
static bool  g_shutterTargetClosed = false;
static int   g_mushBobMode = 1; // 0=calm, 1=normal, 2=bouncy

// ?? Scene offsets – controls the S-curve layout ???????????????????????????????
static const glm::vec3 OFF_FOREST = { 0.f, 0.f, -15.f };  // ????? ???????
static const glm::vec3 OFF_CAMP = { -38.f, 0.f, +10.f };  // ??? ???? - ?????? ???? ????
static const glm::vec3 OFF_VILLAGE = { -50.f, 0.f, -40.f };  // ??? ????
static const glm::vec3 OFF_BARN = { -45.f, 0.f, +60.f };  // ?????? ??????? ????, ????????? ??
static const glm::vec3 OFF_TOWER = { +45.f, 0.f, +45.f };  // ????? ??????
static const glm::vec3 OFF_MED = { 0.f, 0.f, +40.f };     // ???? ?????? -30.f, 0.f, -40.f
static const glm::vec3 OFF_CASTLE = { +35.f, 0.f, +30.f }; // ????? ??????? ?????

static bool g_barnLanternsOn = true;

// ── UFO Animation State ──────────────────────────────────────────────────────
// The UFO hovers between west cluster-1 (~-16, -29) and cluster-2 (~-24, -50),
// midpoint is roughly (-20, -40).
static const glm::vec3 UFO_CENTER = { -32.f, 0.f, -37.f }; // midpoint cluster1/cluster2
static const float     UFO_TREE_H = 7.5f;   // approximate treetop height
static const float     UFO_BOB_LO = UFO_TREE_H + 0.3f;  // lowest hover (just above canopy)
static const float     UFO_BOB_HI = UFO_TREE_H + 5.5f;  // highest hover
static const float     UFO_BOB_SPD = 0.35f;  // bob cycles per second
static float           g_ufoSpinAngle = 0.f;  // dome/light rotation

// ── Firefly system ───────────────────────────────────────────────────────────
struct Firefly {
    glm::vec3 center;   // orbit center (near a tree base)
    float     radius;   // orbit radius
    float     speed;    // angular speed (rad/s)
    float     phase;    // starting angle offset
    float     height;   // Y height above ground
    glm::vec3 color;    // glow colour (warm yellow-green)
};
static std::vector<Firefly> g_fireflies;

static void initStars()
{
    srand(99);
    const int N = 700;
    struct SP { float x, y, z, bright; };
    std::vector<SP> pts;
    pts.reserve(N);
    for (int i = 0; i < N; i++) {
        float th = ((float)rand() / RAND_MAX) * 2.f * PI;
        float elev = glm::radians(8.f + ((float)rand() / RAND_MAX) * 82.f);
        float r = 160.f;
        float t = (float)rand() / RAND_MAX;
        pts.push_back({ r * cosf(elev) * cosf(th),
                        r * sinf(elev),
                        r * cosf(elev) * sinf(th),
                        0.15f + 0.85f * (t * t) });   // skew toward dim
    }
    g_starCnt = N;

    // 8 floats per vertex: pos(3) normal(3) uv(2)
    // uv.s = brightness, uv.t = 0
    std::vector<float> buf;
    buf.reserve(N * 8);
    for (auto& s : pts) {
        buf.push_back(s.x); buf.push_back(s.y); buf.push_back(s.z);
        buf.push_back(0.f); buf.push_back(1.f); buf.push_back(0.f);
        buf.push_back(s.bright); buf.push_back(0.f);
    }

    glGenVertexArrays(1, &g_starVAO);
    glGenBuffers(1, &g_starVBO);
    glBindVertexArray(g_starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_starVBO);
    glBufferData(GL_ARRAY_BUFFER, buf.size() * sizeof(float), buf.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}
static void initFireflies()
{
    // Seed positions around the main tree clusters
    struct Seed { float x, z; };
    static const Seed seeds[] = {
        // West clusters
        { -14.f, -28.f }, { -17.f, -31.f }, { -22.f, -48.f }, { -25.f, -51.f },
        { -20.f, -53.f }, { -32.f, -72.f }, { -35.f, -78.f }, { -18.f,-105.f },
        // East clusters
        {  15.f, -30.f }, {  19.f, -33.f }, {  24.f, -50.f }, {  28.f, -48.f },
        {  30.f, -75.f }, {  34.f, -79.f }, {  20.f,-108.f },
        // Campsite area trees
        { -18.f, -27.f }, { -21.f, -33.f }, { -26.f, -37.f },
    };
    // Palette of warm yellow-green firefly colours
    static const glm::vec3 palette[] = {
        { 0.95f, 1.00f, 0.30f },   // yellow-green
        { 0.80f, 1.00f, 0.20f },   // lime
        { 1.00f, 0.95f, 0.35f },   // warm yellow
        { 0.70f, 1.00f, 0.50f },   // soft green
    };
    srand(42);
    for (auto& s : seeds) {
        // 2-4 fireflies per tree seed
        int count = 2 + rand() % 3;
        for (int k = 0; k < count; k++) {
            Firefly ff;
            ff.center = { s.x + (rand() % 5 - 2) * 0.6f, 0.f, s.z + (rand() % 5 - 2) * 0.6f };
            ff.radius = 0.8f + (rand() % 100) / 100.f * 1.8f;   // 0.8 – 2.6
            ff.speed = (0.5f + (rand() % 100) / 100.f * 1.2f)  // 0.5 – 1.7 rad/s
                * (rand() % 2 == 0 ? 1.f : -1.f);        // CW or CCW
            ff.phase = (rand() % 1000) / 1000.f * 2.f * PI;
            ff.height = 0.6f + (rand() % 100) / 100.f * 2.0f;   // 0.6 – 2.6 m
            ff.color = palette[rand() % 4];
        }
    }
    // Also scatter a few inside the campsite grove
    for (int i = 0; i < 12; i++) {
        Firefly ff;
        ff.center = { -20.f + (rand() % 20 - 10) * 1.2f, 0.f, -37.f + (rand() % 20 - 10) * 1.2f };
        ff.radius = 0.6f + (rand() % 100) / 100.f * 1.4f;
        ff.speed = (0.4f + (rand() % 100) / 100.f * 1.0f) * (rand() % 2 == 0 ? 1.f : -1.f);
        ff.phase = (rand() % 1000) / 1000.f * 2.f * PI;
        ff.height = 0.5f + (rand() % 100) / 100.f * 1.8f;
        ff.color = glm::vec3(0.90f, 1.00f, 0.30f);
        g_fireflies.push_back(ff);
    }
}

// Returns world-space position of firefly i at time t
static glm::vec3 fireflyPos(const Firefly& ff, float t)
{
    float angle = ff.phase + ff.speed * t;
    return ff.center + glm::vec3(cosf(angle) * ff.radius, ff.height, sinf(angle) * ff.radius);
}

static void drawTree(glm::vec3 p, float sc, int seed = 0);


// ?? Mesh type ?????????????????????????????????????????????????????????????????
using Mesh = std::vector<float>;

static void pv(Mesh& m, float x, float y, float z,
    float nx, float ny, float nz,
    float s = 0.f, float t = 0.f)
{
    m.push_back(x); m.push_back(y); m.push_back(z);
    m.push_back(nx); m.push_back(ny); m.push_back(nz);
    m.push_back(s); m.push_back(t);
}

static void quad(Mesh& m, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n)
{
    pv(m, a.x, a.y, a.z, n.x, n.y, n.z); pv(m, b.x, b.y, b.z, n.x, n.y, n.z);
    pv(m, c.x, c.y, c.z, n.x, n.y, n.z); pv(m, a.x, a.y, a.z, n.x, n.y, n.z);
    pv(m, c.x, c.y, c.z, n.x, n.y, n.z); pv(m, d.x, d.y, d.z, n.x, n.y, n.z);
}


// ?? Primitive builders 
static Mesh mkBox()
{
    Mesh m;
    // ??????? face ? UV coordinate ??? ??? ???
    // ?????? face
    pv(m, -1, -1, 1, 0, 0, 1, 0, 0); pv(m, 1, -1, 1, 0, 0, 1, 1, 0);
    pv(m, 1, 1, 1, 0, 0, 1, 1, 1); pv(m, -1, -1, 1, 0, 0, 1, 0, 0);
    pv(m, 1, 1, 1, 0, 0, 1, 1, 1); pv(m, -1, 1, 1, 0, 0, 1, 0, 1);
    // ?????? face
    pv(m, 1, -1, -1, 0, 0, -1, 0, 0); pv(m, -1, -1, -1, 0, 0, -1, 1, 0);
    pv(m, -1, 1, -1, 0, 0, -1, 1, 1); pv(m, 1, -1, -1, 0, 0, -1, 0, 0);
    pv(m, -1, 1, -1, 0, 0, -1, 1, 1); pv(m, 1, 1, -1, 0, 0, -1, 0, 1);
    // ???? face ???? ???????...
    // (???, ???, ???, ??? — ????????? UV 0-1 ??????)
    pv(m, -1, -1, -1, -1, 0, 0, 0, 0); pv(m, -1, -1, 1, -1, 0, 0, 1, 0);
    pv(m, -1, 1, 1, -1, 0, 0, 1, 1); pv(m, -1, -1, -1, -1, 0, 0, 0, 0);
    pv(m, -1, 1, 1, -1, 0, 0, 1, 1); pv(m, -1, 1, -1, -1, 0, 0, 0, 1);
    pv(m, 1, -1, 1, 1, 0, 0, 0, 0); pv(m, 1, -1, -1, 1, 0, 0, 1, 0);
    pv(m, 1, 1, -1, 1, 0, 0, 1, 1); pv(m, 1, -1, 1, 1, 0, 0, 0, 0);
    pv(m, 1, 1, -1, 1, 0, 0, 1, 1); pv(m, 1, 1, 1, 1, 0, 0, 0, 1);
    pv(m, -1, 1, 1, 0, 1, 0, 0, 0); pv(m, 1, 1, 1, 0, 1, 0, 1, 0);
    pv(m, 1, 1, -1, 0, 1, 0, 1, 1); pv(m, -1, 1, 1, 0, 1, 0, 0, 0);
    pv(m, 1, 1, -1, 0, 1, 0, 1, 1); pv(m, -1, 1, -1, 0, 1, 0, 0, 1);
    pv(m, -1, -1, -1, 0, -1, 0, 0, 0); pv(m, 1, -1, -1, 0, -1, 0, 1, 0);
    pv(m, 1, -1, 1, 0, -1, 0, 1, 1); pv(m, -1, -1, -1, 0, -1, 0, 0, 0);
    pv(m, 1, -1, 1, 0, -1, 0, 1, 1); pv(m, -1, -1, 1, 0, -1, 0, 0, 1);
    return m;
}
static Mesh mkCyl(int segs = 60)
{
    Mesh m; float st = 2 * PI / segs;
    for (int i = 0; i < segs; i++) {
        float a0 = i * st, a1 = (i + 1) * st;
        float u0 = (float)i / segs, u1 = (float)(i + 1) / segs;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        pv(m, c0, -1, s0, c0, 0, s0, u0, 0.f); pv(m, c1, -1, s1, c1, 0, s1, u1, 0.f); pv(m, c1, 1, s1, c1, 0, s1, u1, 1.f);
        pv(m, c0, -1, s0, c0, 0, s0, u0, 0.f); pv(m, c1, 1, s1, c1, 0, s1, u1, 1.f); pv(m, c0, 1, s0, c0, 0, s0, u0, 1.f);
        pv(m, 0, 1, 0, 0, 1, 0, 0.5f, 0.5f); pv(m, c0, 1, s0, 0, 1, 0, u0, 1.f); pv(m, c1, 1, s1, 0, 1, 0, u1, 1.f);
        pv(m, 0, -1, 0, 0, -1, 0, 0.5f, 0.5f); pv(m, c1, -1, s1, 0, -1, 0, u1, 1.f); pv(m, c0, -1, s0, 0, -1, 0, u0, 1.f);
    }
    return m;
}
static Mesh mkSph(int segs = 16)
{
    Mesh m; float ps = PI / segs, ts = 2 * PI / segs;
    for (int i = 0; i < segs; i++) {
        float p0 = i * ps - PI / 2.f, p1 = (i + 1) * ps - PI / 2.f;
        for (int j = 0; j < segs; j++) {
            float t0 = j * ts, t1 = (j + 1) * ts;
            auto P = [](float p, float t)->glm::vec3 {
                return{ cosf(p) * cosf(t), sinf(p), cosf(p) * sinf(t) };
                };
            glm::vec3 v00 = P(p0, t0), v10 = P(p1, t0), v01 = P(p0, t1), v11 = P(p1, t1);
            // UV: u = j/segs, v = i/segs
            float u0 = (float)j / segs, u1 = (float)(j + 1) / segs;
            float vv0 = (float)i / segs, vv1 = (float)(i + 1) / segs;
            pv(m, v00.x, v00.y, v00.z, v00.x, v00.y, v00.z, u0, vv0);
            pv(m, v10.x, v10.y, v10.z, v10.x, v10.y, v10.z, u0, vv1);
            pv(m, v11.x, v11.y, v11.z, v11.x, v11.y, v11.z, u1, vv1);
            pv(m, v00.x, v00.y, v00.z, v00.x, v00.y, v00.z, u0, vv0);
            pv(m, v11.x, v11.y, v11.z, v11.x, v11.y, v11.z, u1, vv1);
            pv(m, v01.x, v01.y, v01.z, v01.x, v01.y, v01.z, u1, vv0);
        }
    }
    return m;
}
static Mesh mkCone(int segs = 18)
{
    Mesh m; float st = 2 * PI / segs;
    for (int i = 0; i < segs; i++) {
        float a0 = i * st, a1 = (i + 1) * st;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        glm::vec3 n0 = glm::normalize(glm::vec3(c0, 0.45f, s0));
        glm::vec3 n1 = glm::normalize(glm::vec3(c1, 0.45f, s1));
        glm::vec3 nt = glm::normalize(glm::vec3((c0 + c1) * .5f, 0.45f, (s0 + s1) * .5f));
        pv(m, c0, 0, s0, n0.x, n0.y, n0.z); pv(m, c1, 0, s1, n1.x, n1.y, n1.z); pv(m, 0, 1, 0, nt.x, nt.y, nt.z);
        pv(m, 0, 0, 0, 0, -1, 0); pv(m, c1, 0, s1, 0, -1, 0); pv(m, c0, 0, s0, 0, -1, 0);
    }
    return m;
}
static Mesh mkRoof()
{
    Mesh m;
    glm::vec3 lN = glm::normalize(glm::vec3(-1, 1, 0)), rN = glm::normalize(glm::vec3(1, 1, 0));
    quad(m, { -1,0,-1 }, { 0,1,-1 }, { 0,1,1 }, { -1,0,1 }, lN);
    quad(m, { 0,1,-1 }, { 1,0,-1 }, { 1,0,1 }, { 0,1,1 }, rN);
    pv(m, -1, 0, -1, 0, 0, -1); pv(m, 1, 0, -1, 0, 0, -1); pv(m, 0, 1, -1, 0, 0, -1);
    pv(m, 1, 0, 1, 0, 0, 1);   pv(m, -1, 0, 1, 0, 0, 1);  pv(m, 0, 1, 1, 0, 0, 1);
    return m;
}
static Mesh mkStars(int n = 500)
{
    Mesh m; srand(101);
    for (int i = 0; i < n; i++) {
        float th = ((float)rand() / RAND_MAX) * 2 * PI;
        float ph = ((float)rand() / RAND_MAX) * PI - PI * 0.5f;
        float r = 60.f;
        pv(m, r * cosf(ph) * cosf(th), r * sinf(ph) + 2.f, r * cosf(ph) * sinf(th), 0, 1, 0);
    }
    return m;
}

static Mesh mkSkyDome(int segs = 32, float tileX = 4.0f)
{
    Mesh m; float ps = PI / segs, ts = 2 * PI / segs;
    for (int i = 0; i < segs; i++) {
        float p0 = i * ps - PI / 2.f, p1 = (i + 1) * ps - PI / 2.f;
        for (int j = 0; j < segs; j++) {
            float t0 = j * ts, t1 = (j + 1) * ts;
            auto P = [](float p, float t)->glm::vec3 {
                return{ cosf(p) * cosf(t), sinf(p), cosf(p) * sinf(t) };
                };
            glm::vec3 v00 = P(p0, t0), v10 = P(p1, t0), v01 = P(p0, t1), v11 = P(p1, t1);

            // uv
            float u0 = ((float)j / segs) * tileX;
            float u1 = ((float)(j + 1) / segs) * tileX;
            float raw_v0 = (float)i / segs;
            float raw_v1 = (float)(i + 1) / segs;
           
            float vv0 = (raw_v0 - 0.35f) / 0.65f;
            float vv1 = (raw_v1 - 0.35f) / 0.65f;

            pv(m, v00.x, v00.y, v00.z, v00.x, v00.y, v00.z, u0, vv0);
            pv(m, v10.x, v10.y, v10.z, v10.x, v10.y, v10.z, u0, vv1);
            pv(m, v11.x, v11.y, v11.z, v11.x, v11.y, v11.z, u1, vv1);

            pv(m, v00.x, v00.y, v00.z, v00.x, v00.y, v00.z, u0, vv0);
            pv(m, v11.x, v11.y, v11.z, v11.x, v11.y, v11.z, u1, vv1);
            pv(m, v01.x, v01.y, v01.z, v01.x, v01.y, v01.z, u1, vv0);
        }
    }
    return m;
}

static float catmullRom(float p0, float p1, float p2, float p3, float t)
{
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.f * p1)
        + (-p0 + p2) * t
        + (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t2
        + (-p0 + 3.f * p1 - 3.f * p2 + p3) * t3);
}

static const std::vector<float> sphereProfile = {
    0.00f,  
    0.38f,  
    0.72f,  
    0.95f,  
    1.00f,  
    0.95f,  
    0.72f,  
    0.38f,  
    0.00f   
};

static Mesh mkSplineLatheMesh(
    const std::vector<float>& profile,
    float totalHeight,
    int   radialSegs = 32,
    int   heightSegs = 40)
{
    Mesh m;
    int nKnots = (int)profile.size();

    for (int hi = 0; hi < heightSegs; hi++) {
        float s0 = (float)hi / heightSegs;
        float s1 = (float)(hi + 1) / heightSegs;
        float y0 = s0 * totalHeight;
        float y1 = s1 * totalHeight;

        // --- eval radius at s0 ---
        float idx0 = s0 * (nKnots - 1);
        int   i1a = (int)idx0;
        if (i1a > nKnots - 2) i1a = nKnots - 2;
        if (i1a < 0)          i1a = 0;
        float ta = idx0 - i1a;
        int   i0a = (i1a - 1 < 0) ? 0 : i1a - 1;
        int   i2a = (i1a + 1 > nKnots - 1) ? nKnots - 1 : i1a + 1;
        int   i3a = (i1a + 2 > nKnots - 1) ? nKnots - 1 : i1a + 2;
        float r0 = catmullRom(profile[i0a], profile[i1a], profile[i2a], profile[i3a], ta);
        if (r0 < 0.f) r0 = 0.f;

        // --- eval radius at s1 ---
        float idx1 = s1 * (nKnots - 1);
        int   i1b = (int)idx1;
        if (i1b > nKnots - 2) i1b = nKnots - 2;
        if (i1b < 0)          i1b = 0;
        float tb = idx1 - i1b;
        int   i0b = (i1b - 1 < 0) ? 0 : i1b - 1;
        int   i2b = (i1b + 1 > nKnots - 1) ? nKnots - 1 : i1b + 1;
        int   i3b = (i1b + 2 > nKnots - 1) ? nKnots - 1 : i1b + 2;
        float r1 = catmullRom(profile[i0b], profile[i1b], profile[i2b], profile[i3b], tb);
        if (r1 < 0.f) r1 = 0.f;

        float dr = r1 - r0;
        float dy = y1 - y0;
        float nlen = sqrtf(dy * dy + dr * dr);
        float nY = (nlen > 1e-5f) ? (dr / nlen) : 0.f;
        float nR = (nlen > 1e-5f) ? (dy / nlen) : 1.f;

        for (int ri = 0; ri < radialSegs; ri++) {
            float a0 = (float)ri / radialSegs * 2.f * PI;
            float a1 = (float)(ri + 1) / radialSegs * 2.f * PI;
            float u0 = (float)ri / radialSegs;
            float u1 = (float)(ri + 1) / radialSegs;

            float c0 = cosf(a0), s_0 = sinf(a0);
            float c1 = cosf(a1), s_1 = sinf(a1);

            // quad as two triangles
            pv(m, c0 * r0, y0, s_0 * r0, c0 * nR, nY, s_0 * nR, u0, s0);
            pv(m, c1 * r0, y0, s_1 * r0, c1 * nR, nY, s_1 * nR, u1, s0);
            pv(m, c1 * r1, y1, s_1 * r1, c1 * nR, nY, s_1 * nR, u1, s1);

            pv(m, c0 * r0, y0, s_0 * r0, c0 * nR, nY, s_0 * nR, u0, s0);
            pv(m, c1 * r1, y1, s_1 * r1, c1 * nR, nY, s_1 * nR, u1, s1);
            pv(m, c0 * r1, y1, s_0 * r1, c0 * nR, nY, s_0 * nR, u0, s1);
        }
    }
    return m;
}



// ?? VAO builder 
static GLuint mkVAO(const Mesh& mesh, int& cnt)
{
    cnt = (int)mesh.size() / 8;   // ? 6 ???? 8 ???
    GLuint va, vb;
    glGenVertexArrays(1, &va); glGenBuffers(1, &vb);
    glBindVertexArray(va);
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(float), mesh.data(), GL_STATIC_DRAW);

    // attr 0: position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // attr 1: normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // attr 2: texCoord  ? ????
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return va;
}

// ?? Shader loader ?????????????????????????????????????????????????????????????
static std::string readFile(const char* path)
{
    std::ifstream f(path);
    if (!f) { std::cerr << "Cannot open: " << path << "\n"; return ""; }
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}
static GLuint mkProg(const char* vp, const char* fp)
{
    auto vs_str = readFile(vp); auto fs_str = readFile(fp);
    const char* vc = vs_str.c_str(); const char* fc = fs_str.c_str();
    int ok; char log[1024];
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vc, nullptr); glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(vs, 1024, nullptr, log); std::cerr << "VS:\n" << log << "\n"; }
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fc, nullptr); glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(fs, 1024, nullptr, log); std::cerr << "FS:\n" << log << "\n"; }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(prog, 1024, nullptr, log); std::cerr << "LINK:\n" << log << "\n"; }
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// ?? Uniform helpers 
static void u(const char* n, const glm::mat4& v) { glUniformMatrix4fv(glGetUniformLocation(g_prog, n), 1, GL_FALSE, glm::value_ptr(v)); }
static void u(const char* n, const glm::vec3& v) { glUniform3fv(glGetUniformLocation(g_prog, n), 1, glm::value_ptr(v)); }
static void u(const char* n, float v) { glUniform1f(glGetUniformLocation(g_prog, n), v); }
static void u(const char* n, int v) { glUniform1i(glGetUniformLocation(g_prog, n), v); }
static void uv3a(const char* nm, int idx, const glm::vec3& v)
{
    char buf[64]; snprintf(buf, 64, "%s[%d]", nm, idx);
    glUniform3fv(glGetUniformLocation(g_prog, buf), 1, glm::value_ptr(v));
}

// ?? Mesh VAOs 
static GLuint bxV, cyV, spV, cnV, rfV, stV, sdV;
static int    bxC, cyC, spC, cnC, rfC, stC, sdC;

// ?? Globals for 
static GLuint texWall, texWindows, texRoof, texWood, texStone,texStone1,texStone2, texStone3,texcar, texPlanks, texWindowArch;
static GLuint texPath;   // dedicated path/road texture
static GLuint texHouse;  // Disney house wall colour texture (cream/lavender)
static GLuint texCastle;
static GLuint texFrog;
static GLuint texBarnWall, texBarnInner, texBarnFloor, texBarnRoof, texBarnDoor;
static GLuint texHorizon;
static GLuint texFace, texTorso, texArms, texHands, texLegs, texBoots, texLowerPart;


static GLuint loadTexture(const char* path) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    int w, h, nc;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &nc, 0);
    if (data) {
        GLenum f = (nc == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, f, w, h, 0, f, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cerr << "Failed to load: " << path << "\n";
        glDeleteTextures(1, &texID);
        texID = 0;
    }
    stbi_image_free(data);
    return texID;
}

static void drwTex(GLuint va, int cnt, const glm::mat4& m, GLuint texID, int matType = 7, bool emissive = false, GLenum mode = GL_TRIANGLES)
{
    u("model", m);
    u("objectColor", glm::vec3(1.0f));
    u("matType", matType); u("isEmissive", (int)emissive);
    if (texID > 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texID);
        u("tex0", 0);
    }
    glBindVertexArray(va);
    glDrawArrays(mode, 0, cnt);
}

// ?? Core draw call 
static void drw(GLuint va, int cnt, const glm::mat4& m, glm::vec3 col,
    int matType = 0, bool emissive = false, GLenum mode = GL_TRIANGLES)
{
    u("model", m); u("objectColor", col);
    u("matType", matType); u("isEmissive", (int)emissive);
    glBindVertexArray(va);
    glDrawArrays(mode, 0, cnt);
}



static void drawFractalTree(glm::vec3 p, glm::vec3 dir, float len, float thick, int depth) {
    if (depth <= 0) return;
    glm::vec3 end = p + dir * len;
    glm::mat4 m = glm::translate(glm::mat4(1), (p + end) * 0.5f);
    glm::vec3 up = { 0, 1, 0 };
    glm::vec3 axis = glm::cross(up, dir);
    if (glm::length(axis) > 0.001f) {
        float angle = acosf(glm::dot(up, dir));
        m = glm::rotate(m, angle, axis);
    }
    m = glm::scale(m, { thick, len * 0.5f, thick });
    drwTex(cyV, cyC, m, texWood, 7);

    if (depth > 1) {
        float ang = 0.5f;
        glm::vec3 d1 = glm::normalize(dir + glm::vec3(sinf(ang), cosf(ang), 0) * 0.5f);
        glm::vec3 d2 = glm::normalize(dir + glm::vec3(-sinf(ang), cosf(ang), 0) * 0.5f);
        drawFractalTree(end, d1, len * 0.7f, thick * 0.6f, depth - 1);
        drawFractalTree(end, d2, len * 0.7f, thick * 0.6f, depth - 1);
    }
}

// =============================================================================
// ?? Object builders (unchanged) ???????????????????????????????????????????????
// =============================================================================

static void drawTree(glm::vec3 p, float sc, int seed)
{
    srand(seed);
    float trunkH = sc * (2.6f + (seed % 4) * 0.2f);
    float baseR = sc * (0.18f + (seed % 3) * 0.015f);
    GLuint barkTex = (texTreeBark > 0) ? texTreeBark : texWood;
    GLuint leafTexA = (seed % 2 == 0) ? texLeaf : texLeaf2;
    GLuint leafTexB = (seed % 2 == 0) ? texLeaf2 : texLeaf;
    if (leafTexA == 0) leafTexA = texLeaf2;
    if (leafTexB == 0) leafTexB = texLeaf;


    for (int seg = 0; seg < 3; seg++) {
        float y0 = seg * trunkH / 3.0f;
        float y1 = (seg + 1) * trunkH / 3.0f;
        float mid = (y0 + y1) * 0.5f;
        float taper = 1.0f - seg * 0.20f;
        glm::mat4 m = glm::translate(glm::mat4(1), p + glm::vec3(0, mid, 0));
        m = glm::scale(m, { baseR * taper, (trunkH / 3.0f) * 0.52f, baseR * taper });
        drwTex(cyV, cyC, m, barkTex, 7);
    }

    struct Br { glm::vec3 s, d; float len, t; int depth; };
    std::vector<Br> stk;
    glm::vec3 top = p + glm::vec3(0, trunkH, 0);
    for (int i = 0; i < 3; i++) {
        float yaw = (float)i * (2.0f * PI / 3.0f) + seed * 0.37f;
        glm::vec3 dir = glm::normalize(glm::vec3(cosf(yaw) * 0.45f, 0.80f, sinf(yaw) * 0.45f));
        stk.push_back({ top, dir, sc * 1.05f, baseR * 0.55f, 5 });
    }

    while (!stk.empty()) {
        Br b = stk.back();
        stk.pop_back();
        if (b.depth <= 0 || b.len < 0.06f) continue;

        glm::vec3 e = b.s + b.d * b.len;
        glm::vec3 mid = (b.s + e) * 0.5f;
        float segLen = glm::length(e - b.s);

        if (b.t > 0.014f) {
            glm::mat4 m = glm::translate(glm::mat4(1), mid);
            glm::vec3 up = { 0, 1, 0 };
            glm::vec3 ax = glm::cross(up, b.d);
            if (glm::length(ax) > 0.001f) {
                float ang = acosf(glm::clamp(glm::dot(up, glm::normalize(b.d)), -1.0f, 1.0f));
                m = glm::rotate(m, ang, glm::normalize(ax));
            }
            m = glm::scale(m, { b.t, segLen * 0.52f, b.t });
            drwTex(cyV, cyC, m, barkTex, 7);
        }

        // Textured leaf clusters at branch tips
        if (b.depth <= 2) {
            float lsz = sc * (0.22f + b.depth * 0.09f);
            glm::mat4 lm = glm::translate(glm::mat4(1), e);
            lm = glm::scale(lm, { lsz * 1.35f, lsz * 0.95f, lsz * 1.35f });
            drwTex(spV, spC, lm, leafTexA, 7);

            glm::mat4 lm2 = glm::translate(glm::mat4(1), e + glm::vec3(lsz * 0.35f, 0, -lsz * 0.25f));
            lm2 = glm::scale(lm2, { lsz * 0.90f, lsz * 0.70f, lsz * 0.90f });
            drwTex(spV, spC, lm2, leafTexB, 7);
        }

        float spread = 0.40f + (5 - b.depth) * 0.07f;
        glm::vec3 up = { 0, 1, 0 };
        glm::vec3 ax = glm::cross(up, b.d);
        glm::vec3 orth = glm::length(ax) > 0.001f ? glm::normalize(ax) : glm::vec3(1, 0, 0);

        for (int i = 0; i < 2; i++) {
            float yaw = (float)i * PI + (seed + b.depth) * 0.61f;
            glm::vec3 pitchDir = glm::normalize(b.d * cosf(spread) + orth * sinf(spread));
            glm::mat4 rot = glm::rotate(glm::mat4(1), yaw, b.d);
            glm::vec3 child = glm::normalize(glm::vec3(rot * glm::vec4(pitchDir, 0.0f)));
            child = glm::normalize(child * 0.72f + glm::vec3(0, 1, 0) * 0.28f);
            stk.push_back({ e, child, b.len * 0.66f, b.t * 0.58f, b.depth - 1 });
        }
    }
    // Large textured canopy blob at the very top of the tree
    {
        GLuint topLeafTex = (texLeaf > 0) ? texLeaf : texLeaf2;
        glm::mat4 topCanopy = glm::translate(glm::mat4(1), p + glm::vec3(0, trunkH + sc * 0.6f, 0));
        topCanopy = glm::scale(topCanopy, { sc * 1.4f, sc * 1.0f, sc * 1.4f });
        drwTex(spV, spC, topCanopy, topLeafTex, 7);

        // Second offset sphere for natural asymmetry
        glm::mat4 topCanopy2 = glm::translate(glm::mat4(1), p + glm::vec3(sc * 0.5f, trunkH + sc * 0.3f, sc * 0.4f));
        topCanopy2 = glm::scale(topCanopy2, { sc * 1.0f, sc * 0.8f, sc * 1.0f });
        drwTex(spV, spC, topCanopy2, topLeafTex, 7);
    }

    glm::mat4 ao = glm::translate(glm::mat4(1), p + glm::vec3(0, 0.01f, 0));
    ao = glm::scale(ao, { sc * 1.6f, 0.01f, sc * 1.6f });
    drw(cyV, cyC, ao, { 0.05f, 0.05f, 0.05f });

}

// =============================================================================
// ?? UFO — floating saucer with spinning dome, tractor-beam & sparks
// =============================================================================
static void drawUFO(glm::vec3 pos, float spinAngle, float now)
{
    // ── Colours ──────────────────────────────────────────────────────────────
    glm::vec3 colSaucer{ 0.72f, 0.76f, 0.82f };  // brushed metal silver
    glm::vec3 colDark{ 0.30f, 0.32f, 0.36f };  // underside dark metal
    glm::vec3 colDome{ 0.55f, 0.88f, 1.00f };  // translucent blue dome
    glm::vec3 colLight{ 1.00f, 0.95f, 0.30f };  // yellow rotating lights

    glm::mat4 base = glm::translate(glm::mat4(1), pos);

    // ── 1. Main saucer disc (flat oblate sphere → use scaled sphere) ─────────
    {
        glm::mat4 m = glm::translate(base, { 0.f, 0.f, 0.f });
        m = glm::scale(m, { 1.60f, 0.28f, 1.60f });
        drw(spV, spC, m, colSaucer, 0, false);
    }
    // Upper rim bevel
    {
        glm::mat4 m = glm::translate(base, { 0.f, 0.18f, 0.f });
        m = glm::scale(m, { 1.30f, 0.18f, 1.30f });
        drw(spV, spC, m, colSaucer, 0, false);
    }
    // Underside dark hub
    {
        glm::mat4 m = glm::translate(base, { 0.f, -0.16f, 0.f });
        m = glm::scale(m, { 0.55f, 0.20f, 0.55f });
        drw(spV, spC, m, colDark, 0, false);
    }
    // Bottom emitter nozzle
    {
        glm::mat4 m = glm::translate(base, { 0.f, -0.38f, 0.f });
        m = glm::scale(m, { 0.28f, 0.18f, 0.28f });
        drw(cyV, cyC, m, colDark, 0, false);
    }

    // ── 2. Dome (top hemisphere) ─────────────────────────────────────────────
    {
        glm::mat4 m = glm::translate(base, { 0.f, 0.30f, 0.f });
        m = glm::scale(m, { 0.70f, 0.60f, 0.70f });
        drw(spV, spC, m, colDome, 0, true);   // emissive glow
    }

    // ── 3. Spinning coloured lights around rim ───────────────────────────────
    const int NUM_LIGHTS = 8;
    for (int i = 0; i < NUM_LIGHTS; i++) {
        float a = spinAngle + i * (2.f * PI / NUM_LIGHTS);
        float lx = cosf(a) * 1.42f;
        float lz = sinf(a) * 1.42f;
        // Alternate warm/cool colours
        glm::vec3 lc = (i % 2 == 0)
            ? glm::vec3(1.00f, 0.40f + 0.3f * sinf(now * 3.f + i), 0.10f)  // orange-red
            : glm::vec3(0.20f, 0.80f, 1.00f);                                 // cyan
        glm::mat4 m = glm::translate(base, { lx, 0.05f, lz });
        m = glm::scale(m, { 0.10f, 0.10f, 0.10f });
        drw(spV, spC, m, lc, 0, true);
        // Tiny halo
        glm::mat4 h = glm::translate(base, { lx, 0.05f, lz });
        h = glm::scale(h, { 0.18f, 0.18f, 0.18f });
        drw(spV, spC, h, lc * 0.45f, 0, true);
    }
}

// ---------------------------------------------------------------------------
// Cubic Bézier point evaluator (3D)
// ---------------------------------------------------------------------------
static glm::vec3 bez3(glm::vec3 p0, glm::vec3 p1,
    glm::vec3 p2, glm::vec3 p3, float t)
{
    float u = 1.f - t;
    return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
}

// Draw a thick Bézier tube (approximated as cylinder chain)
static void drawBezTube(glm::vec3 p0, glm::vec3 p1,
    glm::vec3 p2, glm::vec3 p3,
    float radius, glm::vec3 col,
    int steps = 8)
{
    for (int i = 0; i < steps; i++) {
        float t0 = (float)i / steps;
        float t1 = (float)(i + 1) / steps;
        glm::vec3 a = bez3(p0, p1, p2, p3, t0);
        glm::vec3 c = bez3(p0, p1, p2, p3, t1);
        glm::vec3 mid = (a + c) * 0.5f;
        float len = glm::length(c - a);
        if (len < 1e-4f) continue;

        glm::vec3 dir = glm::normalize(c - a);
        glm::vec3 up = { 0, 1, 0 };
        glm::mat4 m = glm::translate(glm::mat4(1), mid);
        glm::vec3 ax = glm::cross(up, dir);
        if (glm::length(ax) > 0.001f) {
            float ang = acosf(glm::clamp(glm::dot(up, dir), -1.f, 1.f));
            m = glm::rotate(m, ang, glm::normalize(ax));
        }
        m = glm::scale(m, { radius, len * 0.52f, radius });
        drw(cyV, cyC, m, col, 0);
    }
}

static void drawRooster(glm::vec3 origin, float now, GLuint texRooster = 0)
{
    glm::vec3 colBody = { 0.50f, 0.00f, 1.00f };
    glm::vec3 colWing = { 0.00f, 0.50f, 1.00f };
    glm::vec3 colTail = { 0.50f, 0.00f, 0.00f };
    glm::vec3 colComb = { 0.90f, 0.15f, 0.12f };
    glm::vec3 colBeak = { 0.95f, 0.75f, 0.15f };
    glm::vec3 colLeg = { 0.95f, 0.72f, 0.20f };
    glm::vec3 colEye = { 0.05f, 0.05f, 0.05f };

    float wingFlap = sinf(now * 4.f) * 0.18f;

    // Decide: textured or flat colour?
    bool hasTex = (texRooster > 0);
    // ── Body ──────────────────────────────────────────────────────────────
    glm::mat4 body = glm::translate(glm::mat4(1), origin + glm::vec3(0, 0.18f, 0));
    body = glm::scale(body, { 0.14f, 0.16f, 0.22f });
    u("model", body);
    u("objectColor", colBody);
    u("matType", 8); u("isEmissive", 0);
    if (hasTex) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texRooster); u("tex0", 0); }
    glBindVertexArray(spV); glDrawArrays(GL_TRIANGLES, 0, spC);

    // ── Head ──────────────────────────────────────────────────────────────
    glm::vec3 headPos = origin + glm::vec3(0, 0.42f, 0.14f);
    glm::mat4 head = glm::translate(glm::mat4(1), headPos);
    head = glm::scale(head, { 0.09f, 0.10f, 0.09f });
    u("model", head);
    u("objectColor", colBody);
    u("matType", 8); u("isEmissive", 0);
    if (hasTex) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texRooster); u("tex0", 0); }
    glBindVertexArray(spV); glDrawArrays(GL_TRIANGLES, 0, spC);

    // ── Neck ──────────────────────────────────────────────────────────────
    drawBezTube(
        origin + glm::vec3(0, 0.28f, 0.10f),
        origin + glm::vec3(0, 0.38f, 0.18f),
        origin + glm::vec3(0, 0.40f, 0.16f),
        headPos,
        0.055f, colBody, 6);

    // ── Comb (3 red bumps) ────────────────────────────────────────────────
    for (int ci = 0; ci < 3; ci++) {
        float cx = (ci - 1) * 0.04f;
        float cy = 0.06f - fabsf(ci - 1) * 0.015f;
        glm::mat4 cm = glm::translate(glm::mat4(1), headPos + glm::vec3(cx, cy, 0));
        cm = glm::scale(cm, { 0.025f, 0.038f, 0.025f });
        drw(spV, spC, cm, colComb, 0);
    }

    // ── Wattle ────────────────────────────────────────────────────────────
    glm::mat4 wattle = glm::translate(glm::mat4(1), headPos + glm::vec3(0, -0.055f, 0.07f));
    wattle = glm::scale(wattle, { 0.022f, 0.036f, 0.022f });
    drw(spV, spC, wattle, colComb, 0);

    // ── Beak ──────────────────────────────────────────────────────────────
    {
        glm::mat4 bm = glm::translate(glm::mat4(1), headPos + glm::vec3(0, -0.01f, 0.09f));
        bm = glm::rotate(bm, glm::radians(90.f), { 1, 0, 0 });
        bm = glm::scale(bm, { 0.025f, 0.045f, 0.025f });
        drw(cnV, cnC, bm, colBeak, 0);
    }

    // ── Eye ───────────────────────────────────────────────────────────────
    glm::mat4 eye = glm::translate(glm::mat4(1), headPos + glm::vec3(0.055f, 0.01f, 0.06f));
    eye = glm::scale(eye, { 0.018f, 0.018f, 0.018f });
    drw(spV, spC, eye, colEye, 0);

    // ── Wings ─────────────────────────────────────────────────────────────
    for (float side : {-1.f, 1.f}) {
        float flapOff = side * wingFlap;
        glm::vec3 wingRoot = origin + glm::vec3(side * 0.08f, 0.22f, 0);
        glm::vec3 wingTip = origin + glm::vec3(side * 0.28f, 0.14f + flapOff, -0.05f);
        drawBezTube(
            wingRoot,
            origin + glm::vec3(side * 0.18f, 0.25f + flapOff * 0.5f, 0.02f),
            origin + glm::vec3(side * 0.24f, 0.20f + flapOff, -0.02f),
            wingTip,
            0.045f, colWing, 5);
        // Wing tip nub — textured + tinted
        glm::mat4 wt = glm::translate(glm::mat4(1), wingTip);
        wt = glm::scale(wt, { 0.06f, 0.04f, 0.08f });
        u("model", wt);
        u("objectColor", colWing);
        u("matType", 8); u("isEmissive", 0);
        if (hasTex) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texRooster); u("tex0", 0); }
        glBindVertexArray(spV); glDrawArrays(GL_TRIANGLES, 0, spC);
    }

    // ── Tail feathers ─────────────────────────────────────────────────────
    struct TailFeather { float spreadX; float peakY; float peakZ; float tipY; float tipZ; };
    static const TailFeather feathers[] = {
        { -0.14f, 0.55f, -0.28f, 0.72f, -0.42f },  // far left
        { -0.07f, 0.60f, -0.26f, 0.82f, -0.38f },  // mid left
        {  0.00f, 0.65f, -0.24f, 0.90f, -0.36f },  // centre (tallest)
        {  0.07f, 0.60f, -0.26f, 0.82f, -0.38f },  // mid right
        {  0.14f, 0.55f, -0.28f, 0.72f, -0.42f },  // far right
    };

    // Alternate tail colours for iridescent look
    glm::vec3 colTail1 = { 0.15f, 0.45f, 0.85f };  // deep blue
    glm::vec3 colTail2 = { 0.10f, 0.65f, 0.35f };  // teal green
    glm::vec3 colTail3 = { 0.55f, 0.15f, 0.75f };  // purple

    for (int ti = 0; ti < 5; ti++) {
        const TailFeather& tf = feathers[ti];

        // Pick iridescent colour cycling across the fan
        glm::vec3 fc = (ti == 0 || ti == 4) ? colTail1
            : (ti == 1 || ti == 3) ? colTail2
            : colTail3;

        // Root: base of tail at lower back of body
        glm::vec3 root = origin + glm::vec3(tf.spreadX * 0.4f, 0.18f, -0.20f);

        // Control point 1: fan outward and upward
        glm::vec3 cp1 = origin + glm::vec3(tf.spreadX * 0.9f, 0.35f, -0.24f);

        // Control point 2: peak of arc
        glm::vec3 cp2 = origin + glm::vec3(tf.spreadX * 1.2f, tf.peakY, tf.peakZ);

        // Tip: curls slightly back down for U-shape
        glm::vec3 tip = origin + glm::vec3(tf.spreadX * 1.0f, tf.tipY, tf.tipZ);

        // Shaft thickness — thicker at centre, thinner at edges
        float thickness = (ti == 2) ? 0.038f : (ti == 1 || ti == 3) ? 0.032f : 0.025f;

        drawBezTube(root, cp1, cp2, tip, thickness, fc, 8);

        // Feather tip wisp — small sphere at tip for fluffy end
        glm::mat4 wisp = glm::translate(glm::mat4(1), tip);
        wisp = glm::scale(wisp, { 0.04f, 0.06f, 0.04f });
        drw(spV, spC, wisp, fc * 1.1f, 0);
        u("model", wisp);
        u("objectColor", colTail);
        u("matType", 8); u("isEmissive", 0);
        if (hasTex) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texRooster); u("tex0", 0); }
        glBindVertexArray(spV); glDrawArrays(GL_TRIANGLES, 0, spC);
    }

    // Base tuft — small cluster at root of tail where feathers meet body
    glm::mat4 tuft = glm::translate(glm::mat4(1), origin + glm::vec3(0, 0.20f, -0.22f));
    tuft = glm::scale(tuft, { 0.09f, 0.07f, 0.06f });
    drw(spV, spC, tuft, colTail2, 0);

    // ── Legs ──────────────────────────────────────────────────────────────
    for (float lx : {-0.05f, 0.05f}) {
        glm::mat4 leg = glm::translate(glm::mat4(1), origin + glm::vec3(lx, 0.04f, 0.04f));
        leg = glm::scale(leg, { 0.022f, 0.10f, 0.022f });
        drw(cyV, cyC, leg, colLeg, 0);
        glm::mat4 foot = glm::translate(glm::mat4(1), origin + glm::vec3(lx, -0.02f, 0.06f));
        foot = glm::scale(foot, { 0.030f, 0.018f, 0.045f });
        drw(spV, spC, foot, colLeg, 0);
    }

    u("objectColor", glm::vec3(1.f)); // reset after rooster
}
// =============================================================================
// Disney Two-Story House  –  whimsical, colourful, with two accessible rooms
// =============================================================================
static void drawDisneyHouse(glm::vec3 p, float facing, float sx = 1.f,
    bool flipDoor = false,
    glm::vec3 wallTint = { 1.f, 1.f, 1.f })
{
    glm::mat4 b = glm::translate(glm::mat4(1), p);
    b = glm::rotate(b, facing, { 0, 1, 0 });
    glm::mat4 m;

    // Disney pastel tint colours (multiplied with texture)
    glm::vec3 creamWall = { 1.00f, 0.97f, 0.88f };
    glm::vec3 lavenderWall = { 0.88f, 0.84f, 1.00f };
    glm::vec3 coralTrim = { 0.96f, 0.55f, 0.36f };
    glm::vec3 skyBlueWin = {0.82f, 0.26f, 0.38f };
    glm::vec3 roseRoof = { 0.82f, 0.26f, 0.38f };
    glm::vec3 tealDoor = { 0.82f, 0.26f, 0.38f };
    glm::vec3 goldTrim = { 0.95f, 0.80f, 0.20f };
    glm::vec3 stoneBase = { 0.68f, 0.62f, 0.55f };

    // Resolve runtime textures (fall back gracefully)
    GLuint tWall = (texHouse > 0) ? texHouse : (texWall > 0 ? texWall : 0);
    GLuint tRoof = (texRoof > 0) ? texRoof : 0;
    GLuint tWood = (texWood > 0) ? texWood : 0;
    GLuint tScar = (texcar > 0) ? texcar : 0;
    GLuint tStone = (texStone > 0) ? texStone : 0;
    GLuint tStone2 = (texStone2 > 0) ? texStone2 : 0;
    GLuint tStone3 = (texStone3 > 0) ? texStone3 : 0;
    GLuint tStone1 = (texStone1 > 0) ? texStone1 : 0;
    GLuint tWin = (texWindows > 0) ? texWindows : 0;
    GLuint tPlank = (texPlanks > 0) ? texPlanks : tWood;

    // ------------------------------------------------------------------ //
    // 0.  STONE FOUNDATION
    // ------------------------------------------------------------------ //
    m = glm::translate(b, { 0.0f, 0.28f, 0.0f });
    m = glm::scale(m, { sx * 2.65f, 0.28f, 2.35f });
    u("objectColor", stoneBase);
    drwTex(bxV, bxC, m, tStone, 8);
    u("objectColor", glm::vec3(1.f));
    // Coral belt
    m = glm::translate(b, { 0.0f, 0.52f, 0.0f });
    m = glm::scale(m, { sx * 2.68f, 0.06f, 2.38f });
    drw(bxV, bxC, m, coralTrim, 0);

    // ------------------------------------------------------------------ //
    // 1.  GROUND FLOOR WALLS  (textured cream plaster)
    // ------------------------------------------------------------------ //
    float gfBot = 0.55f, gfTop = 2.40f, gfMid = (gfBot + gfTop) * 0.5f, gfH = (gfTop - gfBot) * 0.5f;

    m = glm::translate(b, { 0.0f, gfMid, 0.0f });
    m = glm::scale(m, { sx * 2.55f, gfH, 2.25f });
    u("objectColor", creamWall);
    drwTex(bxV, bxC, m, tWall, 8);
    u("objectColor", glm::vec3(1.f));

    // Wainscot lower plank band
    m = glm::translate(b, { 0.0f, 0.85f, 0.0f });
    m = glm::scale(m, { sx * 2.58f, 0.18f, 2.28f });
    u("objectColor", coralTrim * 0.85f);
    drwTex(bxV, bxC, m, tPlank, 8);
    u("objectColor", glm::vec3(1.f));

    // Vertical timber beams
    for (float x : { -2.55f, -1.28f, 0.0f, 1.28f, 2.55f }) {
        m = glm::translate(b, { x * sx, gfMid, 2.27f });
        m = glm::scale(m, { 0.07f, gfH, 0.06f });
        u("objectColor", coralTrim);
        drwTex(bxV, bxC, m, tWood, 8);
        m = glm::translate(b, { x * sx, gfMid, -2.27f });
        m = glm::scale(m, { 0.07f, gfH, 0.06f });
        drwTex(bxV, bxC, m, tWood, 8);
        u("objectColor", glm::vec3(1.f));
    }

    // Floor-divider gold belt
    m = glm::translate(b, { 0.0f, 2.42f, 0.0f });
    m = glm::scale(m, { sx * 2.80f, 0.14f, 2.50f });
    drw(bxV, bxC, m, goldTrim, 0);

    // ------------------------------------------------------------------ //
    // 2.  SECOND FLOOR WALLS  (textured lavender plaster, overhang)
    // ------------------------------------------------------------------ //
    float sfBot = 2.56f, sfTop = 4.62f, sfMid = (sfBot + sfTop) * 0.5f, sfH = (sfTop - sfBot) * 0.5f;

    m = glm::translate(b, { 0.0f, sfMid, 0.0f });
    m = glm::scale(m, { sx * 2.75f, sfH, 2.45f });
    u("objectColor", lavenderWall);
    drwTex(bxV, bxC, m, tWall, 8);
    u("objectColor", glm::vec3(1.f));

    // Diagonal decorative timber panels
    for (float rsx : { -1.f, 1.f }) {
        float cx = 1.90f * sx * rsx;
        for (float rz : { 2.47f, -2.47f }) {
            m = glm::translate(b, { cx, sfMid, rz });
            m = glm::rotate(m, glm::radians(rsx * 30.f), { 0, 0, 1 });
            m = glm::scale(m, { 0.06f, sfH * 1.2f, 0.05f });
            u("objectColor", coralTrim);
            drwTex(bxV, bxC, m, tWood, 8);
            m = glm::translate(b, { cx, sfMid, rz });
            m = glm::rotate(m, glm::radians(-rsx * 30.f), { 0, 0, 1 });
            m = glm::scale(m, { 0.06f, sfH * 1.2f, 0.05f });
            drwTex(bxV, bxC, m, tWood, 8);
            u("objectColor", glm::vec3(1.f));
        }
    }

    // Upper gold trim band
    m = glm::translate(b, { 0.0f, 4.64f, 0.0f });
    m = glm::scale(m, { sx * 2.80f, 0.12f, 2.50f });
    drw(bxV, bxC, m, goldTrim, 0);

    // ------------------------------------------------------------------ //
    // 3.  INTERIOR — GROUND FLOOR (living room)
    // ------------------------------------------------------------------ //
    // ------------------------------------------------------------------ //
    // 3.  INTERIOR — GROUND FLOOR  (large open hall)
    // ------------------------------------------------------------------ //
    // Floor — wide hardwood planks
    // Floor — wide hardwood planks (floor.jpg texture)
    GLuint tFloor = (texFloor > 0) ? texFloor : tWood;
    m = glm::translate(b, { 0.0f, 0.57f, 0.0f });
    m = glm::scale(m, { sx * 9.0f, 0.06f, 8.5f });
    drwTex(bxV, bxC, m, tFloor, 8);

    // Ceiling / upper floor slab
    // Ceiling / upper floor slab (floor.jpg texture on top face)
    m = glm::translate(b, { 0.0f, 2.42f, 0.0f });
    m = glm::scale(m, { sx * 9.2f, 0.10f, 8.7f });
    drwTex(bxV, bxC, m, tFloor, 8);

    // ---- INTERIOR WALLS ----
    // Back wall (north)
    m = glm::translate(b, { 0.0f, 1.50f, -8.5f });
    m = glm::scale(m, { sx * 9.0f, 0.90f, 0.08f });
    u("objectColor", creamWall);
    drwTex(bxV, bxC, m, tWall, 8);
    u("objectColor", glm::vec3(1.f));

    // Left wall (west)
    m = glm::translate(b, { -sx * 8.8f, 1.50f, 0.0f });
    m = glm::scale(m, { 0.08f, 0.90f, 8.5f });
    u("objectColor", creamWall);
    drwTex(bxV, bxC, m, tWall, 8);
    u("objectColor", glm::vec3(1.f));

    // Right wall (east)
    m = glm::translate(b, { sx * 8.8f, 1.50f, 0.0f });
    m = glm::scale(m, { 0.08f, 0.90f, 8.5f });
    u("objectColor", lavenderWall);
    drwTex(bxV, bxC, m, tWall, 8);
    u("objectColor", glm::vec3(1.f));

    // ---- WAINSCOT PANELS around the room ----
    for (float wz = -7.0f; wz <= 7.0f; wz += 2.5f) {
        m = glm::translate(b, { -sx * 8.75f, 0.85f, wz });
        m = glm::scale(m, { 0.06f, 0.18f, 1.0f });
        u("objectColor", coralTrim * 0.85f);
        drwTex(bxV, bxC, m, tPlank, 8);
        m = glm::translate(b, { sx * 8.75f, 0.85f, wz });
        m = glm::scale(m, { 0.06f, 0.18f, 1.0f });
        drwTex(bxV, bxC, m, tPlank, 8);
        u("objectColor", glm::vec3(1.f));
    }

    // ---- FIREPLACE — back wall centre ----
    // Stone surround
    m = glm::translate(b, { 0.0f, 1.00f, -8.42f });
    m = glm::scale(m, { 1.20f, 1.00f, 0.14f });
    u("objectColor", glm::vec3(0.55f, 0.50f, 0.44f));
    drwTex(bxV, bxC, m, tStone, 8);
    u("objectColor", glm::vec3(1.f));
    // Firebox opening
    m = glm::translate(b, { 0.0f, 0.78f, -8.36f });
    m = glm::scale(m, { 0.70f, 0.55f, 0.08f });
    drw(bxV, bxC, m, { 0.10f, 0.09f, 0.08f }, 0);
    // Mantelpiece slab
    m = glm::translate(b, { 0.0f, 1.65f, -8.38f });
    m = glm::scale(m, { 1.30f, 0.10f, 0.25f });
    u("objectColor", stoneBase);
    drwTex(bxV, bxC, m, tStone, 8);
    u("objectColor", glm::vec3(1.f));
    // Fire glow (emissive orange blob)
    glm::vec3 fireGlow = g_day ? glm::vec3(0.9f, 0.4f, 0.1f) : glm::vec3(1.0f, 0.5f, 0.1f);
    m = glm::translate(b, { 0.0f, 0.72f, -8.34f });
    m = glm::scale(m, { 0.28f, 0.18f, 0.06f });
    drw(spV, spC, m, fireGlow, 0, !g_day);

   

    // ---- SOFA LOUNGE (west side) ----
    // Main body
    m = glm::translate(b, { -sx * 4.5f, 0.75f, -2.0f });
    m = glm::scale(m, { 2.0f, 0.30f, 0.90f });
    drw(bxV, bxC, m, { 0.55f, 0.38f, 0.75f }, 0);
    // Back cushion
    m = glm::translate(b, { -sx * 4.5f, 1.12f, -2.75f });
    m = glm::scale(m, { 2.0f, 0.30f, 0.15f });
    drw(bxV, bxC, m, { 0.55f, 0.38f, 0.75f }, 0);
    // Arms
    for (float ax : { -1.f, 1.f }) {
        m = glm::translate(b, { -sx * (4.5f - ax * 1.80f), 0.88f, -2.25f });
        m = glm::scale(m, { 0.22f, 0.22f, 0.80f });
        drw(bxV, bxC, m, { 0.45f, 0.30f, 0.65f }, 0);
    }
    

    // ---- GRAND PIANO (northeast corner) ----
    m = glm::translate(b, { sx * 6.5f, 0.75f, -6.5f });
    m = glm::scale(m, { 1.20f, 0.15f, 0.80f });
    drw(bxV, bxC, m, { 0.08f, 0.07f, 0.07f }, 0);
    // Lid
    m = glm::translate(b, { sx * 6.5f, 0.96f, -6.5f });
    m = glm::scale(m, { 1.22f, 0.03f, 0.82f });
    drw(bxV, bxC, m, { 0.12f, 0.11f, 0.12f }, 0);
    // Keys
    m = glm::translate(b, { sx * 6.5f, 0.93f, -6.05f });
    m = glm::scale(m, { 1.00f, 0.02f, 0.12f });
    drw(bxV, bxC, m, { 0.98f, 0.97f, 0.93f }, 0);
    // Legs
    for (float lx2 : { -0.90f, 0.90f }) {
        m = glm::translate(b, { sx * (6.5f + lx2 * 0.5f), 0.40f, -6.2f });
        m = glm::scale(m, { 0.06f, 0.37f, 0.06f });
        drwTex(cyV, cyC, m, tWood, 8);
    }
    m = glm::translate(b, { sx * 6.5f, 0.40f, -6.90f });
    m = glm::scale(m, { 0.06f, 0.37f, 0.06f });
    drwTex(cyV, cyC, m, tWood, 8);

    // ---- BOOKSHELF — left wall ----
    m = glm::translate(b, { -sx * 8.55f, 1.10f, -5.5f });
    m = glm::scale(m, { 0.22f, 0.90f, 2.20f });
    drwTex(bxV, bxC, m, tWood, 8);
    // Books on shelves
    static const glm::vec3 bookCols[] = {
        {0.70f,0.20f,0.25f},{0.20f,0.40f,0.70f},{0.20f,0.65f,0.30f},
        {0.80f,0.60f,0.10f},{0.55f,0.20f,0.60f},{0.85f,0.30f,0.20f},
    };
    for (int bk = 0; bk < 6; bk++) {
        m = glm::translate(b, { -sx * 8.33f, 1.15f + (bk / 3) * 0.50f, -6.5f + (bk % 3) * 0.70f });
        m = glm::scale(m, { 0.04f, 0.36f, 0.06f });
        drw(bxV, bxC, m, bookCols[bk], 0);
    }

    // ---- CHANDELIER — hanging from ceiling centre ----
    // Stem
    m = glm::translate(b, { 0.0f, 2.24f, -2.0f });
    m = glm::scale(m, { 0.04f, 0.22f, 0.04f });
    drwTex(cyV, cyC, m, tWood, 8);
    // Arms
    for (int arm2 = 0; arm2 < 6; arm2++) {
        float ca = arm2 * (2.f * PI / 6.f);
        float cx2 = cosf(ca) * 0.80f, cz2 = sinf(ca) * 0.80f;
        m = glm::translate(b, { cx2, 2.05f, -2.0f + cz2 });
        m = glm::scale(m, { 0.03f, 0.03f, 0.80f });
        drw(cyV, cyC, m, goldTrim, 0);
        // Candle bulb
        glm::vec3 lBulb = g_day ? glm::vec3(1.0f, 0.92f, 0.7f) : glm::vec3(1.0f, 0.88f, 0.4f);
        m = glm::translate(b, { cx2, 1.98f, -2.0f + cz2 });
        m = glm::scale(m, { 0.07f, 0.09f, 0.07f });
        drw(spV, spC, m, lBulb, 0, !g_day);
    }
    // Ring
    m = glm::translate(b, { 0.0f, 2.02f, -2.0f });
    m = glm::scale(m, { 0.90f, 0.04f, 0.90f });
    drw(cyV, cyC, m, goldTrim, 0);

    // ---- STAIRCASE (left side going up) ----
    for (int st = 0; st < 8; st++) {
        float sy2 = 0.58f + st * 0.22f;
        float sz2 = 6.5f - st * 0.72f;
        m = glm::translate(b, { -sx * 6.5f, sy2, sz2 });
        m = glm::scale(m, { 1.40f, 0.06f, 0.40f });
        u("objectColor", stoneBase);
        drwTex(bxV, bxC, m, tStone, 8);
        u("objectColor", glm::vec3(1.f));
        // Riser
        m = glm::translate(b, { -sx * 6.5f, sy2 - 0.11f, sz2 - 0.35f });
        m = glm::scale(m, { 1.40f, 0.11f, 0.05f });
        drwTex(bxV, bxC, m, tWood, 8);
    }
    // Banister rail
    m = glm::translate(b, { -sx * 7.6f, 1.38f, 4.0f });
    m = glm::rotate(m, glm::radians(-20.f), { 0, 0, 1 });
    m = glm::scale(m, { 0.04f, 1.10f, 0.04f });
    drwTex(cyV, cyC, m, tWood, 8);

    // ---- DECOR: pot plant corner ----
    m = glm::translate(b, { sx * 7.8f, 0.68f, 7.0f });
    m = glm::scale(m, { 0.20f, 0.22f, 0.20f });
    u("objectColor", glm::vec3(0.42f, 0.25f, 0.12f));
    drwTex(cyV, cyC, m, tStone, 8);
    u("objectColor", glm::vec3(1.f));
    m = glm::translate(b, { sx * 7.8f, 1.00f, 7.0f });
    m = glm::scale(m, { 0.35f, 0.30f, 0.35f });
    drw(spV, spC, m, { 0.18f, 0.55f, 0.22f }, 0);

    // ---- INTERIOR UPPER FLOOR (bedroom — unchanged, pushed wider) ----
    // ------------------------------------------------------------------ //
    // 4.  INTERIOR — UPPER FLOOR (bedroom, wider)
    // ------------------------------------------------------------------ //
    m = glm::translate(b, { 0.0f, 3.55f, -8.25f });
    m = glm::scale(m, { sx * 9.0f, 0.90f, 0.08f });
    u("objectColor", lavenderWall);
    drwTex(bxV, bxC, m, tWall, 8);
    u("objectColor", glm::vec3(1.f));
    for (float xs : { -1.f, 1.f }) {
        m = glm::translate(b, { xs * sx * 8.8f, 3.55f, 0.0f });
        m = glm::scale(m, { 0.08f, 0.90f, 8.5f });
        u("objectColor", lavenderWall);
        drwTex(bxV, bxC, m, tWall, 8);
        u("objectColor", glm::vec3(1.f));
    }
    // Upper floor base
    // Upper floor base (floor.jpg texture)
    m = glm::translate(b, { 0.0f, 2.56f, 0.0f });
    m = glm::scale(m, { sx * 9.0f, 0.07f, 8.5f });
    drwTex(bxV, bxC, m, tFloor, 8);

  

    // Bookshelf upper room
    m = glm::translate(b, { -sx * 3.0f, 3.00f, -7.80f });
    m = glm::scale(m, { 1.20f, 0.65f, 0.20f });
    drwTex(bxV, bxC, m, tWood, 8);
    for (int bk2 = 0; bk2 < 5; bk2++) {
        glm::vec3 bkc = bookCols[bk2 % 6];
        m = glm::translate(b, { -sx * (3.8f - bk2 * 0.44f), 3.10f, -7.72f });
        m = glm::scale(m, { 0.07f, 0.42f, 0.06f });
        drw(bxV, bxC, m, bkc, 0);
    }

    // Stairwell opening slot
    m = glm::translate(b, { -sx * 6.5f, 2.56f, 2.5f });
    m = glm::scale(m, { 1.50f, 0.09f, 1.50f });
    drw(bxV, bxC, m, { 0.10f, 0.09f, 0.08f }, 0);
    // ------------------------------------------------------------------ //
    // 4.  INTERIOR — UPPER FLOOR (bedroom)
    // ------------------------------------------------------------------ //
    m = glm::translate(b, { 0.0f, 3.55f, -2.35f });
    m = glm::scale(m, { sx * 2.60f, 0.90f, 0.06f });
    u("objectColor", lavenderWall);
    drwTex(bxV, bxC, m, tWall, 8);
    u("objectColor", glm::vec3(1.f));
    for (float xs : { -1.f, 1.f }) {
        m = glm::translate(b, { xs * sx * 2.58f, 3.55f, 0.0f });
        m = glm::scale(m, { 0.06f, 0.90f, 2.32f });
        u("objectColor", lavenderWall);
        drwTex(bxV, bxC, m, tWall, 8);
        u("objectColor", glm::vec3(1.f));
    }

    // Bed (fabric = plain colour, frame = wood)
    m = glm::translate(b, { sx * 0.70f, 2.72f, -1.90f });
    m = glm::scale(m, { 0.50f, 0.16f, 0.72f });
    drw(bxV, bxC, m, { 0.85f, 0.88f, 1.00f }, 0);
    m = glm::translate(b, { sx * 0.70f, 2.98f, -2.40f });
    m = glm::scale(m, { 0.50f, 0.34f, 0.06f });
    u("objectColor", glm::vec3(0.96f, 0.82f, 0.82f));
    drwTex(bxV, bxC, m, tWood, 8);
    u("objectColor", glm::vec3(1.f));

    // Bookshelf + coloured books
    m = glm::translate(b, { -sx * 0.80f, 3.00f, -2.30f });
    m = glm::scale(m, { 0.38f, 0.50f, 0.12f });
    drwTex(bxV, bxC, m, tWood, 8);
    for (int bk = 0; bk < 5; bk++) {
        glm::vec3 bkc = (bk % 2 == 0) ? glm::vec3(0.70f, 0.20f, 0.25f) : glm::vec3(0.20f, 0.40f, 0.70f);
        m = glm::translate(b, { -sx * (0.97f - bk * 0.14f), 3.06f, -2.24f });
        m = glm::scale(m, { 0.06f, 0.36f, 0.04f });
        drw(bxV, bxC, m, bkc, 0);
    }

    // Stairwell dark slot
    m = glm::translate(b, { -sx * 1.80f, 2.50f, 1.50f });
    m = glm::scale(m, { 0.36f, 0.08f, 0.50f });
    drw(bxV, bxC, m, { 0.12f, 0.10f, 0.08f }, 0);

    // ------------------------------------------------------------------ //
    // 5.  ROOFS — main gable + two turrets + dormers
    // ------------------------------------------------------------------ //
    // Main steeply pitched gable (textured)
    m = glm::translate(b, { 0.0f, 4.76f, 0.0f });
    m = glm::scale(m, { sx * 2.90f, 2.80f, 2.65f });
    u("model", m);
    u("objectColor", roseRoof);   // rose tint blended with texture
    u("matType", 8);
    u("isEmissive", 0);
    if (tRoof > 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tRoof);
        u("tex0", 0);
    }
    glBindVertexArray(rfV);
    glDrawArrays(GL_TRIANGLES, 0, rfC);
    u("objectColor", glm::vec3(1.f));

  
    // Left turret
    {
        float tx = -sx * 2.35f;
        m = glm::translate(b, { tx, 4.00f, -1.60f });
        m = glm::scale(m, { 0.55f, 2.20f, 0.55f });
        u("objectColor", lavenderWall);
        drwTex(cyV, cyC, m, tWall, 8);
        u("objectColor", glm::vec3(1.f));
        m = glm::translate(b, { tx, 6.22f, -1.60f });
        m = glm::scale(m, { 0.68f, 1.80f, 0.68f });
        u("model", m);
        u("objectColor", roseRoof * 0.85f);
        u("matType", 8); u("isEmissive", 0);
        if (tRoof > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tRoof); u("tex0", 0); }
        glBindVertexArray(cnV); glDrawArrays(GL_TRIANGLES, 0, cnC);
        u("objectColor", glm::vec3(1.f));
        // Window
        m = glm::translate(b, { tx, 4.80f, -1.60f + 0.57f });
        m = glm::scale(m, { 0.18f, 0.28f, 0.04f });
        if (tWin > 0) drwTex(bxV, bxC, m, tWin, 7);
        else drw(bxV, bxC, m, skyBlueWin, 0, !g_day);
    }
    // Right turret
    {
        float tx = sx * 2.35f;
        m = glm::translate(b, { tx, 3.70f, 1.60f });
        m = glm::scale(m, { 0.48f, 1.90f, 0.48f });
        u("objectColor", lavenderWall);
        drwTex(cyV, cyC, m, tWall, 8);
        u("objectColor", glm::vec3(1.f));
        m = glm::translate(b, { tx, 5.62f, 1.60f });
        m = glm::scale(m, { 0.60f, 1.50f, 0.60f });
        u("model", m);
        u("objectColor", roseRoof * 0.85f);
        u("matType", 8); u("isEmissive", 0);
        if (tRoof > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tRoof); u("tex0", 0); }
        glBindVertexArray(cnV); glDrawArrays(GL_TRIANGLES, 0, cnC);
        u("objectColor", glm::vec3(1.f));
        m = glm::translate(b, { tx, 4.40f, 1.60f + 0.50f });
        m = glm::scale(m, { 0.16f, 0.24f, 0.04f });
        if (tWin > 0) drwTex(bxV, bxC, m, tWin, 7);
        else drw(bxV, bxC, m, skyBlueWin, 0, !g_day);
    }

    // Dormers
    for (float dx2 : { -sx * 1.10f, sx * 1.10f }) {
        m = glm::translate(b, { dx2, 5.90f, 2.30f });
        m = glm::scale(m, { 0.45f, 0.38f, 0.30f });
        u("objectColor", creamWall);
        drwTex(bxV, bxC, m, tWall, 8);
        u("objectColor", glm::vec3(1.f));
        m = glm::translate(b, { dx2, 6.30f, 2.30f });
        m = glm::scale(m, { 0.50f, 0.42f, 0.32f });
        u("model", m);
        u("objectColor", roseRoof);
        u("matType", 8); u("isEmissive", 0);
        if (tRoof > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tRoof); u("tex0", 0); }
        glBindVertexArray(rfV); glDrawArrays(GL_TRIANGLES, 0, rfC);
        u("objectColor", glm::vec3(1.f));
        m = glm::translate(b, { dx2, 5.90f, 2.62f });
        m = glm::scale(m, { 0.28f, 0.28f, 0.04f });
        if (tWin > 0) drwTex(bxV, bxC, m, tWin, 7);
        else drw(bxV, bxC, m, skyBlueWin, 0, !g_day);
    }

    // ------------------------------------------------------------------ //
    // 6.  CHIMNEYS (brick/stone textured)
    // ------------------------------------------------------------------ //
    for (float cxOff : { sx * 1.55f, -sx * 0.80f }) {
        m = glm::translate(b, { cxOff, 6.50f, -0.70f });
        m = glm::scale(m, { 0.20f, 1.50f, 0.20f });
        u("objectColor", stoneBase);
        drwTex(bxV, bxC, m, tStone, 8);
        u("objectColor", glm::vec3(1.f));
        m = glm::translate(b, { cxOff, 8.05f, -0.70f });
        m = glm::scale(m, { 0.24f, 0.10f, 0.24f });
        drw(bxV, bxC, m, { 0.28f, 0.26f, 0.24f }, 0);
    }

    // ------------------------------------------------------------------ //
    // 7.  FRONT DOOR  (teal wood + stone arch)
    // ------------------------------------------------------------------ //
    float doorX = flipDoor ? sx * 0.85f : -sx * 0.85f;

    // Stone porch steps
    for (int si = 0; si < 4; si++) {
        m = glm::translate(b, { doorX, 0.10f + si * 0.13f, 2.30f + si * 0.20f });
        m = glm::scale(m, { 0.70f - si * 0.06f, 0.07f, 0.20f });
        u("objectColor", stoneBase);
        drwTex(bxV, bxC, m, tStone, 8);
        u("objectColor", glm::vec3(1.f));
    }

    // Porch canopy slab + mini-roof
    m = glm::translate(b, { doorX, 2.05f, 2.55f });
    m = glm::scale(m, { 0.70f, 0.06f, 0.40f });
    drw(bxV, bxC, m, coralTrim, 0);
    m = glm::translate(b, { doorX, 2.05f, 2.55f });
    m = glm::scale(m, { 0.72f, 0.40f, 0.42f });
    u("objectColor", roseRoof);
    drwTex(rfV, rfC, m, tRoof, 8);
    u("objectColor", glm::vec3(1.f));

    // Door frame (stone)
    m = glm::translate(b, { doorX, 1.30f, 2.28f });
    m = glm::scale(m, { 0.46f, 1.00f, 0.06f });
    u("objectColor", stoneBase);
    drwTex(bxV, bxC, m, tStone, 8);
    u("objectColor", glm::vec3(1.f));

    
    // Door panel — swings open on H key (rotates around hinge at doorX edge)
    {
        float swingAngle = g_houseDoorFactor * glm::radians(-90.f); // swings outward
        float hingeX = doorX - sx * 0.40f; // hinge at left edge of door
        glm::mat4 dm = glm::translate(b, { hingeX, 1.28f, 2.30f });
        dm = glm::rotate(dm, swingAngle, { 0, 1, 0 });
        dm = glm::translate(dm, { sx * 0.40f, 0.f, 0.f }); // pivot offset
        dm = glm::scale(dm, { 0.40f, 0.92f, 0.05f });
        u("objectColor", roseRoof);
        u("model", dm);
        u("matType", 7);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tWall);
        u("tex0", 0);
        glBindVertexArray(bxV);
        glDrawArrays(GL_TRIANGLES, 0, bxC);
        u("objectColor", glm::vec3(1.f));
    }

    // Round fanlight window above door
    m = glm::translate(b, { doorX, 2.20f, 2.30f });
    m = glm::scale(m, { 0.22f, 0.22f, 0.04f });
    if (tWin > 0) drwTex(spV, spC, m, tWin, 7);
    else drw(spV, spC, m, skyBlueWin, 0, !g_day);

    // Door knob
    m = glm::translate(b, { doorX + sx * 0.24f, 1.28f, 2.33f });
    m = glm::scale(m, { 0.04f, 0.04f, 0.04f });
    drw(spV, spC, m, goldTrim, 0);

    // ------------------------------------------------------------------ //
    // 8.  GROUND FLOOR WINDOWS  (textured panes + coral shutters)
    // ------------------------------------------------------------------ //
    for (float wx : { -sx * 1.35f, sx * 1.35f }) {
        // Sill (stone)
        m = glm::translate(b, { wx, 1.28f, 2.28f });
        m = glm::scale(m, { 0.44f, 0.06f, 0.12f });
        u("objectColor", stoneBase);
        drwTex(bxV, bxC, m, tStone, 8);
        u("objectColor", glm::vec3(1.f));
        // Pane
        m = glm::translate(b, { wx, 1.52f, 2.29f });
        m = glm::scale(m, { 0.34f, 0.44f, 0.04f });
        if (tWin > 0) drwTex(bxV, bxC, m, tWin, 7);
        else drw(bxV, bxC, m, skyBlueWin, 0, !g_day);
        // Arch crown
        m = glm::translate(b, { wx, 1.98f, 2.29f });
        m = glm::scale(m, { 0.34f, 0.22f, 0.04f });
        if (tWin > 0) drwTex(spV, spC, m, tWin, 7);
        else drw(spV, spC, m, skyBlueWin * 0.85f, 0, !g_day);
        // Shutters (wood, coral-tinted)
        m = glm::translate(b, { wx - sx * 0.25f, 1.60f, 2.27f });
        m = glm::rotate(m, glm::radians(-g_shutterSwing * 70.f), { 0, 1, 0 });
        m = glm::scale(m, { 0.14f, 0.55f, 0.04f });
        u("objectColor", coralTrim);
        drwTex(bxV, bxC, m, tWood, 8);
        m = glm::translate(b, { wx + sx * 0.25f, 1.60f, 2.27f });
        m = glm::rotate(m, glm::radians(g_shutterSwing * 70.f), { 0, 1, 0 });
        m = glm::scale(m, { 0.14f, 0.55f, 0.04f });
        drwTex(bxV, bxC, m, tWood, 8);
        u("objectColor", glm::vec3(1.f));
    }

    // ------------------------------------------------------------------ //
    // 9.  SECOND FLOOR WINDOWS  (3 arched, textured)
    // ------------------------------------------------------------------ //
    for (float wx : { -sx * 1.55f, 0.0f, sx * 1.55f }) {
        m = glm::translate(b, { wx, 3.55f, 2.47f });
        m = glm::scale(m, { 0.32f, 0.48f, 0.04f });
        if (tWin > 0) drwTex(bxV, bxC, m, tWin, 7);
        else drw(bxV, bxC, m, skyBlueWin, 0, !g_day);
        m = glm::translate(b, { wx, 4.05f, 2.47f });
        m = glm::scale(m, { 0.32f, 0.26f, 0.04f });
        if (tWin > 0) drwTex(spV, spC, m, tWin, 7);
        else drw(spV, spC, m, skyBlueWin * 0.80f, 0, !g_day);
        // Flower box ledge
        m = glm::translate(b, { wx, 3.25f, 2.52f });
        m = glm::scale(m, { 0.38f, 0.06f, 0.14f });
        drw(bxV, bxC, m, coralTrim, 0);
    }

    // ------------------------------------------------------------------ //
    // 10.  DECORATIVE DETAILS — flower boxes, lanterns, finial
    // ------------------------------------------------------------------ //
    // Flower boxes (terracotta + coloured blooms)
    for (float wx : { -sx * 1.35f, sx * 1.35f }) {
        m = glm::translate(b, { wx, 1.05f, 2.45f });
        m = glm::scale(m, { 0.36f, 0.10f, 0.16f });
        u("objectColor", glm::vec3(0.42f, 0.25f, 0.12f));
        drwTex(bxV, bxC, m, tStone, 8);
        u("objectColor", glm::vec3(1.f));
        for (int fi = 0; fi < 3; fi++) {
            glm::vec3 fc = (fi == 0) ? glm::vec3(1.0f, 0.30f, 0.50f)
                : (fi == 1) ? glm::vec3(1.0f, 0.85f, 0.15f)
                : glm::vec3(0.30f, 0.70f, 1.00f);
            m = glm::translate(b, { wx + sx * (fi - 1) * 0.11f, 1.22f, 2.48f });
            m = glm::scale(m, { 0.06f, 0.06f, 0.06f });
            drw(spV, spC, m, fc, 0, true);
        }
    }

    // Lantern posts
    for (float lx : { doorX - sx * 0.52f, doorX + sx * 0.52f }) {
        m = glm::translate(b, { lx, 1.15f, 2.35f });
        m = glm::scale(m, { 0.04f, 1.15f, 0.04f });
        u("objectColor", glm::vec3(0.25f, 0.22f, 0.18f));
        drwTex(cyV, cyC, m, tWood, 8);
        u("objectColor", glm::vec3(1.f));
        m = glm::translate(b, { lx, 2.35f, 2.35f });
        m = glm::scale(m, { 0.10f, 0.14f, 0.10f });
        drw(bxV, bxC, m, goldTrim, 0);
        glm::vec3 lGlow = g_day ? glm::vec3(0.9f, 0.7f, 0.3f) : glm::vec3(1.0f, 0.85f, 0.35f);
        m = glm::translate(b, { lx, 2.35f, 2.35f });
        m = glm::scale(m, { 0.08f, 0.10f, 0.08f });
        drw(spV, spC, m, lGlow, 0, !g_day);
    }

    
}
 

// ---------------------------------------------------------------------------
// Smooth Bézier path — draws a textured road strip along a cubic Bézier curve.
// ---------------------------------------------------------------------------
static void drawBezierPath(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
    float width = 1.5f, int segments = 24)
{
    auto bezier = [&](float t) -> glm::vec3 {
        float u = 1.f - t;
        return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
        };

    GLuint roadTex = (texPath > 0) ? texPath : (texStone1 > 0 ? texStone1 : 0);

    for (int i = 0; i < segments; i++) {
        float t0 = (float)i / segments;
        float t1 = (float)(i + 1) / segments;

        glm::vec3 a = bezier(t0);
        glm::vec3 b = bezier(t1);
        glm::vec3 mid = (a + b) * 0.5f;
        float len = glm::length(b - a);
        if (len < 1e-4f) continue;

        // Orient slab along the segment direction
        float angle = atan2f(b.x - a.x, b.z - a.z);

        // Main road slab
        glm::mat4 m = glm::translate(glm::mat4(1), { mid.x, 0.016f, mid.z });
        m = glm::rotate(m, angle, { 0, 1, 0 });
        m = glm::scale(m, { width, 0.014f, len * 0.52f });
        if (roadTex > 0) drwTex(bxV, bxC, m, roadTex, 8);
        else             drw(bxV, bxC, m, { 0.52f, 0.48f, 0.42f }, 2);

        // Subtle centre-line highlight
        glm::mat4 cl = glm::translate(glm::mat4(1), { mid.x, 0.022f, mid.z });
        cl = glm::rotate(cl, angle, { 0, 1, 0 });
        cl = glm::scale(cl, { width * 0.18f, 0.006f, len * 0.51f });
        drw(bxV, bxC, cl, { 0.30f, 0.27f, 0.22f }, 0);
    }
}
// ── Toy Rocket (in front of house) ───────────────────────────────────────

static void drawG(glm::vec3 pos)
{
    glm::mat4 b = glm::translate(glm::mat4(1), pos);
    glm::mat4 m;

    // ?? Color fallback (used when no texture) ?????????????????????????????
    glm::vec3 stoneCol = { 0.50f, 0.46f, 0.40f };
    glm::vec3 roofCol = { 0.20f, 0.28f, 0.18f }; // dark slate green
    glm::vec3 gateCol = { 0.22f, 0.18f, 0.12f };


    const float TW = 7.0f;   // half-width of keep
    const float TH = 12.0f;  // tower height half (cylinder goes +-TH)
    const float TR = 1.8f;   // tower radius
    const float WH = 8.0f;   // curtain-wall half-height
    const float WT = 0.7f;   // curtain-wall half-thickness


    // GROUND
    m = glm::translate(b, { 0.f, 0.02f, 0.f });
    m = glm::scale(m, { TW - TR, 0.02f, TW - TR });
    drwTex(bxV, bxC, m, texCastle, 8);
}

static const int N_DROPS = 750;
struct Drop { float dx, dz, y, spd; };
static Drop   g_drops[N_DROPS];
static GLuint g_rainVAO, g_rainVBO;


static void update(float dt)
{

    static std::vector<float> rv;
    rv.clear(); rv.reserve(N_DROPS * 12);
    float nx[3] = { 0,1,0 };
    for (auto& d : g_drops) {
        float wx = cam.pos.x + d.dx, wz = cam.pos.z + d.dz;
        rv.push_back(wx);       rv.push_back(d.y + 0.28f); rv.push_back(wz);
        rv.push_back(nx[0]); rv.push_back(nx[1]); rv.push_back(nx[2]);
        rv.push_back(wx + 0.05f); rv.push_back(d.y);       rv.push_back(wz);
        rv.push_back(nx[0]); rv.push_back(nx[1]); rv.push_back(nx[2]);
    }
    glBindBuffer(GL_ARRAY_BUFFER, g_rainVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, rv.size() * sizeof(float), rv.data());
}


// =============================================================================
// ?? Scenes – each now takes a glm::vec3 
// =============================================================================

static void drawSkyDome()
{
    glDepthMask(GL_FALSE);
    u("isBackground", 1);


    // We can center it directly at cam.pos (so equator meshes with the horizon)
    glm::mat4 m = glm::translate(glm::mat4(1), cam.pos + glm::vec3(0, -5.f, 0));
    m = glm::scale(m, { -240.f, 200.f, 240.f });
    u("model", m);

    // Smooth blending color with day/night and rain states
    glm::vec3 baseCol = g_day ? glm::vec3(1.0f) : glm::vec3(0.08f, 0.10f, 0.20f);
    if (g_rain) baseCol *= 0.45f;

    u("objectColor", baseCol);
    u("matType", 7); // Using basic texture matType
    u("isEmissive", 1); // Rendering self-illuminated to not have awkward normals

    if (texHorizon > 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texHorizon);
        u("tex0", 0);
    }

    glBindVertexArray(sdV);
    glDrawArrays(GL_TRIANGLES, 0, sdC);

    u("isBackground", 0);
    glDepthMask(GL_TRUE);
}


static void drawSceneT(glm::vec3 off)
{
    float fx = off.x, fz = off.z;

    // ?? 3. Trees
    for (int i = 0; i < 6; i++) {
        drawTree({ -5.f - i * 1.8f + fx, 0.f, -3.f - i * 2.2f + fz }, 1.2f + i * 0.05f, i + 20);
        drawTree({ 5.f + i * 1.8f + fx, 0.f, -3.f - i * 2.2f + fz }, 1.2f + i * 0.05f, i + 30);
    }
}


static void drawSceneCampsite(float t, glm::vec3 off)
{
    float z = -37.f + off.z;

    float lx = off.x - 8.0f; // Left offset from road center


    // Trees surrounding campsite on the left side — natural arc placement
    drawTree({ lx - 10.0f, 0, z - 5.0f }, 1.30f, 40);
    drawTree({ lx - 7.5f,  0, z - 8.0f }, 1.15f, 41);
    drawTree({ lx - 3.0f,  0, z - 9.0f }, 1.22f, 42);
    drawTree({ lx + 2.0f,  0, z - 8.5f }, 1.38f, 43);
    drawTree({ lx + 4.5f,  0, z - 5.5f }, 1.20f, 44);
    drawTree({ lx + 5.0f,  0, z - 1.0f }, 1.32f, 45);
    drawTree({ lx + 4.0f,  0, z + 5.0f }, 1.10f, 46);
    drawTree({ lx - 9.0f,  0, z + 4.0f }, 1.28f, 47);
    drawTree({ lx - 11.f,  0, z + 1.0f }, 1.18f, 48);
    // Additional trees between campsite and road for natural screening
    drawTree({ off.x - 4.5f, 0, z - 3.0f }, 1.05f, 140);
    drawTree({ off.x - 5.0f, 0, z + 2.5f }, 0.95f, 141);
    drawTree({ off.x - 4.0f, 0, z + 6.0f }, 1.12f, 142);


}

static void drawScene(glm::vec3 off)
{
    float z = -77.f + off.z;
    const float RX = off.x; // Road X


    // West
    drawTree({ RX - 14.f, 0, z - 20.f }, 1.3f, 50);
    drawTree({ RX - 18.f, 0, z - 15.f }, 1.1f, 51);
    drawTree({ RX - 16.f, 0, z - 25.f }, 1.2f, 52);

    // East
    drawTree({ RX + 22.f, 0, z + 5.f }, 1.25f, 53);
    drawTree({ RX + 20.f, 0, z + 120.f }, 1.05f, 54);
    drawTree({ RX + 24.f, 0, z + 2.f }, 1.15f, 55);

    //  trees
    drawTree({ RX - 8.f,  0, z - 45.f }, 1.1f, 56);
    drawTree({ RX + 12.f, 0, z - 45.f }, 1.2f, 57);
}



static void drawScene1(glm::vec3 off)
{
    float z = -183.f + off.z;

    for (int i = 0; i < 5; i++) {
        drawTree({ -14.f + off.x,0,z - 18.f + i * 8.f }, 1.1f + i * 0.05f, 80 + i);
        drawTree({ 14.f + off.x,0,z - 18.f + i * 8.f }, 1.1f + i * 0.05f, 85 + i);
    }

}

// =============================================================================
// ?? Input ?????????????????????????????????????????????????????????????????????
// =============================================================================

// ---------- Per-frame keyboard movement using BasicCamera ---------
static void processInput(GLFWwindow* win, float dt)
{
    // Keyboard-only: always active (no mouse capture check needed)
    if (g_orbitHouse || g_birdView) return;

    // ---- Sprint: set BEFORE movement so it takes effect this frame ----
    float savedSpeed = camera.MovementSpeed;
    if (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
        camera.MovementSpeed = savedSpeed * 2.5f;

    glm::vec3 prevPos = camera.Position;

    // ---- WASD: translate (W/S = forward/back, A/D = strafe) ----
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, dt);
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, dt);
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, dt);
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, dt);

    // ---- E / Q: fly up / down ----
    if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, dt);
    if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, dt);

    // Restore base speed
    camera.MovementSpeed = savedSpeed;

    // ---- Arrow keys: LEFT/RIGHT = Yaw, UP/DOWN = Pitch ----
    if (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS)
        camera.ProcessYaw(-60.f * dt);
    if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS)
        camera.ProcessYaw(60.f * dt);
    if (glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS)
        camera.ProcessPitch(40.f * dt);
    if (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS)
        camera.ProcessPitch(-40.f * dt);

    // ---- R / F: pitch up / down (alternative) ----
    if (glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS)
        camera.ProcessPitch(40.f * dt);
    if (glfwGetKey(win, GLFW_KEY_F) == GLFW_PRESS) {
        // F is also orbit-toggle in cbKey; only use for pitch when NOT orbit mode
        if (!g_orbitHouse)
            camera.ProcessPitch(-40.f * dt);
    }

    // ---- Y: Yaw rotation (keyboard look-left/right alternative) ----
    // Y+Shift = yaw left,  Y alone = yaw right
    if (glfwGetKey(win, GLFW_KEY_Y) == GLFW_PRESS) {
        float dir = (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
            ? -1.f : 1.f;
        camera.ProcessYaw(60.f * dir * dt);
    }

    // ---- Z / X: Roll tilt ----
    if (glfwGetKey(win, GLFW_KEY_Z) == GLFW_PRESS)
        camera.ProcessRoll(-40.f * dt);
    if (glfwGetKey(win, GLFW_KEY_X) == GLFW_PRESS)
        camera.ProcessRoll(40.f * dt);

    // ---- Collision rollback ----
    if (checkObs(camera.Position))
        camera.Position = prevPos;
}


void cbKey(GLFWwindow* win, int key, int, int action, int) {
    // ESC always closes the window (no cursor-release step needed)
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(win, true);
    if (key == GLFW_KEY_N && action == GLFW_PRESS) {
        g_day = !g_day;
        g_barnLanternsOn = !g_day; // Keep barn lights in sync when environment changes
    }
    if (key == GLFW_KEY_T && action == GLFW_PRESS) g_rain = !g_rain;

    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        g_birdView = false;
        g_orbitHouse = !g_orbitHouse;
        if (g_orbitHouse) {
            glm::vec3 t = g_orbitTarget;
            float dx = cam.pos.x - t.x, dz = cam.pos.z - t.z;
            g_orbitRadius = (std::max)(6.f, sqrtf(dx * dx + dz * dz));
            g_orbitAngle = atan2f(dz, dx);
        }
        else {
            cam.pitch = glm::degrees(asinf(glm::clamp(cam.front.y, -1.f, 1.f)));
            cam.yaw = glm::degrees(atan2f(cam.front.z, cam.front.x));
        }
    }
    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        g_orbitHouse = false;
        if (!g_birdView) {
            g_birdSavedPos = cam.pos;
            g_birdSavedYaw = cam.yaw; g_birdSavedPitch = cam.pitch; g_birdSavedRoll = cam.roll;
            g_birdView = true;
        }
        else {
            g_birdView = false;
            cam.pos = g_birdSavedPos;
            cam.yaw = g_birdSavedYaw; cam.pitch = g_birdSavedPitch; cam.roll = g_birdSavedRoll;
            cam.updateDir();
        }
    }

    if (key == GLFW_KEY_H && action == GLFW_PRESS) {
        g_houseDoorOpen = !g_houseDoorOpen;
        PlaySoundA("SystemDefault", NULL, SND_ASYNC);
    }
    if (key == GLFW_KEY_L && action == GLFW_PRESS) {
        if (!g_roosterWalking) {
            // Launch the parabolic fall from roof
            g_roosterWalking = true;
            g_roosterOnGround = false;
            g_roosterParabT = 0.f;
            g_roosterWalkPos = ROOSTER_START;
        }
        else {
            // Reset back to roof
            g_roosterWalking = false;
            g_roosterOnGround = false;
            g_roosterParabT = 0.f;
            g_roosterWalkPos = ROOSTER_START;
        }
        PlaySoundA("SystemDefault", NULL, SND_ASYNC);
    }
    // U = launch rocket up, J = return rocket to ground
    if (key == GLFW_KEY_U && action == GLFW_PRESS)  g_rocketUp = true;
    if (key == GLFW_KEY_U && action == GLFW_RELEASE) g_rocketUp = false;
    if (key == GLFW_KEY_J && action == GLFW_PRESS) {
        g_rocketUp = false;
        g_rocketTargetY = 0.f;  // return home
    }

    if (key == GLFW_KEY_O && action == GLFW_PRESS) {
        g_carLightsOn = !g_carLightsOn;
        PlaySoundA("SystemDefault", NULL, SND_ASYNC);
    }

    auto toggleOnPress = [&](int k, bool& b) {
        if (key == k && action == GLFW_PRESS) b = !b;
        };
    toggleOnPress(GLFW_KEY_1, g_lightDir);
    toggleOnPress(GLFW_KEY_2, g_lightPoint);
    toggleOnPress(GLFW_KEY_3, g_lightSpot);
    toggleOnPress(GLFW_KEY_5, g_lightAmb);
    toggleOnPress(GLFW_KEY_6, g_lightDiff);
    toggleOnPress(GLFW_KEY_7, g_lightSpec);

    

}
static void drawToyRocket(glm::vec3 pos, float now, float liftY, float spinAngle = 0.f)
{
    glm::vec3 colBody = { 0.92f, 0.18f, 0.18f };  // red body
    glm::vec3 colNose = { 0.95f, 0.85f, 0.20f };  // yellow nose cone
    glm::vec3 colFin = { 0.20f, 0.35f, 0.90f };  // blue fins
    glm::vec3 colWindow = { 0.65f, 0.92f, 1.00f };  // cyan porthole
    glm::vec3 colBand = { 0.95f, 0.95f, 0.95f };  // white band stripe
    glm::vec3 colNozzle = { 0.28f, 0.26f, 0.24f };  // dark nozzle
    glm::vec3 colFlame = { 1.00f, 0.55f, 0.10f };  // orange flame
    glm::vec3 colFlame2 = { 1.00f, 0.90f, 0.20f };  // yellow flame core

    glm::vec3 rp = pos + glm::vec3(0, liftY, 0);
    glm::mat4 base = glm::translate(glm::mat4(1), rp);
    base = glm::rotate(base, spinAngle, { 0, 1, 0 });
    glm::mat4 m;

    bool flying = liftY > 0.05f;

    // ── Body (main cylinder) ──────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.65f, 0.f });
    m = glm::scale(m, { 0.18f, 0.85f, 0.18f });
    drw(cyV, cyC, m, colBody, 0);

    // ── White band stripe (mid-body ring) ────────────────────────────────
    m = glm::translate(base, { 0.f, 0.72f, 0.f });
    m = glm::scale(m, { 0.195f, 0.06f, 0.195f });
    drw(cyV, cyC, m, colBand, 0);

    // ── Second stripe (lower body) ────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.52f, 0.f });
    m = glm::scale(m, { 0.192f, 0.04f, 0.192f });
    drw(cyV, cyC, m, colBand, 0);

    // ── Nose cone ────────────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 1.42f, 0.f });
    m = glm::scale(m, { 0.18f, 0.42f, 0.18f });
    drw(cnV, cnC, m, colNose, 0);

    // ── Nose tip sphere (shiny cap) ───────────────────────────────────────
    m = glm::translate(base, { 0.f, 1.84f, 0.f });
    m = glm::scale(m, { 0.06f, 0.06f, 0.06f });
    drw(spV, spC, m, colNose, 0);

    // ── Porthole window ───────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.90f, 0.19f });
    m = glm::scale(m, { 0.07f, 0.07f, 0.04f });
    drw(spV, spC, m, colWindow, 0, !g_day);

    // Window rim
    m = glm::translate(base, { 0.f, 0.90f, 0.185f });
    m = glm::scale(m, { 0.09f, 0.09f, 0.03f });
    drw(cyV, cyC, m, colBand, 0);

    // ── 3 Fins (evenly spaced 120° apart) ────────────────────────────────
    for (int fi = 0; fi < 3; fi++) {
        float angle = fi * (2.f * PI / 3.f);
        glm::mat4 fm = glm::translate(base, { 0.f, 0.25f, 0.f });
        fm = glm::rotate(fm, angle, { 0, 1, 0 });
        fm = glm::translate(fm, { 0.26f, 0.f, 0.f });
        fm = glm::rotate(fm, glm::radians(90.f), { 0, 0, 1 });
        fm = glm::scale(fm, { 0.22f, 0.08f, 0.14f });
        drw(bxV, bxC, fm, colFin, 0);

        // Fin edge highlight
        glm::mat4 fe = glm::translate(base, { 0.f, 0.10f, 0.f });
        fe = glm::rotate(fe, angle, { 0, 1, 0 });
        fe = glm::translate(fe, { 0.34f, 0.f, 0.f });
        fe = glm::rotate(fe, glm::radians(90.f), { 0, 0, 1 });
        fe = glm::scale(fe, { 0.04f, 0.04f, 0.18f });
        drw(cyV, cyC, fe, colFin * 0.75f, 0);
    }

    // ── Nozzle bell ───────────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.02f, 0.f });
    m = glm::scale(m, { 0.13f, 0.12f, 0.13f });
    drw(cyV, cyC, m, colNozzle, 0);

    // Nozzle inner dark hole
    m = glm::translate(base, { 0.f, -0.04f, 0.f });
    m = glm::scale(m, { 0.07f, 0.06f, 0.07f });
    drw(cyV, cyC, m, glm::vec3(0.06f, 0.05f, 0.05f), 0);

    // ── Launch pad / stand (only visible on ground) ───────────────────────
    if (!flying) {
        // Three landing legs
        for (int li = 0; li < 3; li++) {
            float la = li * (2.f * PI / 3.f) + PI / 6.f;
            glm::mat4 leg = glm::translate(base, { 0.f, 0.10f, 0.f });
            leg = glm::rotate(leg, la, { 0, 1, 0 });
            leg = glm::translate(leg, { 0.18f, 0.f, 0.f });
            leg = glm::rotate(leg, glm::radians(28.f), { 0, 0, 1 });
            leg = glm::scale(leg, { 0.025f, 0.22f, 0.025f });
            drw(cyV, cyC, leg, colNozzle, 0);
            // Foot pad
            glm::mat4 fp = glm::translate(base, {
                cosf(la) * 0.27f, 0.01f, sinf(la) * 0.27f });
            fp = glm::scale(fp, { 0.055f, 0.015f, 0.055f });
            drw(cyV, cyC, fp, colNozzle, 0);
        }
        // Small launch pad base ring
        m = glm::translate(base, { 0.f, 0.005f, 0.f });
        m = glm::scale(m, { 0.35f, 0.012f, 0.35f });
        drw(cyV, cyC, m, glm::vec3(0.30f, 0.28f, 0.26f), 0);
    }

    // ── Exhaust flame (only when flying or just launched) ─────────────────
    if (flying || liftY > 0.001f) {
        float flicker1 = 0.55f + 0.45f * sinf(now * 18.f + 0.3f);
        float flicker2 = 0.50f + 0.50f * sinf(now * 23.f + 1.1f);
        float flicker3 = 0.45f + 0.55f * sinf(now * 31.f + 2.4f);

        // Outer flame cone
        glm::mat4 fl = glm::translate(base, { 0.f, -0.10f, 0.f });
        fl = glm::rotate(fl, glm::radians(180.f), { 1, 0, 0 });
        fl = glm::scale(fl, { 0.11f * flicker1, 0.45f * flicker2, 0.11f * flicker1 });
        drw(cnV, cnC, fl, colFlame, 0, true);

        // Inner bright core flame
        glm::mat4 fl2 = glm::translate(base, { 0.f, -0.10f, 0.f });
        fl2 = glm::rotate(fl2, glm::radians(180.f), { 1, 0, 0 });
        fl2 = glm::scale(fl2, { 0.06f * flicker2, 0.30f * flicker3, 0.06f * flicker2 });
        drw(cnV, cnC, fl2, colFlame2, 0, true);

        // Flame glow halo sphere at nozzle exit
        glm::mat4 flh = glm::translate(base, { 0.f, -0.14f, 0.f });
        flh = glm::scale(flh, { 0.12f * flicker3, 0.08f, 0.12f * flicker3 });
        drw(spV, spC, flh, colFlame * 0.70f, 0, true);
    }

    // ── Star decal on body (small 5-point star approximated as sphere) ────
    m = glm::translate(base, { 0.f, 0.65f, 0.19f });
    m = glm::scale(m, { 0.04f, 0.04f, 0.02f });
    drw(spV, spC, m, glm::vec3(1.f, 1.f, 0.9f), 0, true);
}
static void drawToyCar(glm::vec3 pos, float yawDeg, float wheelRot,
    float steer, bool lightsOn, float now)
{
    glm::vec3 colBody = { 0.90f, 0.10f, 0.10f };
    glm::vec3 colBodyDark = { 0.60f, 0.05f, 0.05f };
    glm::vec3 colRoof = { 0.80f, 0.07f, 0.07f };
    glm::vec3 colWindow = { 0.55f, 0.80f, 0.98f };
    glm::vec3 colRim = { 0.88f, 0.72f, 0.20f };
    glm::vec3 colTyre = { 0.12f, 0.11f, 0.11f };
    glm::vec3 colChrome = { 0.80f, 0.78f, 0.72f };
    glm::vec3 colUnder = { 0.22f, 0.20f, 0.18f };
    glm::vec3 colStripe = { 0.96f, 0.96f, 0.96f };
    glm::vec3 colLightOn = { 1.00f, 0.97f, 0.85f };
    glm::vec3 colLightOff = { 0.88f, 0.86f, 0.74f };
    glm::vec3 colTailOn = { 1.00f, 0.12f, 0.10f };
    glm::vec3 colTailOff = { 0.55f, 0.08f, 0.08f };

    GLuint tBody = (texcar > 0) ? texcar : 0;
   

    GLuint tWheel = (texStone2 > 0) ? texStone2 : 0;
    

    GLuint tGlass = (texWindows > 0) ? texWindows : 0;

    glm::mat4 base = glm::translate(glm::mat4(1.f), pos);
    base = glm::rotate(base, glm::radians(yawDeg), { 0.f, 1.f, 0.f });
    glm::mat4 m;

    // ── Chassis underplate ────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.07f, 0.f });
    m = glm::scale(m, { 0.50f, 0.05f, 1.00f });
    drw(bxV, bxC, m, colUnder, 0);

    // ── Lower hull ────────────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.19f, 0.f });
    m = glm::scale(m, { 0.52f, 0.13f, 1.05f });
    u("model", m);
    u("objectColor", colBodyDark);  // DARK RED tint mixed with texture
    u("matType", 8);
    u("isEmissive", 0);
    if (tBody > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tBody); u("tex0", 0); }
    glBindVertexArray(bxV); glDrawArrays(GL_TRIANGLES, 0, bxC);


    // ── Main body shell ───────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.40f, 0.f });
    m = glm::scale(m, { 0.50f, 0.22f, 1.00f });
    u("model", m);
    u("objectColor", colBody);      // RED tint mixed with texture
    u("matType", 8);
    u("isEmissive", 0);
    if (tBody > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tBody); u("tex0", 0); }
    glBindVertexArray(bxV); glDrawArrays(GL_TRIANGLES, 0, bxC);

   


    // ── Front bonnet slope ────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.44f, 0.72f });
    m = glm::rotate(m, glm::radians(-18.f), { 1.f, 0.f, 0.f });
    m = glm::scale(m, { 0.50f, 0.13f, 0.34f });
    u("model", m);
    u("objectColor", colBody);
    u("matType", 8); u("isEmissive", 0);
    if (tBody > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tBody); u("tex0", 0); }
    glBindVertexArray(bxV); glDrawArrays(GL_TRIANGLES, 0, bxC);
    // ── Rear boot slope ───────────────────────────────────────────────────
    m= glm::translate(base, { 0.f, 0.46f, -0.72f });
    m = glm::rotate(m, glm::radians(14.f), { 1.f, 0.f, 0.f });
    m = glm::scale(m, { 0.50f, 0.11f, 0.32f });
    u("model", m);
    u("objectColor", colBody);
    u("matType", 8); u("isEmissive", 0);
    if (tBody > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tBody); u("tex0", 0); }
    glBindVertexArray(bxV); glDrawArrays(GL_TRIANGLES, 0, bxC);


    // ── Racing stripe ─────────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.624f, 0.f });
    m = glm::scale(m, { 0.07f, 0.003f, 0.96f });
    drw(bxV, bxC, m, colStripe, 0);

    // ── Roof cabin ────────────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.71f, -0.10f });
    m = glm::scale(m, { 0.44f, 0.20f, 0.52f });
    u("model", m);
    u("objectColor", colRoof);
    u("matType", 8); u("isEmissive", 0);
    if (tBody > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tBody); u("tex0", 0); }
    glBindVertexArray(bxV); glDrawArrays(GL_TRIANGLES, 0, bxC);


    // Roof cap
    m = glm::translate(base, { 0.f, 0.89f, -0.10f });
    m = glm::scale(m, { 0.40f, 0.055f, 0.46f });
    u("model", m);
    u("objectColor", colRoof * 0.88f);
    u("matType", 8); u("isEmissive", 0);
    if (tBody > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tBody); u("tex0", 0); }
    glBindVertexArray(bxV); glDrawArrays(GL_TRIANGLES, 0, bxC);
    u("objectColor", glm::vec3(1.f));
    // ── Door lines ────────────────────────────────────────────────────────
    for (float sx2 : { -0.52f, 0.52f }) {
        m = glm::translate(base, { sx2, 0.44f, 0.0f });
        m = glm::scale(m, { 0.015f, 0.14f, 0.30f });
        drw(bxV, bxC, m, colBodyDark, 0);
    }

    // ── Windshield ────────────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.73f, 0.37f });
    m = glm::rotate(m, glm::radians(-27.f), { 1.f, 0.f, 0.f });
    m = glm::scale(m, { 0.38f, 0.19f, 0.04f });
    if (tGlass > 0) drwTex(bxV, bxC, m, tGlass, 7);
    else            drw(bxV, bxC, m, colWindow, 0, !g_day);

    // ── Rear window ───────────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.73f, -0.57f });
    m = glm::rotate(m, glm::radians(22.f), { 1.f, 0.f, 0.f });
    m = glm::scale(m, { 0.38f, 0.17f, 0.04f });
    if (tGlass > 0) drwTex(bxV, bxC, m, tGlass, 7);
    else            drw(bxV, bxC, m, colWindow, 0, !g_day);

    // ── Side windows ─────────────────────────────────────────────────────
    for (float sx2 : { -0.52f, 0.52f }) {
        m = glm::translate(base, { sx2, 0.3f, -0.08f });
        m = glm::scale(m, { 0.04f, 0.17f, 0.35f });
        if (tGlass > 0) drwTex(bxV, bxC, m, tGlass, 7);
        else            drw(bxV, bxC, m, colWindow, 0, !g_day);
    }

    // ── Front bumper ─────────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.22f, 1.08f });
    m = glm::scale(m, { 0.50f, 0.09f, 0.06f });
    drw(bxV, bxC, m, colChrome, 0);

    m = glm::translate(base, { 0.f, 0.14f, 1.06f });
    m = glm::scale(m, { 0.48f, 0.04f, 0.05f });
    drw(bxV, bxC, m, colChrome * 0.82f, 0);

    // ── Rear bumper ───────────────────────────────────────────────────────
    m = glm::translate(base, { 0.f, 0.22f, -1.08f });
    m = glm::scale(m, { 0.50f, 0.09f, 0.06f });
    drw(bxV, bxC, m, colChrome, 0);

    // ── Headlights ────────────────────────────────────────────────────────
    for (float sx2 : { -0.34f, 0.34f }) {
        // Housing
        m = glm::translate(base, { sx2, 0.38f, 1.06f });
        m = glm::scale(m, { 0.10f, 0.07f, 0.04f });
        drw(bxV, bxC, m, lightsOn ? colLightOn : colLightOff, 0, lightsOn);

        // Lens
        m = glm::translate(base, { sx2, 0.38f, 1.095f });
        m = glm::scale(m, { 0.09f, 0.07f, 0.03f });
        drw(spV, spC, m, lightsOn ? colLightOn : colLightOff, 0, lightsOn);

        // Chrome ring
        m = glm::translate(base, { sx2, 0.38f, 1.078f });
        m = glm::scale(m, { 0.11f, 0.09f, 0.02f });
        drw(cyV, cyC, m, colChrome, 0);

        // Halo glow (only when on)
        if (lightsOn) {
            m = glm::translate(base, { sx2, 0.38f, 1.11f });
            m = glm::scale(m, { 0.14f, 0.11f, 0.04f });
            drw(spV, spC, m, glm::vec3(1.0f, 0.95f, 0.68f) * 0.45f, 0, true);
        }
    }

    // ── Tail lights ───────────────────────────────────────────────────────
    for (float sx2 : { -0.34f, 0.34f }) {
        m = glm::translate(base, { sx2, 0.38f, -1.06f });
        m = glm::scale(m, { 0.10f, 0.07f, 0.04f });
        drw(bxV, bxC, m, lightsOn ? colTailOn : colTailOff, 0, lightsOn);

        m = glm::translate(base, { sx2, 0.38f, -1.09f });
        m = glm::scale(m, { 0.08f, 0.06f, 0.02f });
        drw(spV, spC, m, lightsOn ? colTailOn : colTailOff * 0.7f, 0, lightsOn);
    }

    // ── Wheels ────────────────────────────────────────────────────────────
    struct WheelDef { float wx, wz; bool front; };
    WheelDef wheels[4] = {
        { -0.56f,  0.62f, true  },
        {  0.56f,  0.62f, true  },
        { -0.56f, -0.62f, false },
        {  0.56f, -0.62f, false },
    };
    for (auto& w : wheels) {
        glm::mat4 wm = glm::translate(base, { w.wx, 0.22f, w.wz });
        if (w.front)
            wm = glm::rotate(wm, steer, { 0.f, 1.f, 0.f });
        wm = glm::rotate(wm, glm::radians(90.f), { 0.f, 0.f, 1.f });
        wm = glm::rotate(wm, wheelRot, { 0.f, 0.f, 1.f });

        // Tyre
        glm::mat4 tyreM = glm::scale(wm, { 0.22f, 0.10f, 0.22f });
        u("objectColor", colTyre);
        if (tWheel > 0) drwTex(cyV, cyC, tyreM, tWheel, 8);
        else            drw(cyV, cyC, tyreM, colTyre, 0);
        u("objectColor", glm::vec3(1.f));

        // Gold rim
        glm::mat4 rimM = glm::scale(wm, { 0.14f, 0.115f, 0.14f });
        drw(cyV, cyC, rimM, colRim, 0);

        // 5 spokes
        for (int sp = 0; sp < 5; sp++) {
            float sa = sp * (2.f * PI / 5.f);
            glm::mat4 sm = glm::translate(wm, { cosf(sa) * 0.07f, 0.f, sinf(sa) * 0.07f });
            sm = glm::rotate(sm, sa, { 0.f, 0.f, 1.f });
            sm = glm::scale(sm, { 0.025f, 0.115f, 0.025f });
            drw(cyV, cyC, sm, colChrome, 0);
        }

        // Centre hub
        glm::mat4 hubM = glm::scale(wm, { 0.05f, 0.118f, 0.05f });
        drw(cyV, cyC, hubM, colChrome, 0);
    }

    // ── Antenna ───────────────────────────────────────────────────────────
    m = glm::translate(base, { 0.20f, 0.95f, -0.15f });
    m = glm::scale(m, { 0.012f, 0.14f, 0.012f });
    drw(cyV, cyC, m, colChrome, 0);
    m = glm::translate(base, { 0.20f, 1.11f, -0.15f });
    m = glm::scale(m, { 0.025f, 0.025f, 0.025f });
    drw(spV, spC, m, colChrome, 0);

    // ── Exhaust ───────────────────────────────────────────────────────────
    m = glm::translate(base, { -0.38f, 0.12f, -1.06f });
    m = glm::rotate(m, glm::radians(90.f), { 1.f, 0.f, 0.f });
    m = glm::scale(m, { 0.04f, 0.10f, 0.04f });
    drw(cyV, cyC, m, colUnder, 0);
}

// =============================================================================
// Swing Chair — ruled surface seat + rope/chain + A-frame posts
// =============================================================================
static Mesh mkRuledSeat(int uSegs = 20, int vSegs = 12)
{
    // Ruled surface: linearly interpolates between two cubic Bézier curves
    // Curve A (back edge) and Curve B (front edge) — slight bowl/sag shape
    Mesh m;

    auto curveA = [](float u) -> glm::vec3 {
        // Back edge — slightly raised ends, sagging centre
        float x = (u - 0.5f) * 1.20f;
        float y = 0.04f * (4.f * u * u - 4.f * u + 1.f); // gentle upward bow at ends
        float z = -0.30f;
        return { x, y, z };
        };
    auto curveB = [](float u) -> glm::vec3 {
        // Front edge — same bow but front of seat
        float x = (u - 0.5f) * 1.20f;
        float y = 0.04f * (4.f * u * u - 4.f * u + 1.f);
        float z = 0.30f;
        return { x, y, z };
        };

    for (int vi = 0; vi < vSegs; vi++) {
        float v0 = (float)vi / vSegs;
        float v1 = (float)(vi + 1) / vSegs;
        for (int ui = 0; ui < uSegs; ui++) {
            float u0 = (float)ui / uSegs;
            float u1 = (float)(ui + 1) / uSegs;

            // Ruled surface: P(u,v) = (1-v)*A(u) + v*B(u)
            auto ruled = [&](float u, float v) -> glm::vec3 {
                glm::vec3 a = curveA(u);
                glm::vec3 b = curveB(u);
                return a * (1.f - v) + b * v;
                };

            glm::vec3 p00 = ruled(u0, v0), p10 = ruled(u1, v0);
            glm::vec3 p01 = ruled(u0, v1), p11 = ruled(u1, v1);

            // Approximate normal from cross product of edges
            glm::vec3 du = p10 - p00;
            glm::vec3 dv = p01 - p00;
            glm::vec3 n = glm::normalize(glm::cross(du, dv));
            if (glm::length(n) < 0.001f) n = glm::vec3(0, 1, 0);

            pv(m, p00.x, p00.y, p00.z, n.x, n.y, n.z, u0, v0);
            pv(m, p10.x, p10.y, p10.z, n.x, n.y, n.z, u1, v0);
            pv(m, p11.x, p11.y, p11.z, n.x, n.y, n.z, u1, v1);

            pv(m, p00.x, p00.y, p00.z, n.x, n.y, n.z, u0, v0);
            pv(m, p11.x, p11.y, p11.z, n.x, n.y, n.z, u1, v1);
            pv(m, p01.x, p01.y, p01.z, n.x, n.y, n.z, u0, v1);
        }
    }
    return m;
}
static void drawSwingChair(float now,
    GLuint swingSeatVAO, int swingSeatCnt,
    GLuint texWood, GLuint texRope)
{
    const glm::vec3 FRAME_CENTER = { -25.f, 0.f, -40.5f };
    const float     FRAME_H = 2.8f;
    const float     FRAME_W = 1.6f;
    const float     ROPE_LEN = 1.4f;
    const float     SWING_AMP = glm::radians(28.f);
    const float     SWING_FREQ = 0.5f;

    float swingAngle = SWING_AMP * sinf(now * SWING_FREQ * 2.f * PI);

    glm::vec3 col_wood = { 0.55f, 0.35f, 0.15f };
    glm::vec3 col_rope = { 0.75f, 0.65f, 0.40f };
    glm::vec3 col_seat = { 1.00f, 0.20f, 0.60f };
    glm::vec3 col_metal = { 0.50f, 0.50f, 0.52f };

    glm::mat4 base = glm::translate(glm::mat4(1.f), FRAME_CENTER);
    glm::mat4 m;

    // ── Define seatBase ONCE, at the top, before anything uses it ─────────
    glm::mat4 seatBase = glm::translate(glm::mat4(1.f),
        FRAME_CENTER + glm::vec3(0.f, FRAME_H, 0.f));
    seatBase = glm::rotate(seatBase, swingAngle, glm::vec3(1.f, 0.f, 0.f));
    seatBase = glm::translate(seatBase, glm::vec3(0.f, -ROPE_LEN, 0.f));

    // ── A-frame legs ──────────────────────────────────────────────────────
    for (float side : { -1.f, 1.f }) {
        glm::mat4 leg = glm::translate(base, glm::vec3(side * FRAME_W, FRAME_H * 0.5f, -0.6f));
        leg = glm::rotate(leg, side * glm::radians(12.f), glm::vec3(0.f, 0.f, 1.f));
        leg = glm::scale(leg, glm::vec3(0.07f, FRAME_H * 0.52f, 0.07f));
        u("objectColor", col_wood);
        u("matType", 8); u("isEmissive", 0);
        if (texWood > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texWood); u("tex0", 0); }
        u("model", leg);
        glBindVertexArray(cyV); glDrawArrays(GL_TRIANGLES, 0, cyC);

        glm::mat4 leg2 = glm::translate(base, glm::vec3(side * FRAME_W, FRAME_H * 0.5f, 0.6f));
        leg2 = glm::rotate(leg2, side * glm::radians(12.f), glm::vec3(0.f, 0.f, 1.f));
        leg2 = glm::scale(leg2, glm::vec3(0.07f, FRAME_H * 0.52f, 0.07f));
        u("model", leg2);
        glBindVertexArray(cyV); glDrawArrays(GL_TRIANGLES, 0, cyC);

        glm::mat4 brace = glm::translate(base, glm::vec3(side * FRAME_W, FRAME_H * 0.28f, 0.f));
        brace = glm::rotate(brace, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
        brace = glm::scale(brace, glm::vec3(0.045f, 0.62f, 0.045f));
        u("model", brace);
        glBindVertexArray(cyV); glDrawArrays(GL_TRIANGLES, 0, cyC);
    }

    // ── Top horizontal bar ────────────────────────────────────────────────
    m = glm::translate(base, glm::vec3(0.f, FRAME_H, 0.f));
    m = glm::rotate(m, glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f));
    m = glm::scale(m, glm::vec3(0.08f, FRAME_W * 1.35f, 0.08f));
    u("objectColor", col_wood);
    u("matType", 8); u("isEmissive", 0);
    if (texWood > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texWood); u("tex0", 0); }
    u("model", m);
    glBindVertexArray(cyV); glDrawArrays(GL_TRIANGLES, 0, cyC);

    // Metal hook spheres
    for (float hx : { -0.55f, 0.55f }) {
        m = glm::translate(base, glm::vec3(hx, FRAME_H - 0.04f, 0.f));
        m = glm::scale(m, glm::vec3(0.06f, 0.06f, 0.06f));
        u("objectColor", col_metal);
        u("matType", 0);
        u("model", m);
        glBindVertexArray(spV); glDrawArrays(GL_TRIANGLES, 0, spC);
    }

    // ── Hanging supports — bottom locked to seatBase ──────────────────────
    for (float rx : { -0.25f, 0.25f }) {
        glm::vec3 ropeTop = FRAME_CENTER + glm::vec3(rx, FRAME_H - 0.06f, 0.f);

        // Bottom point comes from the SAME seatBase matrix the seat uses
        glm::vec4 botLocal = seatBase * glm::vec4(rx, 0.f, 0.f, 1.f);
        glm::vec3 ropeBot = glm::vec3(botLocal);

        glm::vec3 ropeMid = (ropeTop + ropeBot) * 0.5f;
        float     ropeLen = glm::length(ropeBot - ropeTop);
        glm::vec3 ropeD = glm::normalize(ropeBot - ropeTop);

        glm::mat4 rm = glm::translate(glm::mat4(1.f), ropeMid);
        glm::vec3 up = { 0.f, 1.f, 0.f };
        glm::vec3 ax = glm::cross(up, ropeD);
        if (glm::length(ax) > 0.001f) {
            float ang = acosf(glm::clamp(glm::dot(up, ropeD), -1.f, 1.f));
            rm = glm::rotate(rm, ang, glm::normalize(ax));
        }
        rm = glm::scale(rm, glm::vec3(0.025f, ropeLen * 0.5f, 0.025f));
        u("objectColor", col_rope);
        u("matType", 8); u("isEmissive", 0);
        if (texRope > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texRope); u("tex0", 0); }
        else if (texWood > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texWood); u("tex0", 0); }
        u("model", rm);
        glBindVertexArray(cyV); glDrawArrays(GL_TRIANGLES, 0, cyC);
    }

    // ── Seat (ruled surface VAO) ──────────────────────────────────────────
    glm::mat4 seatM = glm::scale(seatBase, glm::vec3(1.f, 1.f, 1.f));
    u("objectColor", col_seat);
    u("matType", 8); u("isEmissive", 0);
    if (texWood > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texWood); u("tex0", 0); }
    u("model", seatM);
    glBindVertexArray(swingSeatVAO);
    glDrawArrays(GL_TRIANGLES, 0, swingSeatCnt);

    // Seat underside
    glm::mat4 seatBox = glm::translate(seatBase, glm::vec3(0.f, -0.035f, 0.f));
    seatBox = glm::scale(seatBox, glm::vec3(0.60f, 0.035f, 0.30f));
    u("objectColor", col_seat * 0.80f);
    u("matType", 8);
    if (texWood > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texWood); u("tex0", 0); }
    u("model", seatBox);
    glBindVertexArray(bxV); glDrawArrays(GL_TRIANGLES, 0, bxC);

    // Seat slats
    for (int sl = 0; sl < 3; sl++) {
        float sz = -0.18f + sl * 0.18f;
        glm::mat4 slat = glm::translate(seatBase, glm::vec3(0.f, 0.012f, sz));
        slat = glm::scale(slat, glm::vec3(0.58f, 0.012f, 0.055f));
        u("objectColor", col_seat * 0.90f);
        u("matType", 8);
        if (texWood > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texWood); u("tex0", 0); }
        u("model", slat);
        glBindVertexArray(bxV); glDrawArrays(GL_TRIANGLES, 0, bxC);
    }

    // Back support
    glm::mat4 back = glm::translate(seatBase, glm::vec3(0.f, 0.28f, -0.30f));
    back = glm::rotate(back, glm::radians(-15.f), glm::vec3(1.f, 0.f, 0.f));
    back = glm::scale(back, glm::vec3(0.58f, 0.30f, 0.04f));
    u("objectColor", col_seat);
    u("matType", 8);
    if (texWood > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texWood); u("tex0", 0); }
    u("model", back);
    glBindVertexArray(bxV); glDrawArrays(GL_TRIANGLES, 0, bxC);

    // Footrest
    glm::mat4 foot = glm::translate(seatBase, glm::vec3(0.f, -0.22f, 0.32f));
    foot = glm::scale(foot, glm::vec3(0.50f, 0.022f, 0.08f));
    u("objectColor", col_seat * 0.85f);
    u("matType", 8);
    if (texWood > 0) { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texWood); u("tex0", 0); }
    u("model", foot);
    glBindVertexArray(bxV); glDrawArrays(GL_TRIANGLES, 0, bxC);

    // Ground shadow disc
    glm::mat4 ao = glm::translate(glm::mat4(1.f), FRAME_CENTER + glm::vec3(0.f, 0.01f, 0.3f * sinf(swingAngle * 2.f)));
    ao = glm::scale(ao, glm::vec3(1.4f, 0.008f, 0.6f));
    u("objectColor", glm::vec3(0.04f, 0.07f, 0.03f));
    u("matType", 0); u("isEmissive", 0);
    u("model", ao);
    glBindVertexArray(cyV); glDrawArrays(GL_TRIANGLES, 0, cyC);

    u("objectColor", glm::vec3(1.f));
}
static void drawMushroomCluster(float now,
    GLuint texMushroom,
    GLuint texGrass,
    GLuint mushroomCapVAO, int mushroomCapCnt,
    GLuint mushroomStemVAO, int mushroomStemCnt)
{
    // Pre-mixed colours (no lambda in static array)
    glm::vec3 stemCream = { 1.00f, 0.90f, 0.60f };
    glm::vec3 stemTan = { 0.78f, 0.68f, 0.52f };

    // cap colour mixed with grass bg manually
    glm::vec3 capRed = { 0.74f, 0.20f, 0.11f };   // red   + 15% grass
    glm::vec3 capOrange = { 0.80f, 0.43f, 0.10f };   // orange+ 20% grass
    glm::vec3 capBrown = { 0.65f, 0.65f, 1.00f };   // brown + 25% grass
    glm::vec3 capPurple = { 0.44f, 0.22f, 0.58f };   // purple+ 18% grass
    glm::vec3 capRed2 = { 0.90f, 0.40f, 0.50f };   // red   + 30% grass
    glm::vec3 capOrange2 = { 1.00f, 0.85f, 0.00f };   // orange+ 22% grass

    struct MushDef {
        glm::vec3 pos;
        float     height;
        float     tiltX, tiltZ;
        glm::vec3 stemCol, capCol;
        int       variant;
    };

    MushDef mushrooms[] = {
        { { 19.55f, 0.0f, -5.6f }, 3.80f,  0.00f,  0.00f, stemCream, capRed,     0 },
        { { 18.20f, 0.0f, -5.9f }, 3.10f,  0.00f, -0.18f, stemTan,   capOrange,  1 },
        { { 17.00f, 0.f, -6.3f }, 2.60f,  -0.08f,  0.00f, stemCream, capBrown,   2 },
        { { 15.70f, 0.f, -6.5f }, 2.10f, -0.05f,  0.22f, stemTan,   capPurple,  0 },
        { { 16.80f, 0.f, -5.4f }, 1.60f,  0.0f, -0.08f, stemCream, capRed2,    1 },
        { { 19.00f, 0.f, -5.2f }, 1.30f,  0.00f,  0.15f, stemTan,   capOrange2, 2 },
    };

    int count = (int)(sizeof(mushrooms) / sizeof(mushrooms[0]));

    for (int i = 0; i < count; i++)
    {
        MushDef& md = mushrooms[i];

        float bobY = sinf(now * 1.1f + i * 1.3f) * 0.025f;

        float stemH = md.height * 0.52f;
        float capH = md.height * 0.48f;
        float stemR = md.height * 0.095f;
        float capBrimR = md.height * (md.variant == 1 ? 0.28f
            : md.variant == 2 ? 0.55f
            : 0.40f);

        glm::mat4 base = glm::translate(glm::mat4(1.f), md.pos + glm::vec3(0.f, bobY, 0.f));
        base = glm::rotate(base, md.tiltZ, glm::vec3(0.f, 0.f, 1.f));
        base = glm::rotate(base, md.tiltX, glm::vec3(1.f, 0.f, 0.f));

        // ── Stem ──────────────────────────────────────────────────────────
        glm::mat4 sm = glm::translate(base, glm::vec3(0.f, 0.f, 0.f));
        sm = glm::scale(sm, glm::vec3(stemR, stemH, stemR));
        u("objectColor", md.stemCol);
        u("matType", 8);
        u("isEmissive", 0);
        if (texMushroom > 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texMushroom);
            u("tex0", 0);
        }
        u("model", sm);
        glBindVertexArray(mushroomStemVAO);
        glDrawArrays(GL_TRIANGLES, 0, mushroomStemCnt);

        // Stem ring / skirt
        float ringY = stemH * 0.68f;
        float ringR = stemR * 1.7f;
        glm::mat4 ring = glm::translate(base, glm::vec3(0.f, ringY, 0.f));
        ring = glm::scale(ring, glm::vec3(ringR, ringR * 0.12f, ringR));
        u("objectColor", md.stemCol * 0.85f);
        u("matType", 8);
        if (texMushroom > 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texMushroom);
            u("tex0", 0);
        }
        u("model", ring);
        glBindVertexArray(cyV); glDrawArrays(GL_TRIANGLES, 0, cyC);

        // ── Cap ───────────────────────────────────────────────────────────
        glm::mat4 cm = glm::translate(base, glm::vec3(0.f, stemH, 0.f));
        cm = glm::scale(cm, glm::vec3(capBrimR, capH, capBrimR));
        u("objectColor", md.capCol);
        u("matType", 8);
        u("isEmissive", 0);
        if (texMushroom > 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texMushroom);
            u("tex0", 0);
        }
        u("model", cm);
        glBindVertexArray(mushroomCapVAO);
        glDrawArrays(GL_TRIANGLES, 0, mushroomCapCnt);

        // ── Spots ─────────────────────────────────────────────────────────
        int nSpots = 4 + md.variant * 2;
        for (int s = 0; s < nSpots; s++) {
            float sa = s * (2.f * PI / nSpots) + md.variant * 0.42f;
            float sUp = 0.55f + 0.20f * ((s % 3) * 0.3f);
            float sr = capBrimR * (0.85f - sUp * 0.55f);
            glm::vec3 spotPos = {
                cosf(sa) * sr * 0.72f,
                stemH + capH * sUp,
                sinf(sa) * sr * 0.72f
            };
            glm::mat4 spot = glm::translate(base, spotPos);
            float spotR = capBrimR * 0.08f;
            spot = glm::scale(spot, glm::vec3(spotR, spotR * 0.4f, spotR));
            u("objectColor", glm::vec3(1.0f, 0.97f, 0.93f));
            u("matType", 0);
            int emissive = g_day ? 0 : 1;
            u("isEmissive", emissive);
            u("model", spot);
            glBindVertexArray(spV); glDrawArrays(GL_TRIANGLES, 0, spC);
        }

        // ── Ground shadow disc ─────────────────────────────────────────────
        glm::mat4 ao = glm::translate(base, glm::vec3(0.f, 0.01f, 0.f));
        ao = glm::scale(ao, glm::vec3(capBrimR * 0.9f, 0.008f, capBrimR * 0.9f));
        u("objectColor", glm::vec3(0.05f, 0.08f, 0.03f));
        u("matType", 0);
        u("isEmissive", 0);
        u("model", ao);
        glBindVertexArray(cyV); glDrawArrays(GL_TRIANGLES, 0, cyC);

        u("objectColor", glm::vec3(1.f));
    }
}



static void drawCampfire(float now)
{
    glm::vec3 fp = FIRE_POS;

    // ── Logs (two crossed cylinders on ground) ────────────────────────
    glm::vec3 logCol = { 0.28f, 0.16f, 0.08f };
    for (float ang : { 35.f, -35.f }) {
        glm::mat4 lm = glm::translate(glm::mat4(1), fp + glm::vec3(0.f, 0.055f, 0.f));
        lm = glm::rotate(lm, glm::radians(ang), { 0.f, 1.f, 0.f });
        lm = glm::rotate(lm, glm::radians(90.f), { 0.f, 0.f, 1.f });
        lm = glm::scale(lm, { 0.065f, 0.52f, 0.065f });
        u("model", lm); u("objectColor", logCol);
        u("matType", 0); u("isEmissive", 0);
        glBindVertexArray(cyV); glDrawArrays(GL_TRIANGLES, 0, cyC);
    }
    // Charred log ends (dark embers)
    for (float sx : { -0.50f, 0.50f }) {
        glm::mat4 em = glm::translate(glm::mat4(1), fp + glm::vec3(sx * cosf(glm::radians(35.f)), 0.055f, sx * sinf(glm::radians(35.f))));
        em = glm::scale(em, { 0.07f, 0.07f, 0.07f });
        u("model", em); u("objectColor", glm::vec3(0.18f, 0.09f, 0.04f));
        u("matType", 0); u("isEmissive", 0);
        glBindVertexArray(cyV); glDrawArrays(GL_TRIANGLES, 0, cyC);
    }

    // ── Ember bed (glowing orange disc on ground) ─────────────────────
    float emberPulse = 0.65f + 0.35f * sinf(now * 4.1f);
    glm::mat4 eb = glm::translate(glm::mat4(1), fp + glm::vec3(0.f, 0.012f, 0.f));
    eb = glm::scale(eb, { 0.28f, 0.01f, 0.28f });
    u("model", eb);
    u("objectColor", glm::vec3(1.0f, 0.38f + 0.12f * emberPulse, 0.0f));
    u("matType", 0); u("isEmissive", 1);
    glBindVertexArray(cyV); glDrawArrays(GL_TRIANGLES, 0, cyC);

    // ── Flame layers — stacked cones, each flickering independently ───
    struct FlameLayer {
        float yOff, scaleXZ, scaleY, phase, speed;
        glm::vec3 col;
    };
    static const FlameLayer flames[] = {
        // outer base — wide deep orange
        { 0.05f, 0.22f, 0.38f, 0.0f, 3.1f, { 1.0f, 0.28f, 0.02f } },
        // mid — orange
        { 0.10f, 0.16f, 0.52f, 1.1f, 4.3f, { 1.0f, 0.48f, 0.04f } },
        // inner — yellow-orange
        { 0.14f, 0.10f, 0.62f, 2.3f, 5.7f, { 1.0f, 0.72f, 0.10f } },
        // core — bright yellow
        { 0.18f, 0.05f, 0.55f, 0.7f, 7.2f, { 1.0f, 0.95f, 0.30f } },
        // tip wisp
        { 0.22f, 0.03f, 0.35f, 1.8f, 9.0f, { 1.0f, 1.00f, 0.60f } },
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (auto& fl : flames) {
        float flicker = 0.75f + 0.25f * sinf(now * fl.speed + fl.phase)
            + 0.12f * sinf(now * fl.speed * 1.7f + fl.phase + 0.5f);
        float sway = sinf(now * 1.3f + fl.phase) * 0.04f;

        glm::mat4 fm = glm::translate(glm::mat4(1),
            fp + glm::vec3(sway, fl.yOff, sway * 0.5f));
        fm = glm::scale(fm, {
            fl.scaleXZ * flicker,
            fl.scaleY * flicker,
            fl.scaleXZ * flicker
            });
        u("model", fm);
        u("objectColor", fl.col);
        u("matType", 0); u("isEmissive", 1);
        glBindVertexArray(cnV); glDrawArrays(GL_TRIANGLES, 0, cnC);
    }

    // ── Spark particles — tiny bright spheres shooting upward ─────────
    srand(13);
    for (int i = 0; i < 12; i++) {
        float t = fmodf(now * (0.6f + i * 0.07f) + i * 0.41f, 1.0f);
        float age = t;                        // 0=just spawned, 1=faded
        if (age > 0.75f) continue;           // cull old sparks

        float sx = ((float)rand() / RAND_MAX - 0.5f) * 0.18f;
        float sz = ((float)rand() / RAND_MAX - 0.5f) * 0.18f;
        float sy = 0.12f + age * 1.40f;    // rises over life
        float fade = 1.0f - (age / 0.75f);

        glm::vec3 sparkCol = (i % 3 == 0)
            ? glm::vec3(1.0f, 0.95f, 0.40f)   // yellow
            : (i % 3 == 1)
            ? glm::vec3(1.0f, 0.55f, 0.10f)   // orange
            : glm::vec3(1.0f, 0.25f, 0.05f);  // deep red

        glm::mat4 sm = glm::translate(glm::mat4(1),
            fp + glm::vec3(sx, sy, sz));
        sm = glm::scale(sm, glm::vec3(0.022f * fade));
        u("model", sm);
        u("objectColor", sparkCol * fade);
        u("matType", 0); u("isEmissive", 1);
        glBindVertexArray(spV); glDrawArrays(GL_TRIANGLES, 0, spC);
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);

    

    glDisable(GL_BLEND);
    u("objectColor", glm::vec3(1.f));
    u("isEmissive", 0);
}

static void drawMoonAndStars(float now)
{
    if (g_day) return;

    // ── STARS ───────────────────────────────────────────────────────────────
    {
        glm::mat4 sm = glm::translate(glm::mat4(1), cam.pos);
        u("model", sm);
        u("objectColor", glm::vec3(1.f, 1.f, 1.f));
        u("matType", 0);        // plain unlit
        u("isEmissive", 1);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);

        glBindVertexArray(g_starVAO);
        // Draw each star as a GL_POINT — shader sizes it via gl_PointSize
        for (int i = 0; i < g_starCnt; i++) {
            // Per-star twinkle: modulate brightness with time + star index
            float phase = (float)i * 0.739f;
            float twinkle = 0.65f + 0.35f * sinf(now * (2.2f + fmodf(phase, 3.1f)) + phase);
            float bright = twinkle; // shader ignores objectColor for points anyway

            // Re-upload brightness as objectColor.r — cheapest approach
           glUniform3f(glGetUniformLocation(g_prog, "objectColor"), bright, bright, bright * 0.52f);

            // Set point size based on precomputed brightness stored in VBO uv.s
            // We do this by uploading a uniform; shader uses it for gl_PointSize
            glUniform1f(glGetUniformLocation(g_prog, "u_starBright"), bright);
            glDrawArrays(GL_POINTS, i, 1);
        }

        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
        u("objectColor", glm::vec3(1.f));
    }

    // ── MOON ────────────────────────────────────────────────────────────────
    {
        float elev = glm::radians(18.f);
        float moonR = 160.f;
        glm::vec3 moonPos = cam.pos + glm::vec3(
            moonR * cosf(elev) * cosf(g_moonAngle),
            moonR * sinf(elev),
            moonR * cosf(elev) * sinf(g_moonAngle)
        );

        // --- outer soft halo (additive, no depth write) ---
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        glm::mat4 h1 = glm::translate(glm::mat4(1), moonPos);
        h1 = glm::scale(h1, glm::vec3(6.0f));
        drw(spV, spC, h1, glm::vec3(0.50f, 0.58f, 0.80f) * 0.08f, 0, true);

        glm::mat4 h2 = glm::translate(glm::mat4(1), moonPos);
        h2 = glm::scale(h2, glm::vec3(4.5f));
        drw(spV, spC, h2, glm::vec3(0.60f, 0.70f, 0.92f) * 0.14f, 0, true);

        glm::mat4 h3 = glm::translate(glm::mat4(1), moonPos);
        h3 = glm::scale(h3, glm::vec3(3.2f));
        drw(spV, spC, h3, glm::vec3(0.75f, 0.82f, 1.00f) * 0.22f, 0, true);

        glm::mat4 h4 = glm::translate(glm::mat4(1), moonPos);
        h4 = glm::scale(h4, glm::vec3(2.2f));
        drw(spV, spC, h4, glm::vec3(0.88f, 0.92f, 1.00f) * 0.35f, 0, true);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);

        // --- moon sphere ---
        float moonRadius = 2.5f;
        glm::mat4 mm = glm::translate(glm::mat4(1), moonPos);
        mm = glm::rotate(mm, glm::radians(15.f), glm::vec3(0.f, 0.f, 1.f));
        mm = glm::rotate(mm, now * 0.012f, glm::vec3(0.05f, 1.f, 0.05f));
        mm = glm::scale(mm, glm::vec3(moonRadius));

        u("model", mm);
        u("objectColor", glm::vec3(0.92f, 0.95f, 1.00f));
        u("matType", 7);       // textured
        u("isEmissive", 1);    // self-lit — not affected by scene lights

        // primary texture
        if (texMoon > 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texMoon);
            u("tex0", 0);
        }

        // second texture blended on top (uses tex1 if you add it to your shader,
        // otherwise just binding it here does nothing — safe to leave)
        if (texMoon2 > 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, texMoon2);
            glUniform1i(glGetUniformLocation(g_prog, "tex1"), 1);
            glUniform1f(glGetUniformLocation(g_prog, "u_moonBlend"), 0.38f);
        }
        else {
            glUniform1f(glGetUniformLocation(g_prog, "u_moonBlend"), 0.f);
        }

        glBindVertexArray(spV);
        glDrawArrays(GL_TRIANGLES, 0, spC);

        // cleanup tex1
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        u("objectColor", glm::vec3(1.f));
    }
}
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* win = glfwCreateWindow(SCR_W, SCR_H,
        "3D Toy House",
        nullptr, nullptr);

    if (!win) { std::cerr << "Window failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    // Mouse callbacks are registered but empty (keyboard-only mode)

    glfwSetKeyCallback(win, cbKey);

    glfwSetFramebufferSizeCallback(win, [](GLFWwindow*, int w, int h) {
        glViewport(0, 0, w, h);
        });

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD failed\n"; return -1; }
    glViewport(0, 0, SCR_W, SCR_H);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_PROGRAM_POINT_SIZE);


    camera.updateCameraVectors();

    g_prog = mkProg("vertex.vert", "fragment.frag");
    { auto m = mkBox();  bxV = mkVAO(m, bxC); }
    { auto m = mkCyl();  cyV = mkVAO(m, cyC); }
    { auto m = mkSph();  spV = mkVAO(m, spC); }
    { auto m = mkCone(); cnV = mkVAO(m, cnC); }
    { auto m = mkRoof(); rfV = mkVAO(m, rfC); }
    { auto m = mkStars(); stV = mkVAO(m, stC); }
    { auto m = mkSkyDome(); sdV = mkVAO(m, sdC); }
    // ── Mushroom VAOs (cubic-spline lathed shapes) ────────────────────────────
// CAP profile: Catmull-Rom knots define radius from bottom-edge to tip
//   knot 0 = brim (widest), knots taper to 0 at tip — gives classic dome
    static const std::vector<float> capProfile = { 0.00f, 0.92f, 1.00f, 0.96f, 0.82f, 0.58f, 0.30f, 0.10f, 0.00f };
    // STEM profile: slight bulge at base, narrows mid, tiny flare at top
    static const std::vector<float> stemProfile = { 0.88f, 0.70f, 0.62f, 0.60f, 0.62f, 0.68f, 0.72f };

    GLuint mushroomCapVAO, mushroomStemVAO;
    int    mushroomCapCnt = 0, mushroomStemCnt = 0;
    {
        auto cm = mkSplineLatheMesh(capProfile, 1.0f, 40, 48);
        mushroomCapCnt = (int)cm.size() / 8;
        glGenVertexArrays(1, &mushroomCapVAO);
        GLuint vb; glGenBuffers(1, &vb);
        glBindVertexArray(mushroomCapVAO);
        glBindBuffer(GL_ARRAY_BUFFER, vb);
        glBufferData(GL_ARRAY_BUFFER, cm.size() * sizeof(float), cm.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);               glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    }
    {
        auto sm = mkSplineLatheMesh(stemProfile, 1.0f, 24, 24);
        mushroomStemCnt = (int)sm.size() / 8;
        glGenVertexArrays(1, &mushroomStemVAO);
        GLuint vb; glGenBuffers(1, &vb);
        glBindVertexArray(mushroomStemVAO);
        glBindBuffer(GL_ARRAY_BUFFER, vb);
        glBufferData(GL_ARRAY_BUFFER, sm.size() * sizeof(float), sm.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);               glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    }
    // ── Swing seat VAO (ruled surface) ───────────────────────────────────────
    GLuint swingSeatVAO;
    int    swingSeatCnt = 0;
    {
        auto sm = mkRuledSeat(20, 12);
        swingSeatCnt = (int)sm.size() / 8;
        GLuint vb;
        glGenVertexArrays(1, &swingSeatVAO);
        glGenBuffers(1, &vb);
        glBindVertexArray(swingSeatVAO);
        glBindBuffer(GL_ARRAY_BUFFER, vb);
        glBufferData(GL_ARRAY_BUFFER, sm.size() * sizeof(float), sm.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    }

  
    // ?? Load Digitized Image Texture ??????????????????????????????????????
    texWall = loadTexture("wall.jpg");

    texMoon = loadTexture("moon.jpg");    // greyscale or colour crater map
    
    // optional second crater/detail layer
    texRoof = loadTexture("roof.jpg");
    texcar = loadTexture("car.jpg");
    texStone1 = loadTexture("road.jpg");
    texStone = loadTexture("st.jpg");
    texStone2 = loadTexture("tt.jpg");
    texStone3 = loadTexture("val.jpg");
    texPlanks = loadTexture("orng.jpg"); // using known wood texture due to missing 191944.png

    // Disney house wall texture: soft plaster/paint look
    texHouse = loadTexture("wall.jpg");
    if (texHouse == 0) texHouse = loadTexture("wall.jpg");
    if (texHouse == 0) texHouse = texWall;  // fallback to existing wall texture

    
  
    texTreeBark = loadTexture("tree.jpg");
    if (texTreeBark == 0) texTreeBark = loadTexture("tree.jpg");
    if (texTreeBark == 0) texTreeBark = loadTexture("");
    
    texLeaf = loadTexture("leafs.jpg");
    texLeaf2 = loadTexture("leafs.jpg");

    // === ROOSTER TEXTURE ===
// Place your rooster image (e.g. rooster.png or rooster.jpg) in the SAME folder as your .exe
    GLuint texRooster = loadTexture("roster.jpg");  // <-- PUT YOUR IMAGE FILE NAME HERE 
    GLuint texMushroom = loadTexture("mashroom1.jpg");
   texHorizon = loadTexture("sky_horizon.jpg");
    glBindTexture(GL_TEXTURE_2D, texHorizon);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // ?? Grass ground texture (????? — house wall ?? ???? ????? ??) ??????
    texGrass = loadTexture("grass.jpg");
    // fallback: ??? grass.png ?? ???? ????? sample_texture.png
    if (texGrass == 0 || texGrass == (GLuint)-1) texGrass = 0;

    texFloor = loadTexture("floor.jpg");
    if (texFloor == 0) texFloor = texWood;

    // Default sample for other usage
    GLuint myTex = loadTexture("sample_texture.png");
    // grass ground ? ????? texture ?? ????? procedural matType=1 use ???
    if (texGrass == 0) texGrass = myTex;

    glUseProgram(g_prog);
    u("tex0", 0);



    // Keyboard-only mode: cursor stays visible, input always active
    camera.MovementSpeed = 5.5f;   // match original cam.speed

    // Initialise firefly orbits
    initFireflies();
    initStars();

    float prevT = (float)glfwGetTime();

    while (!glfwWindowShouldClose(win))
    {
        float now = (float)glfwGetTime();
        float dt = now - prevT; prevT = now;

        // ---- Keyboard movement (BasicCamera) ----
        processInput(win, dt);

        g_shutterSwing += ((g_shutterTargetClosed ? 1.f : 0.f) - g_shutterSwing) * 3.f * dt;
        g_houseDoorFactor += ((g_houseDoorOpen ? 1.f : 0.f) - g_houseDoorFactor) * 3.5f * dt;
        if (g_ceilingFanOn) g_fanBladeAngle += dt * 10.f;
        g_ufoSpinAngle += dt * 1.4f;  // UFO lights spin
        if (g_orbitHouse && !g_birdView) {
            g_orbitAngle += dt * 0.65f;
            float h = glm::clamp(cam.pos.y, 1.4f, 10.f);
            glm::vec3 t = g_orbitTarget;
            cam.pos.x = t.x + cosf(g_orbitAngle) * g_orbitRadius;
            cam.pos.z = t.z + sinf(g_orbitAngle) * g_orbitRadius;
            cam.pos.y = h;
            glm::vec3 aim = t + glm::vec3(0.f, 1.2f, 0.f);
            cam.front = glm::normalize(aim - cam.pos);
        }
        // ── Egg physics ──────────────────────────────────────────────────
       

        const float ROOSTER_WALK_SPD = 0.9f;   // slow walk speed

        if (g_roosterWalking) {
            if (!g_roosterOnGround) {
                // Advance parabolic arc timer
                g_roosterParabT += dt / g_roosterParabDur;

                if (g_roosterParabT >= 1.0f) {
                    // Arc complete — snap to landing spot
                    g_roosterParabT = 1.0f;
                    g_roosterOnGround = true;
                    g_roosterWalkPos = ROOSTER_LAND;
                }
                else {
                    float t = g_roosterParabT;

                    // Quadratic Bézier arc:
                    //   P0 = ROOSTER_START (ridge)
                    //   P1 = control point — forward and slightly higher for a natural lob
                    //   P2 = ROOSTER_LAND  (road)
                    glm::vec3 P0 = ROOSTER_START;
                    glm::vec3 P1 = {
                        0.30f,          // same X — falls straight ahead
                        9.80f,          // peak height (above ridge for a nice arc)
                        0.5f          // midway Z toward landing
                    };
                    glm::vec3 P2 = ROOSTER_LAND;

                    // B(t) = (1-t)^2 * P0  +  2(1-t)t * P1  +  t^2 * P2
                    float u = 1.f - t;
                    g_roosterWalkPos = u * u * P0 + 2.f * u * t * P1 + t * t * P2;
                }
            }
            else {
                // Slow walk forward along road toward player (+Z)
                g_roosterWalkPos.z += ROOSTER_WALK_SPD * dt;

                // Loop back to landing spot when it reaches the player
                if (g_roosterWalkPos.z > 7.0f)
                    g_roosterWalkPos.z = ROOSTER_LAND.z;
            }
        }
        g_moonAngle += dt * (2.f * PI / 300.f);
        // ── Toy Car Controls: P=forward, LAlt=backward, LCtrl=turn left, RCtrl=turn right ──
        const float CAR_ACCEL = 4.0f;
        const float CAR_MAX_SPD = 6.0f;
        const float CAR_FRICTION = 3.0f;
        const float CAR_TURN_SPD = 90.0f;   // degrees per second

        bool fwd = glfwGetKey(win, GLFW_KEY_P) == GLFW_PRESS;
        bool bwd = glfwGetKey(win, GLFW_KEY_LEFT_ALT) == GLFW_PRESS;
        bool turnL = glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
        bool turnR = glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

        // Speed
        if (fwd) {
            g_carMovingBack = false;
            g_carSpeed += CAR_ACCEL * dt;
            if (g_carSpeed > CAR_MAX_SPD) g_carSpeed = CAR_MAX_SPD;
        }
        else if (bwd) {
            g_carMovingBack = true;
            g_carSpeed += CAR_ACCEL * dt;
            if (g_carSpeed > CAR_MAX_SPD) g_carSpeed = CAR_MAX_SPD;
        }
        else {
            g_carSpeed -= CAR_FRICTION * dt;
            if (g_carSpeed < 0.f) g_carSpeed = 0.f;
        }

        // Rotation — only ONE variable g_carYaw, in degrees, used everywhere
        if (turnL) g_carYaw -= CAR_TURN_SPD * dt;
        if (turnR) g_carYaw += CAR_TURN_SPD * dt;

        // Movement uses g_carYaw (degrees → radians), NOT g_carAngle
        float yawRad = glm::radians(g_carYaw);
        float dir = g_carMovingBack ? -1.f : 1.f;
        if (g_carSpeed > 0.f) {
            g_carPos.x -= sinf(yawRad) * g_carSpeed * dir * dt;
            g_carPos.z -= cosf(yawRad) * g_carSpeed * dir * dt;
        }

        // Wheel spin
        g_wheelSpin += g_carSpeed * dir * dt * 3.5f;
        const float ROCKET_MAX_H = 28.f;
        const float ROCKET_SPEED = 4.5f;
        const float ROCKET_SPIN_SPD = 4.5f;  // radians per second while moving

        if (g_rocketUp) g_rocketTargetY = ROCKET_MAX_H;

        float diff = g_rocketTargetY - g_rocketY;
        float step = ROCKET_SPEED * dt;

        bool isMoving = fabsf(diff) > 0.01f;

        if (fabsf(diff) <= step)
            g_rocketY = g_rocketTargetY;
        else
            g_rocketY += (diff > 0 ? 1.f : -1.f) * step;

        // Spin only while rocket is actually moving
        if (isMoving)
            g_rocketSpin += ROCKET_SPIN_SPD * dt;

        glm::vec3 skyDay{ 0.70f, 0.82f, 0.95f };    // Lighter, hazier sky
        glm::vec3 skyNight{ 0.05f, 0.06f, 0.15f };  // Less pitch black, more night-mist
        glm::vec3 skyRainD{ 0.52f, 0.56f, 0.62f };
        glm::vec3 skyRainN{ 0.04f, 0.06f, 0.12f };
        float df = g_day ? 1.f : 0.f;
        glm::vec3 skyC = g_rain ? (g_day ? skyRainD : skyRainN) : (g_day ? skyDay : skyNight);


        glClearColor(skyC.r, skyC.g, skyC.b, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = g_birdView
            ? glm::lookAt(glm::vec3(0.f, 58.f, 0.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f))
            : camera.GetViewMatrix();
        int fbW, fbH;
        glfwGetFramebufferSize(win, &fbW, &fbH);
        glm::mat4 proj = glm::perspective(glm::radians(g_fov), (float)fbW / (float)fbH, 0.05f, 280.f);

        // Firefly flicker: soft sinusoidal brightness pulse per-firefly (shared base)
        float ffFlicker = 0.7f + 0.3f * sinf(now * 4.5f) + 0.15f * sinf(now * 9.3f);
        // Use first firefly as the representative point-light source for the shader
        glm::vec3 fireLPos = g_fireflies.empty()
            ? glm::vec3(-20.f, 1.5f, -37.f)
            : fireflyPos(g_fireflies[0], now);

        glUseProgram(g_prog);
        u("view", view);
        u("projection", proj);
        u("viewPos", cam.pos);
        u("dayFactor", df);
        u("isRaining", (int)g_rain);
        u("time", now);
        u("skyColor", skyC);
        float thunderFlash = 0.0f;
        if (g_thunderTimer > 0) thunderFlash = (g_thunderTimer / 0.6f);
        u("thunderEffect", thunderFlash);
        u("lightDirOn", g_lightDir ? 1.f : 0.f);
        u("lightPointOn", g_lightPoint ? 1.f : 0.f);
        u("lightSpotOn", g_lightSpot ? 1.f : 0.f);
        u("lightAmbOn", g_lightAmb ? 1.f : 0.f);
        u("lightDiffOn", g_lightDiff ? 1.f : 0.f);
        u("lightSpecOn", g_lightSpec ? 1.f : 0.f);

        
       
        // Sun slightly behind the player to illuminate fronts of objects
        u("sunDir", glm::normalize(glm::vec3(-0.35f, 1.0f, 0.55f)));
        u("sunColor", glm::vec3(1.0f, 0.96f, 0.82f));
        u("sunIntensity", 1.80f);

        u("moonDir", glm::normalize(glm::vec3(-0.35f, 1.0f, 0.55f)));
        u("moonColor", glm::vec3(0.55f, 0.64f, 0.92f));

        float fireFlicker = 0.80f + 0.20f * sinf(now * 5.2f)
            + 0.10f * sinf(now * 8.7f + 1.1f)
            + 0.05f * sinf(now * 13.f + 2.3f);

        u("firePos", FIRE_POS + glm::vec3(0.f, 0.4f, 0.f));
        u("fireColor", glm::vec3(1.0f, 0.52f, 0.08f));   // warm orange-yellow
        u("fireIntensity", fireFlicker * 9.5f);
        u("firePos", fireLPos);
        u("fireColor", glm::vec3(0.80f, 1.00f, 0.25f));   // firefly yellow-green
        u("fireIntensity", g_day ? 0.0f : ffFlicker * 5.5f);
        // Car headlight uniforms
        float   carRad2 = glm::radians(g_carYaw);
        glm::vec3 carFwd = { sinf(carRad2), 0.f, cosf(carRad2) };
        glm::vec3 carHLPos = g_carPos + carFwd * 1.1f + glm::vec3(0.f, 0.38f, 0.f);
        glm::vec3 carHLDir = glm::normalize(carFwd + glm::vec3(0.f, -0.12f, 0.f));

        u("carHeadlightPos", carHLPos);
        u("carHeadlightDir", carHLDir);
        u("carHeadlightOn", g_carLightsOn ? 1 : 0);
        u("carHeadlightColor", glm::vec3(1.0f, 0.97f, 0.88f));
        u("carHeadlightInner", cosf(glm::radians(12.f)));
        u("carHeadlightOuter", cosf(glm::radians(26.f)));

        // ?? Drawing Horizon ??
        drawSkyDome();
        drawMoonAndStars(now);
        // ?? Infinite Grass Ground ??????
        {
            // Center the grass ground on the camera's X and Z to make it endless, locking its texture in the shader
            glm::mat4 m = glm::translate(glm::mat4(1), { cam.pos.x, 0.0f, cam.pos.z });
            m = glm::scale(m, { 450.f, 0.01f, 450.f });
            drwTex(bxV, bxC, m, texGrass, 1);
        }

        // Smooth Bézier path to the Disney house entrance.
        // p0 = player start area, p3 = house porch, p1/p2 = gentle S-curve controls.
        drawBezierPath(
            { 0.0f, 0.f,  7.0f },   // p0 – near camera spawn
            { -1.2f, 0.f,  0.0f },  // p1 – pull left
            { 1.0f, 0.f, -9.0f },  // p2 – pull right
            { 0.0f, 0.f, -14.5f },  // p3 – house porch
            2.6f, 32);               // width, segments

        drawToyRocket(g_rocketBasePos, now, g_rocketY, g_rocketSpin);
        drawToyCar(g_carPos, g_carYaw, g_carWheelRot, g_carSteer, g_carLightsOn, now);
        // Disney two-storied house in the middle.
        drawDisneyHouse({ 0.f, 0.f, -18.f }, glm::radians(0.f), 1.f, false, { 0.98f, 0.96f, 0.90f });
        drawMushroomCluster(now, texMushroom, texGrass,
            mushroomCapVAO, mushroomCapCnt,
            mushroomStemVAO, mushroomStemCnt);
        // After processInput, before drawing:
        

        // In your draw section, after drawGlowingRock:
        drawCampfire(now);
        drawSwingChair(now, swingSeatVAO, swingSeatCnt, texWood, texStone3);
        // ── Rooster on ridge ─────────────────────────────────────────────
       // Draw rooster — on roof normally, or walking on road when L is pressed
        if (g_roosterWalking) {
            // Gentle step bob only once on the ground
            float stepBob = g_roosterOnGround
                ? fabsf(sinf(now * 4.5f)) * 0.025f   // one-sided bob = natural peck-walk
                : 0.f;
            glm::vec3 drawPos = g_roosterWalkPos + glm::vec3(0.f, stepBob, 0.f);
            drawRooster(drawPos, now, texRooster);
        }
        else {
            drawRooster({ 0.30f, 7.72f, -18.f }, now, texRooster);
        }
        
        // Trees only (user request): no roads, buildings, props, characters, or VFX.
        // Realistic tree bunches — natural clusters with varied spacing and sizes
        static const struct { float x, z; float sc; int seed; } treeBunches[] = {
            // West side — staggered natural clusters (3-4 trees per group)
            // Cluster 1: near road entrance, sparse
            { -14.f,  -28.f, 1.2f, 200 }, { -17.f, -31.f, 1.5f, 201 }, { -19.f, -27.f, 1.0f, 202 },
            // Cluster 2: mid-west, dense copse
            { -22.f,  -48.f, 1.6f, 203 }, { -25.f, -51.f, 1.3f, 204 }, { -20.f, -53.f, 1.7f, 205 }, { -27.f, -46.f, 1.1f, 250 },
            // Cluster 3: further west, scattered
            { -32.f,  -72.f, 1.4f, 206 }, { -35.f, -78.f, 1.6f, 207 }, { -29.f, -82.f, 1.2f, 208 },
            // Cluster 4: west-far, sparse treeline
            { -18.f, -105.f, 1.5f, 209 }, { -23.f,-110.f, 1.3f, 210 }, { -15.f,-112.f, 1.4f, 211 },
            // East side — asymmetric natural clusters
            // Cluster 5: near road, scattered pair
            {  15.f,  -30.f, 1.3f, 220 }, {  19.f, -33.f, 1.1f, 221 },
            // Cluster 6: mid-east, dense grove
            {  24.f,  -50.f, 1.5f, 223 }, {  21.f, -54.f, 1.7f, 224 }, {  28.f, -48.f, 1.2f, 225 }, {  26.f, -56.f, 1.4f, 251 },
            // Cluster 7: east woodland edge
            {  30.f,  -75.f, 1.5f, 226 }, {  34.f, -79.f, 1.3f, 227 }, {  27.f, -83.f, 1.6f, 228 }, {  32.f, -85.f, 1.1f, 252 },
            // Cluster 8: east-far, sporadic
            {  20.f, -108.f, 1.4f, 229 }, {  25.f,-113.f, 1.2f, 230 },
            // Deep background forest edges (both sides) — larger trees
            { -42.f,  -25.f, 1.9f, 240 }, { -48.f, -32.f, 1.7f, 241 }, { -44.f, -38.f, 2.0f, 242 }, { -39.f, -20.f, 1.6f, 253 },
            {  42.f,  -28.f, 1.8f, 243 }, {  47.f, -35.f, 1.6f, 244 }, {  40.f, -22.f, 1.9f, 245 }, {  45.f, -18.f, 1.5f, 254 },
            { -52.f,  -65.f, 1.7f, 246 }, { -56.f, -72.f, 1.9f, 247 }, { -49.f, -60.f, 1.5f, 255 },
            {  52.f,  -68.f, 1.6f, 248 }, {  57.f, -62.f, 1.8f, 249 }, {  49.f, -74.f, 1.4f, 256 },
        };
        for (auto& tb : treeBunches)
            drawTree({ tb.x, 0.f, tb.z }, tb.sc * 1.45f, tb.seed);
        // Left background forest
        for (int i = 0; i < 6; i++) {
            drawTree({ -74.f - i * 2.2f, 0, -102.f - i * 1.5f }, (1.1f + i * 0.05f) * 1.45f, 110 + i);
            drawTree({ -86.f + i * 1.5f, 0, -104.f - i * 1.0f }, (1.0f + i * 0.04f) * 1.45f, 116 + i);
        }
        // Right background forest
        for (int i = 0; i < 6; i++) {
            drawTree({ 74.f + i * 2.2f, 0, -102.f - i * 1.5f }, (1.1f + i * 0.05f) * 1.45f, 122 + i);
            drawTree({ 86.f - i * 1.5f, 0, -104.f - i * 1.0f }, (1.0f + i * 0.04f) * 1.45f, 128 + i);
        }

        // ── Draw UFO hovering above west cluster-1 tree ───────────────────────
        {
            // Smooth up-down sinusoidal bob (UFO_BOB_LO .. UFO_BOB_HI)
            float ufoY = UFO_BOB_LO + (UFO_BOB_HI - UFO_BOB_LO)
                * (0.5f + 0.5f * sinf(now * UFO_BOB_SPD * 2.f * PI));
            glm::vec3 ufoPos = { UFO_CENTER.x, ufoY, UFO_CENTER.z };
            drawUFO(ufoPos, g_ufoSpinAngle, now);
        }

        // ── Draw Fireflies ────────────────────────────────────────────────────
        // Only render at night so they're clearly visible
        if (!g_day) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive blending for glow
            for (int fi = 0; fi < (int)g_fireflies.size(); fi++) {

                const Firefly& ff = g_fireflies[fi];
                glm::vec3 wp = fireflyPos(ff, now);

                // Per-firefly individual flicker (offset by index)
                float indivFlk = 0.6f + 0.4f * sinf(now * (3.5f + fi * 0.37f) + ff.phase);

                // Core: tiny bright sphere
                glm::mat4 fm = glm::translate(glm::mat4(1), wp);
                fm = glm::scale(fm, glm::vec3(0.04f));
                drw(spV, spC, fm, ff.color * indivFlk, 0, true);

                // Halo: slightly larger, more transparent sphere for bloom effect
                glm::mat4 hm = glm::translate(glm::mat4(1), wp);
                hm = glm::scale(hm, glm::vec3(0.09f));
                drw(spV, spC, hm, ff.color * (indivFlk * 0.4f), 0, true);
            }
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
        }
        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}