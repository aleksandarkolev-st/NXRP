#include "GameView.h"
#include "InteractionPose.h"
#include "Maze.Acoustics.h"
#include "Maze.Puzzle.h"
#include "Maze.Atmosphere.h"
#include "Maze.CreatureMesh.h"
#include "ExecutablePath.h"
#include <SFML/OpenGL.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <vector>

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT 0x8D00
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif

namespace
{
    using PfnGenFramebuffers = void (APIENTRY*)(GLsizei, GLuint*);
    using PfnDeleteFramebuffers = void (APIENTRY*)(GLsizei, const GLuint*);
    using PfnBindFramebuffer = void (APIENTRY*)(GLenum, GLuint);
    using PfnFramebufferTexture2D = void (APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
    using PfnCheckFramebufferStatus = GLenum (APIENTRY*)(GLenum);
    using PfnActiveTexture = void (APIENTRY*)(GLenum);

    struct FramebufferApi
    {
        PfnGenFramebuffers genFramebuffers = nullptr;
        PfnDeleteFramebuffers deleteFramebuffers = nullptr;
        PfnBindFramebuffer bindFramebuffer = nullptr;
        PfnFramebufferTexture2D framebufferTexture2D = nullptr;
        PfnCheckFramebufferStatus checkFramebufferStatus = nullptr;
        PfnActiveTexture activeTexture = nullptr;

        bool load()
        {
            const auto fetch = [](const char* name) -> void*
            {
                const auto address = reinterpret_cast<std::uintptr_t>(wglGetProcAddress(name));
                return address > 3u ? reinterpret_cast<void*>(address) : nullptr;
            };
            genFramebuffers = reinterpret_cast<PfnGenFramebuffers>(fetch("glGenFramebuffers"));
            deleteFramebuffers = reinterpret_cast<PfnDeleteFramebuffers>(fetch("glDeleteFramebuffers"));
            bindFramebuffer = reinterpret_cast<PfnBindFramebuffer>(fetch("glBindFramebuffer"));
            framebufferTexture2D = reinterpret_cast<PfnFramebufferTexture2D>(fetch("glFramebufferTexture2D"));
            checkFramebufferStatus = reinterpret_cast<PfnCheckFramebufferStatus>(fetch("glCheckFramebufferStatus"));
            activeTexture = reinterpret_cast<PfnActiveTexture>(fetch("glActiveTexture"));
            if (!activeTexture) activeTexture = reinterpret_cast<PfnActiveTexture>(fetch("glActiveTextureARB"));
            return genFramebuffers && deleteFramebuffers && bindFramebuffer
                && framebufferTexture2D && checkFramebufferStatus && activeTexture;
        }
    };

    FramebufferApi& framebufferApi()
    {
        static FramebufferApi api;
        static const bool loaded = api.load();
        (void)loaded;
        return api;
    }

    constexpr float pi = 3.14159265f;
    constexpr float safehouseDoorWidth = 1.16f * refugeScale;
    const char* names[] = { "MOON", "THORN", "EYE" };
    const float colors[][3] = { { .42f, .68f, .85f }, { .80f, .35f, .25f }, { .76f, .69f, .32f } };
    bool same(MazePosition a, MazePosition b) { return a.row == b.row && a.col == b.col; }
    bool open(const MazeGrid& maze, int row, int col)
    {
        return maze.contains({ row, col }) && maze.at(row, col) != MazeCell::Wall;
    }
    void quad(float ax, float ay, float az, float bx, float by, float bz,
              float cx, float cy, float cz, float dx, float dy, float dz,
              float nx, float ny, float nz)
    {
        glNormal3f(nx, ny, nz);
        glTexCoord2f(0, 0); glVertex3f(ax, ay, az);
        glTexCoord2f(1, 0); glVertex3f(bx, by, bz);
        glTexCoord2f(1, 1); glVertex3f(cx, cy, cz);
        glTexCoord2f(0, 1); glVertex3f(dx, dy, dz);
    }
    void subdividedQuad(float ax, float ay, float az, float bx, float by, float bz,
                         float cx, float cy, float cz, float dx, float dy, float dz,
                         float nx, float ny, float nz, int divisions)
    {
        glNormal3f(nx, ny, nz);
        const auto vertex = [&](float u, float v)
        {
            const float x = (ax + (bx-ax)*u)*(1-v) + (dx + (cx-dx)*u)*v;
            const float y = (ay + (by-ay)*u)*(1-v) + (dy + (cy-dy)*u)*v;
            const float z = (az + (bz-az)*u)*(1-v) + (dz + (cz-dz)*u)*v;
            glTexCoord2f(u, v); glVertex3f(x, y, z);
        };
        for (int y = 0; y < divisions; ++y) for (int x = 0; x < divisions; ++x)
        {
            const float u0 = static_cast<float>(x) / divisions, u1 = static_cast<float>(x+1) / divisions;
            const float v0 = static_cast<float>(y) / divisions, v1 = static_cast<float>(y+1) / divisions;
            vertex(u0,v0); vertex(u1,v0); vertex(u1,v1); vertex(u0,v1);
        }
    }
    void emitBox(float x, float z, float half, float height)
    {
        const float l = x-half, r = x+half, n = z-half, f = z+half;
        quad(l,0,n,r,0,n,r,height,n,l,height,n,0,0,-1);
        quad(r,0,f,l,0,f,l,height,f,r,height,f,0,0,1);
        quad(l,0,f,l,0,n,l,height,n,l,height,f,-1,0,0);
        quad(r,0,n,r,0,f,r,height,f,r,height,n,1,0,0);
        quad(l,height,n,r,height,n,r,height,f,l,height,f,0,1,0);
    }
    void box(float x, float z, float half, float height)
    {
        glBegin(GL_QUADS); emitBox(x, z, half, height); glEnd();
    }
    void drawNightSky(float x, float y, float z, float flash, float time, std::uint32_t seed)
    {
        constexpr float radius = 20.f;
        constexpr int sectors = 32, bands = 8;
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_TEXTURE_2D);
        glPushMatrix(); glTranslatef(x, y, z);
        glBegin(GL_QUADS);
        for (int band = 0; band < bands; ++band) for (int sector = 0; sector < sectors; ++sector)
        {
            const auto vertex = [&](int b, int s)
            {
                const float elevation = (pi * .5f) * static_cast<float>(b) / bands;
                const float azimuth = 2.f * pi * static_cast<float>(s) / sectors;
                const float cloud = .5f + .5f * std::sin(azimuth * 3.f + std::sin(elevation * 4.f)
                    + static_cast<float>(seed % 97u) * .07f + time * .015f);
                const float horizon = 1.f - static_cast<float>(b) / bands;
                const float glow = .004f + .011f * cloud * horizon + flash * (.10f + .06f * cloud);
                glColor3f(.012f + glow * .65f, .019f + glow * .8f, .030f + glow);
                const float ring = radius * std::cos(elevation);
                glVertex3f(ring * std::sin(azimuth), radius * std::sin(elevation), ring * std::cos(azimuth));
            };
            vertex(band,sector); vertex(band,sector+1); vertex(band+1,sector+1); vertex(band+1,sector);
        }
        glEnd();
        // Sparse stars stay fixed in world space and fade during a flash.
        glPointSize(1.f); glBegin(GL_POINTS);
        for (unsigned i = 0; i < 40; ++i)
        {
            unsigned h = seed ^ (i * 0x9e3779b9u); h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15;
            const float azimuth = (h & 0xffffu) / 65535.f * 2.f * pi;
            const float elevation = .30f + ((h >> 16) & 0xffffu) / 65535.f * 1.15f;
            const float brightness = (.15f + (h % 11u) * .006f) * (1.f - flash);
            glColor3f(brightness * .8f, brightness * .9f, brightness);
            const float ring = radius * .99f * std::cos(elevation);
            glVertex3f(ring * std::sin(azimuth), radius * .99f * std::sin(elevation), ring * std::cos(azimuth));
        }
        glEnd(); glPopMatrix(); glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING); glEnable(GL_FOG);
    }

    void foliageCard(float x, float z, float nx, float nz, float along, float y,
                     float width, float height, float slant)
    {
        const float tx = -nz, tz = nx;
        const float centerX = x + tx * along + nx * .06f;
        const float centerZ = z + tz * along + nz * .06f;
        const float hx = tx * width * .5f + nx * slant;
        const float hz = tz * width * .5f + nz * slant;
        quad(centerX-hx, y, centerZ-hz, centerX+hx, y, centerZ+hz,
             centerX+hx, y+height, centerZ+hz, centerX-hx, y+height, centerZ-hz, nx,0,nz);
    }
    void part(float x, float y, float z, float width, float height, float depth)
    {
        glPushMatrix(); glTranslatef(x, y, z); glScalef(width, height, depth);
        box(0.f, 0.f, .5f, 1.f); glPopMatrix();
    }
    void ellipsoid(float x, float y, float z, float rx, float ry, float rz)
    {
        constexpr int slices = 12, bands = 8;
        glPushMatrix(); glTranslatef(x, y, z); glScalef(rx, ry, rz);
        for (int band = 0; band < bands; ++band)
        {
            const float lower = -pi * .5f + pi * static_cast<float>(band) / bands;
            const float upper = -pi * .5f + pi * static_cast<float>(band + 1) / bands;
            glBegin(GL_QUAD_STRIP);
            for (int slice = 0; slice <= slices; ++slice)
            {
                const float azimuth = 2.f * pi * static_cast<float>(slice) / slices;
                for (const float elevation : { lower, upper })
                {
                    const float nx = std::cos(elevation) * std::cos(azimuth);
                    const float ny = std::sin(elevation);
                    const float nz = std::cos(elevation) * std::sin(azimuth);
                    glNormal3f(nx, ny, nz); glVertex3f(nx, ny, nz);
                }
            }
            glEnd();
        }
        glPopMatrix();
    }
    void shrineGlyph(int symbol, int number)
    {
        glLineWidth(2.f);
        // Symbol and numeral float just above the stone and face the viewer.
        glBegin(GL_LINES);
        if (symbol == 0)
        {
            for (int i = 0; i < 16; ++i)
            {
                const float a = -pi * .65f + i * pi * 1.3f / 16.f;
                const float b = -pi * .65f + (i + 1) * pi * 1.3f / 16.f;
                glVertex3f(.10f * std::cos(a), .16f + .10f * std::sin(a), 0);
                glVertex3f(.10f * std::cos(b), .16f + .10f * std::sin(b), 0);
            }
        }
        else if (symbol == 1)
        {
            glVertex3f(0,.05f,0); glVertex3f(0,.27f,0);
            glVertex3f(-.10f,.11f,0); glVertex3f(.10f,.22f,0);
            glVertex3f(-.10f,.22f,0); glVertex3f(.10f,.11f,0);
        }
        else
        {
            glVertex3f(-.12f,.16f,0); glVertex3f(0,.24f,0);
            glVertex3f(0,.24f,0); glVertex3f(.12f,.16f,0);
            glVertex3f(.12f,.16f,0); glVertex3f(0,.08f,0);
            glVertex3f(0,.08f,0); glVertex3f(-.12f,.16f,0);
            glVertex3f(0,.12f,0); glVertex3f(0,.20f,0);
        }
        constexpr float segments[][4] = {
            {-.07f,.00f,.07f,.00f}, {.07f,.00f,.07f,-.10f},
            {.07f,-.10f,.07f,-.20f}, {-.07f,-.20f,.07f,-.20f},
            {-.07f,-.10f,-.07f,-.20f}, {-.07f,.00f,-.07f,-.10f},
            {-.07f,-.10f,.07f,-.10f} };
        constexpr unsigned masks[] = { 0, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };
        for (int i = 0; i < 7; ++i) if (masks[number] & (1u << i))
        {
            glVertex3f(segments[i][0], segments[i][1], 0);
            glVertex3f(segments[i][2], segments[i][3], 0);
        }
        glEnd(); glLineWidth(1.f);
    }
    void stoneCap(float x, float z, float half, float base, float peak)
    {
        glBegin(GL_TRIANGLES);
        glNormal3f(0.f, .45f, -1.f);
        glVertex3f(x-half,base,z-half); glVertex3f(x+half,base,z-half); glVertex3f(x,peak,z);
        glNormal3f(1.f, .45f, 0.f);
        glVertex3f(x+half,base,z-half); glVertex3f(x+half,base,z+half); glVertex3f(x,peak,z);
        glNormal3f(0.f, .45f, 1.f);
        glVertex3f(x+half,base,z+half); glVertex3f(x-half,base,z+half); glVertex3f(x,peak,z);
        glNormal3f(-1.f, .45f, 0.f);
        glVertex3f(x-half,base,z+half); glVertex3f(x-half,base,z-half); glVertex3f(x,peak,z);
        glEnd();
    }
    unsigned int loadCreatureMesh(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        auto mesh = readCreatureMesh(file);
        if (!mesh) return 0;
        const auto list = glGenLists(1);
        if (!list) return 0;
        glNewList(list, GL_COMPILE); glBegin(GL_TRIANGLES);
        for (auto index : mesh->indices)
        {
            const auto& vertex = mesh->vertices[index];
            glNormal3fv(vertex.normal); glTexCoord2fv(vertex.uv); glVertex3fv(vertex.position);
        }
        glEnd(); glEndList();
        return list;
    }

    struct UploadedAnimation { unsigned int first = 0, frames = 0; float duration = 0.f; };
    UploadedAnimation loadAnimatedMesh(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        const auto mesh = readAnimatedMesh(file);
        if (!mesh) return {};
        const auto count = static_cast<unsigned int>(mesh->frames.size());
        const auto first = glGenLists(static_cast<GLsizei>(count));
        if (!first) return {};
        for (unsigned int frame = 0; frame < count; ++frame)
        {
            glNewList(first + frame, GL_COMPILE); glBegin(GL_TRIANGLES);
            for (const auto index : mesh->indices)
            {
                const auto& vertex = mesh->frames[frame][index];
                glNormal3fv(vertex.normal); glTexCoord2fv(mesh->uv[index].data()); glVertex3fv(vertex.position);
            }
            glEnd(); glEndList();
        }
        return { first, count, mesh->duration };
    }

    unsigned int loadSurfaceTexture(const std::filesystem::path& path, unsigned int side = 512)
    {
        // Normalize the source to a power-of-two texture for older OpenGL drivers.
        sf::Image source;
        if (source.loadFromFile(path))
        {
            const auto size = source.getSize();
            const auto* input = source.getPixelsPtr();
            std::vector<unsigned char> rgba(side * side * 4);
            for (unsigned int y = 0; y < side; ++y) for (unsigned int x = 0; x < side; ++x)
            {
                const auto from = ((y * size.y / side) * size.x + x * size.x / side) * 4;
                const auto to = (y * side + x) * 4;
                for (int channel = 0; channel < 4; ++channel) rgba[to + channel] = input[from + channel];
            }
            unsigned int texture = 0;
            glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, side, side, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            return texture;
        }
        return 0;
    }
    unsigned int makeHedgeTexture()
    {
        if (const auto texture = loadSurfaceTexture(executableDirectory() / "assets" / "textures" / "hedge.png")) return texture;
        // Keep the maze readable if the optional texture is missing.
        constexpr int side = 128;
        std::vector<unsigned char> pixels(side * side * 3);
        std::mt19937 rng(0x64726162u);
        for (int y = 0; y < side; ++y) for (int x = 0; x < side; ++x)
        {
            const auto offset = (y * side + x) * 3;
            const auto noise = static_cast<unsigned char>(rng() % 13);
            pixels[offset] = 18 + noise / 2; pixels[offset + 1] = 32 + noise;
            pixels[offset + 2] = 16 + noise / 2;
        }
        // Layer small, wrapped ellipses so the procedural foliage tiles at cell edges.
        for (int leaf = 0; leaf < 800; ++leaf)
        {
            const int cx = rng() % side, cy = rng() % side;
            const int rx = 2 + rng() % 5, ry = 3 + rng() % 7;
            const int light = rng() % 48;
            for (int dy = -ry; dy <= ry; ++dy) for (int dx = -rx; dx <= rx; ++dx)
            {
                const float d = float(dx * dx) / (rx * rx) + float(dy * dy) / (ry * ry);
                if (d > 1.f) continue;
                const int x = (cx + dx + side) % side, y = (cy + dy + side) % side;
                const auto offset = (y * side + x) * 3;
                const int edge = d > .7f ? -10 : 0;
                const int vein = dx == 0 ? 9 : 0;
                pixels[offset] = static_cast<unsigned char>(std::max(0, 34 + light / 2 + edge + vein));
                pixels[offset + 1] = static_cast<unsigned char>(std::max(0, 64 + light + edge + vein));
                pixels[offset + 2] = static_cast<unsigned char>(std::max(0, 26 + light / 2 + edge));
            }
        }
        unsigned int texture = 0;
        glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, side, side, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        return texture;
    }

}

GameView::GameView(const sf::Font& font)
    : voiceLabel_(font,"",12), heading_(font, "WAYWARD", 24), objective_(font, "", 19), journal_(font, "", 16), hint_(font, "", 17),
      seedText_(font, "", 15), feedback_(font, "", 20), staminaLabel_(font, "BREATH", 14), soundLabel_(font, "", 14)
{
    heading_.setFillColor({ 231, 226, 198 });
    objective_.setFillColor({ 226, 222, 198 });
    journal_.setFillColor({ 177, 185, 165 });
    hint_.setFillColor({ 177, 185, 165 });
    seedText_.setFillColor({ 144, 155, 139 });
    feedback_.setFillColor({ 237, 189, 126 });
    staminaLabel_.setFillColor({ 177, 185, 165 });
    soundLabel_.setFillColor({ 177, 185, 165 });
}
void GameView::resize(sf::Vector2u size) { if (size.x && size.y) viewport_ = size; }
void GameView::reset(GameState* game)
{
    if (game && initialized_ && shift_.row >= 0 && game->maze.contains(shift_))
        game->maze.at(shift_.row, shift_.col) = MazeCell::Wall;
    initialized_ = false; clearChunks_ = true; notice_.clear();
}
void GameView::resetFeedback() { }

void GameView::initialize(GameState& game)
{
    seatedHouse_=-1; seatedBlend_=ballHoldBlend_=throwClock_=0.f; heldBallLast_=false;handInertia_={};lastHeldBall_={};
    interiorHunter_=false; interiorHouse_=-1; hunterY_=0.f;
    x_ = z_ = .5f; elevation_ = 0.f; jumpHeight_ = jumpVelocity_ = crouchBlend_ = 0.f; crouched_ = jumpHeld_ = false;
    const bool south = open(game.maze, 1, 0);
    angle_ = south ? 0.f : pi / 2.f; pitch_ = 0.f;
    inscriptionX_ = south ? .10f : .75f;
    inscriptionZ_ = south ? .75f : .10f;
    elapsed_ = monsterClock_ = shiftClock_ = ambientTime_ = directorClock_ = 0.f;
    directorEvent_ = 0;
    thunderEvent_ = 0; soundMoving_ = soundRunning_ = false;
    bobPhase_ = bobBlend_ = sprintBlend_ = 0.f;
    darkness_ = hedgeDarkness(x_, z_, game.mazeSeed.value_or(0));
    shift_ = { -1, -1 }; shiftPhase_ = ShiftPhase::Idle; shiftProgress_ = 0.f; shiftCount_ = 0; captureCount_ = 0;
    voiceNoiseTime_.fill(0.f);voiceNoiseLevel_.fill(0.f);
    remoteCaptures_.fill(0); seenRemoteCaptures_ = 0; remoteCaptureCooldown_.fill(0.f);
    safehouses_ = generateSafehouses(game.maze.size(), game.mazeSeed.value_or(0));
    doors_.assign(safehouses_.size(), {});
    refugeNoise_.assign(safehouses_.size(), 0.f); doorStrikes_.fill(0); drinkClock_ = 0.f; drinkCooldowns_.fill(0.f); drinks_.fill(0); seenDrink_=0; interactionSlot_=0;
    refugeBalls_.assign(safehouses_.size(), {});
    for (auto& balls : refugeBalls_) { balls[0].x = -.5f; balls[0].z = -.5f; balls[1].x = -.7f; balls[1].z = .5f; }
    shiftDelay_ = 10.f + static_cast<float>(game.mazeSeed.value_or(0) % 21u);
    fieldNotes_ = 0; journalPage_ = -1; refugeSlots_.assign(safehouses_.size(), {});
    poker_={}; pokerVisible_=false;
    packs_={};adventurePanels_.fill(0);openedCaches_=0;seenRecall_=seenRestore_=0;surveyed_.assign(game.maze.cellCount(),false);
    cacheOpenClocks_.fill(0.f);adventureMotionClock_=0.f;lootingCache_=-1;
    for (auto& house : doors_) for (auto& door : house)
    { door.open = false; door.swing = 0.f; door.rattle = 0.f;door.travel=0.f; }
    fear_ = scareClock_ = rattleClock_ = 0.f; scareCount_ = 0; whisperEvent_ = doorEvent_ = 0;
    whisperPan_ = doorPan_ = 0.f; doorSlam_ = false;
    doorSource_=-1;
    checkpoints_.fill(-1);
    solved_ = 0; activeStones_ = 0; gateOpen_ = false; inscriptionRead_ = false; gateInput_.clear();
    discoveredClues_.fill(false);
    puzzle_ = generateMazePuzzle(game.maze, game.mazeSeed.value_or(0));
    shrines_ = puzzle_.stones; order_ = puzzle_.order; clues_ = puzzle_.numbers;
    gateRule_ = puzzle_.gateRule; gateCode_ = gateCode(puzzle_);
    storyRuin_=generateStoryRuin(game.maze.size(),game.mazeSeed.value_or(0));
    storySites_=generateStorySites(game.maze,game.mazeSeed.value_or(0),shrines_);
    caches_=Adventure::caches(game.maze,game.mazeSeed.value_or(0),shrines_);
    storyFootsteps_=generateStoryFootsteps(game.maze,storySites_,safehouses_);
    resetHunter(game);
    monsterStepDuration_ = 1.15f; stamina_ = 1.f; exhausted_ = false;
    monsterHeading_ = monsterTargetHeading_ = 0.f; creatureGait_ = 0.f;
    movementHeard_ = runningHeard_ = false;
    notice_ = "Explore, gather and trade. E reads the inscription. I satchel / Tab atlas.";
    messageClock_ = 0.f;
    initialized_ = true;
}

bool GameView::walkable(const GameState& game, float x, float z) const
{
    constexpr float radius = .17f;
    if(elevation_<.1f)for(int i=0;i<4;++i)
    {
        const auto& site=storySites_[i];const float dx=x-(site.cell.col+site.x),dz=z-(site.cell.row+site.z);
        const float localX=dx*std::cos(site.angle)-dz*std::sin(site.angle),localZ=dx*std::sin(site.angle)+dz*std::cos(site.angle);
        const float width=i==0?.21f:i==1?.20f:.15f,depth=i==1?.136f:.04f;
        if(std::abs(localX)<width+radius && std::abs(localZ)<depth+radius)return false;
    }
    for (float dx : { -radius, radius }) for (float dz : { -radius, radius })
    {
        const int row = static_cast<int>(std::floor(z + dz));
        const int col = static_cast<int>(std::floor(x + dx));
        if (!open(game.maze, row, col) || (!gateOpen_ && game.maze.at(row, col) == MazeCell::Exit)) return false;
    }
    // Upper gallery rails and stair risers prevent stepping into thin air.
    if (elevation_ > .05f)
    {
        const int house = safehouseAt({ static_cast<int>(z_), static_cast<int>(x_) }, safehouses_);
        if (house < 0) return false;
        const float hx = safehouses_[house].col + .5f, hz = safehouses_[house].row + .5f;
        if (std::abs(x - hx) > 1.22f * refugeScale || std::abs(z - hz) > 1.22f * refugeScale) return false;
    }
    const float nextHeight = refugeFloorHeight(x, z, elevation_, safehouses_);
    if (std::abs(nextHeight - elevation_) > .18f) return false;
    // Cabins have a doorway in the middle of each side. Their walls are
    // physical, while the generator guarantees clear approaches to the doors.
    for (const auto house : safehouses_)
    {
        const float hx = house.col + .5f, hz = house.row + .5f;
        for (float side : { -1.f, 1.f }) for (float end : { -1.f, 1.f })
        {
            if (std::abs(x - (hx + side * 1.45f * refugeScale)) < .05f + radius &&
                std::abs(z - (hz + end * 1.02f * refugeScale)) < .43f * refugeScale + radius) return false;
            if (std::abs(z - (hz + side * 1.45f * refugeScale)) < .05f + radius &&
                std::abs(x - (hx + end * 1.02f * refugeScale)) < .43f * refugeScale + radius) return false;
        }
    }
    return !doorBlocks(x, z);
}

bool GameView::doorBlocks(float x, float z) const
{
    constexpr float radius = .17f;
    for (std::size_t house = 0; house < doors_.size(); ++house)
        for (int side = 0; side < 4; ++side)
        {
            const auto& door = doors_[house][side];
            if (door.swing > .45f) continue;
            const auto frame = safehouseDoorFrame(safehouses_[house], side);
            if (frame.spansZ)
            {
                if (std::abs(x - frame.centreX) < .09f + radius &&
                    std::abs(z - frame.centreZ) < .59f * refugeScale + radius) return true;
            }
            else if (std::abs(z - frame.centreZ) < .09f + radius &&
                std::abs(x - frame.centreX) < .59f * refugeScale + radius) return true;
        }
    return false;
}

void GameView::handleEvent(const sf::Event& event, GameState& game)
{
    if (!initialized_ || game.phase != GamePhase::Playing) return;
    const auto* key = event.getIf<sf::Event::KeyPressed>();
    if (!key) return;
    if(adventureEvent(key->code,game)) return;
    if(key->code==sf::Keyboard::Key::P) adventurePanels_[networkGuest_?networkSelfSlot_:interactionSlot_]=0;
    if(key->code==sf::Keyboard::Key::Q && (networkGuest_ || interactionSlot_==networkSelfSlot_)) pokerVisible_=false;
    if(pokerEvent(key->code,game)) return;
    if (key->code==sf::Keyboard::Key::Q) { seatedHouse_=-1; drinkClock_=0.f; notice_="You stand up."; messageClock_=2.f; return; }
    if (key->code == sf::Keyboard::Key::G || key->code == sf::Keyboard::Key::K)
    { if (!networkGuest_) playRefugeBall(key->code == sf::Keyboard::Key::G, networkSelfSlot_); return; }
    if (key->code == sf::Keyboard::Key::J || key->code == sf::Keyboard::Key::F)
    {
        const int house=safehouseAt(game.player,safehouses_),site=nearbyStorySite();
        const int note=site>=0?site:house>=0?4+house:-1;
        if(key->code==sf::Keyboard::Key::F && note>=0)
        {
            fieldNotes_|=1u<<note;journalPage_=note;
            notice_=std::string("FIELD NOTE COLLECTED / J to reread\n")+storyNote(note);
        }
        else if(key->code==sf::Keyboard::Key::J && fieldNotes_)
        {
            do {journalPage_=(journalPage_+1)%8;}while(!(fieldNotes_&(1u<<journalPage_)));
            notice_=storyNote(journalPage_);
        }
        else notice_="Look for abandoned belongings and keeper notes. F collects / J rereads.";
        messageClock_ = 14.f;
        return;
    }

    if (key->code == sf::Keyboard::Key::E) { interact(game); return; }
    if (!nearGate(game) || solved_ != 3 || gateOpen_) return;
    int digit = 0;
    switch (key->code)
    {
    case sf::Keyboard::Key::Num1: case sf::Keyboard::Key::Numpad1: digit = 1; break;
    case sf::Keyboard::Key::Num2: case sf::Keyboard::Key::Numpad2: digit = 2; break;
    case sf::Keyboard::Key::Num3: case sf::Keyboard::Key::Numpad3: digit = 3; break;
    case sf::Keyboard::Key::Num4: case sf::Keyboard::Key::Numpad4: digit = 4; break;
    case sf::Keyboard::Key::Num5: case sf::Keyboard::Key::Numpad5: digit = 5; break;
    case sf::Keyboard::Key::Num6: case sf::Keyboard::Key::Numpad6: digit = 6; break;
    case sf::Keyboard::Key::Num7: case sf::Keyboard::Key::Numpad7: digit = 7; break;
    case sf::Keyboard::Key::Num8: case sf::Keyboard::Key::Numpad8: digit = 8; break;
    case sf::Keyboard::Key::Num9: case sf::Keyboard::Key::Numpad9: digit = 9; break;
    case sf::Keyboard::Key::Backspace: if (!gateInput_.empty()) gateInput_.pop_back(); return;
    case sf::Keyboard::Key::Enter:
        if (gateInput_.size() == 3)
        {
            std::string answer;
            for (int number : gateCode_) answer += static_cast<char>('0' + number);
            if (gateInput_ == answer)
            {
                gateOpen_ = true; notice_ = "The gate opens. Step through.";
                if (game.maze.at(game.player.row, game.player.col) == MazeCell::Exit) game.phase = GamePhase::Won;
            }
            else notice_ = "The lock rejects the code. Study its inscription.";
            gateInput_.clear(); messageClock_ = 4.f;
        }
        return;
    default: return;
    }
    if (gateInput_.size() < 3) gateInput_ += static_cast<char>('0' + digit);
}

int GameView::nearbyShrine() const
{
    int nearest = -1;
    float best = .55f * .55f;
    for (int i = 0; i < 3; ++i)
    {
        const float dx = x_ - (shrines_[i].col + .5f), dz = z_ - (shrines_[i].row + .5f);
        const float distance = dx * dx + dz * dz;
        if (distance < best) { best = distance; nearest = i; }
    }
    return nearest;
}

int GameView::nearbySlotHandle() const
{
    if (elevation_ > .1f) return -1;
    for (std::size_t i = 0; i < safehouses_.size(); ++i)
    {
        const float dx = safehouses_[i].col + .5f - .56f * refugeScale - x_;
        const float dz = safehouses_[i].row + .5f - .85f * refugeScale - z_;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance < .80f && (distance < .15f || (dx * std::sin(angle_) + dz * std::cos(angle_)) / distance > .65f)) return static_cast<int>(i);
    }
    return -1;
}

void GameView::alignSlotView(int house)
{
    const auto knob=slotKnobPoint(safehouses_[house],0.f,2.f);
    x_=knob.x+.52f;z_=knob.z+.05f;angle_=std::atan2(-.52f,-.05f);pitch_=.23f;
}
void GameView::pullSlotHandle(const GameState& game)
{
    const int house = nearbySlotHandle();
    if (house < 0) { notice_ = "Face the red cabinet handle and press E to pull."; messageClock_ = 3.f; return; }
    auto& machine = refugeSlots_[house];
    if (elapsed_ < machine.stopTime) return;
    if(interactionSlot_==networkSelfSlot_) alignSlotView(house);
    machine.actor=interactionSlot_;
    machine.stopTime = elapsed_ + 2.f;
    refugeNoise_[house] = 5.f;
    std::mt19937 random(game.mazeSeed.value_or(0) ^ (++machine.spins * 2654435761u) ^ (house * 0x85ebca6bu));
    for (auto& symbol : machine.symbols) symbol = random() % 4;
    notice_ = "The Lucky Lantern reels spin. Match three symbols!"; messageClock_ = 3.f;
}

int GameView::nearbyDoor() const
{
    int nearest = -1;
    float best = 1.35f * 1.35f;
    for (std::size_t house = 0; house < doors_.size(); ++house)
        for (int side = 0; side < 4; ++side)
        {
            const auto frame = safehouseDoorFrame(safehouses_[house], side);
            const float dx = x_ - frame.centreX, dz = z_ - frame.centreZ;
            const float distance = dx * dx + dz * dz;
            if (distance < best)
            {
                best = distance;
                nearest = static_cast<int>(house * 4 + side);
            }
        }
    return nearest;
}

void GameView::toggleDoor(int house, int side, bool slam)
{
    if (house < 0 || static_cast<std::size_t>(house) >= doors_.size() || side < 0 || side > 3) return;
    auto& door = doors_[house][side];
    const auto frame = safehouseDoorFrame(safehouses_[house], side);
    const auto occupied = [&](float px, float pz)
    {
        const float dx = px - frame.centreX, dz = pz - frame.centreZ;
        return dx * dx + dz * dz < .78f * .78f;
    };
    if (slam)
    {
        if (!door.open) return;
        // Never crush a player standing in the opening. The creature can only
        // slam a door after the player has stepped clear of it.
        if (occupied(x_, z_)) return;
        for (const auto& player : remote_)
            if (player.present && occupied(player.x, player.z)) return;
        door.open = false; door.rattle = 1.f;
        doorEvent_++; doorSlam_ = true;
    }
    else
    {
        if (door.open && (occupied(x_, z_) || std::any_of(remote_.begin(), remote_.end(),
            [&](const RemotePlayer& player) { return player.present && occupied(player.x, player.z); })))
        {
            notice_ = "The doorway is occupied. Step clear before closing it.";
            messageClock_ = 2.5f;
            return;
        }
        door.open = !door.open; door.rattle = 0.f;
        doorEvent_++; doorSlam_ = false;
    }
    doorSource_=house*4+side;
    const float dx = frame.centreX - x_, dz = frame.centreZ - z_;
    const float distance = std::sqrt(dx * dx + dz * dz);
    doorPan_ = distance > .001f
        ? std::clamp((-dx * std::cos(angle_) + dz * std::sin(angle_)) / distance, -1.f, 1.f) : 0.f;
    notice_ = slam ? "The creature slams the safehouse door."
        : door.open ? "The door creaks open. The safehouse waits."
        : "The door swings shut. The hedge goes quiet.";
    messageClock_ = 3.f;
}

void GameView::updateDoors(float seconds)
{
    for (auto& house : doors_) for (auto& door : house)
    {
        const float target = door.open ? 1.f : 0.f;
        const float step = seconds * (door.open ? 2.6f : 3.2f);
        door.travel = door.travel < target ? std::min(target, door.travel + step)
            : std::max(target, door.travel - step);
        door.swing=interactionSmooth(door.travel);
        door.rattle = std::max(0.f, door.rattle - seconds * 1.8f);
    }
}

unsigned int GameView::doorMask() const noexcept
{
    unsigned int mask = 0;
    for (std::size_t house = 0; house < doors_.size(); ++house)
        for (int side = 0; side < 4; ++side)
            if (doors_[house][side].open) mask |= 1u << (house * 4 + side);
    return mask;
}

bool GameView::nearGate(const GameState& game) const
{
    const auto& size = game.maze.size();
    const float dx = x_ - (size.cols - .5f), dz = z_ - (size.rows - .5f);
    return dx * dx + dz * dz < 1.15f * 1.15f;
}

bool GameView::nearInscription() const
{
    const float dx = x_ - inscriptionX_, dz = z_ - inscriptionZ_;
    return dx * dx + dz * dz < .7f * .7f;
}

int GameView::nearbyBar() const
{
    if(elevation_>.1f) return -1;
    for(std::size_t house=0;house<safehouses_.size();++house)
    {
        const float dx=x_-(safehouses_[house].col+.5f-.85f*refugeScale);
        const float dz=z_-(safehouses_[house].row+.5f+.85f*refugeScale);
        if(std::abs(dx)<1.2f && std::abs(dz)<.95f) return static_cast<int>(house);
    }
    return -1;
}

bool GameView::pokerEvent(sf::Keyboard::Key key,const GameState& game)
{
    const int house=nearbyBar();
    if(house<0) return false;
    const int actor=networkGuest_?networkSelfSlot_:interactionSlot_;
    auto& hand=poker_[actor];
    const sf::Keyboard::Key digits[]={sf::Keyboard::Key::Num1,sf::Keyboard::Key::Num2,sf::Keyboard::Key::Num3,sf::Keyboard::Key::Num4,sf::Keyboard::Key::Num5};
    const sf::Keyboard::Key pads[]={sf::Keyboard::Key::Numpad1,sf::Keyboard::Key::Numpad2,sf::Keyboard::Key::Numpad3,sf::Keyboard::Key::Numpad4,sf::Keyboard::Key::Numpad5};
    if(key==sf::Keyboard::Key::P)
    {
        if(actor==networkSelfSlot_) pokerVisible_=true;
        if(!networkGuest_ && RefugePoker::play(hand,game.mazeSeed.value_or(0),actor,elapsed_))
        {
            refugeNoise_[house]=3.f;
            notice_=hand.phase==1?"Cards scrape across the bar. Choose what to hold.":"The draw is over. Every sound carries beyond the doors.";
            messageClock_=4.f;
        }
        return true;
    }
    if(hand.phase==1) for(int i=0;i<5;++i) if(key==digits[i] || key==pads[i])
    {
        if(!networkGuest_) hand.held^=1u<<i;
        return true;
    }
    return false;
}

void GameView::drawPoker(sf::RenderTarget& target)
{
    if(!pokerVisible_ || nearbyBar()<0) return;
    const auto& hand=poker_[networkSelfSlot_];
    if(!hand.phase) return;
    const float scale=std::min(1.f,static_cast<float>(viewport_.x)/760.f);
    const float left=(viewport_.x-660.f*scale)*.5f, top=viewport_.y*.48f;
    sf::RectangleShape panel({660.f*scale,214.f*scale});
    panel.setPosition({left,top}); panel.setFillColor({12,29,25,235});
    panel.setOutlineColor({172,139,81});panel.setOutlineThickness(1.f);target.draw(panel);
    sf::Text text(heading_.getFont(),"",static_cast<unsigned>(18*scale));
    const auto label=[&](const std::string& value,float x,float y,sf::Color color)
    {text.setString(value);text.setFillColor(color);text.setPosition({left+x*scale,top+y*scale});target.draw(text);};
    label("KEEPER'S DRAW / FREE PLAY",18,10,{230,205,156});
    label("SCORE "+std::to_string(hand.score),470,10,{230,205,156});
    static constexpr const char* ranks[]={"2","3","4","5","6","7","8","9","10","J","Q","K","A"};
    static constexpr const char* suits[]={"CLUBS","DIAMONDS","HEARTS","SPADES"};
    for(int i=0;i<5;++i)
    {
        const bool held=hand.held&(1u<<i); const int c=hand.cards[i];
        const float reveal=hand.phase==2 && held?1.f:interactionSmooth((1.2f+elapsed_-hand.readyAt-i*.075f)/.24f);
        const float width=112.f*std::max(.06f,std::abs(2.f*reveal-1.f));
        const float lift=held?5.f:0.f;
        sf::RectangleShape card({width*scale,100.f*scale});
        card.setPosition({left+(18+i*127.f+(112.f-width)*.5f)*scale,top+(44+16.f*(1.f-reveal)-lift)*scale});
        card.setFillColor(reveal<.5f?sf::Color(100,35,31):held?sf::Color(241,224,177):sf::Color(220,217,202));
        card.setOutlineColor({209,157,67});card.setOutlineThickness(held?3.f:0.f);target.draw(card);
        if(reveal<.97f) continue;
        const sf::Color ink=c/13==1 || c/13==2?sf::Color(143,35,29):sf::Color(25,36,32);
        label(ranks[c%13],28+i*127.f,52-lift,ink);label(suits[c/13],28+i*127.f,82-lift,ink);
        label(std::to_string(i+1)+(held?" HOLD":""),28+i*127.f,116-lift,ink);
    }
    label(elapsed_<hand.readyAt-.55f?"DEALING...":hand.phase==1?"1-5 hold / P draw once / Q put cards away":std::string(RefugePoker::name(RefugePoker::category(hand.cards)))+" / +"+std::to_string(RefugePoker::payout(RefugePoker::category(hand.cards)))+" / P deal again",18,157,{230,205,156});
    label("The world keeps moving. Listen for the doors.",18,184,{157,181,164});
}

void GameView::sitAtBar(int house)
{
    seatedHouse_=house;
    x_=safehouses_[house].col+.5f-.85f*refugeScale+1.10f;
    z_=safehouses_[house].row+.5f+.85f*refugeScale;
    angle_=-pi*.5f; pitch_=0.f; jumpHeight_=jumpVelocity_=0.f;
}
void GameView::interact(GameState& game)
{
    const int cache=nearbyCache();
    if(cache>=0)
    {
        if(interactionSlot_==networkSelfSlot_) {adventureMotionClock_=1.2f;adventureMotionItem_=-1;lootingCache_=cache;}
        if(networkGuest_)return;
        auto& pack=packs_[interactionSlot_];const auto loot=caches_[cache];
        openedCaches_|=std::uint64_t{1}<<cache;
        cacheOpenClocks_[cache]=1.2f;
        pack.coins=std::min(99999u,pack.coins+loot.coins);
        pack.materials[loot.kind]=std::min(999u,pack.materials[loot.kind]+2);
        notice_=std::string("Cache discovered: +2 ")+Adventure::materials[loot.kind]+" / +"+std::to_string(loot.coins)+" crowns. I opens your satchel.";
        messageClock_=5.f;return;
    }
    if (nearbySlotHandle() >= 0) { pullSlotHandle(game); return; }
    const int bar=nearbyBar();
    if(bar>=0)
    {
        if (drinkCooldowns_[interactionSlot_] > 0.f)
        { notice_="BAR / Finishing your drink... / Q stand"; messageClock_=2.f; return; }
        drinkCooldowns_[interactionSlot_]=4.f; ++drinks_[interactionSlot_];
        if (interactionSlot_ == networkSelfSlot_)
        { sitAtBar(bar); drinkClock_=4.f; stamina_=1.f; fear_=std::max(0.f,fear_-.15f); }
        refugeNoise_[bar]=4.f;
        notice_="You sit at the bar and drink. E another drink / Q stand.";
        messageClock_=5.f; return;
    }
    const int door = elevation_ < .1f ? nearbyDoor() : -1;
    if (door >= 0)
    {
        toggleDoor(door / 4, door % 4);
        return;
    }
    const int i = nearbyShrine();
    if (i >= 0)
    {
        const auto result = touchStone(puzzle_, i, solved_, activeStones_);
        if (result == StoneTouchResult::Solved || result == StoneTouchResult::AlreadySolved)
            notice_ = "The seal is broken. Read the stone numbers at the gate.";
        else if (result == StoneTouchResult::Reset) notice_ = "The stones fall silent. The sequence begins again.";
        else if (puzzle_.stoneKind == StonePuzzleKind::Balance)
            notice_ = "The lit stones total " + std::to_string(stoneBalance(puzzle_, activeStones_)) + ".";
        else notice_ = "The stone answers. Find the next symbol.";
        messageClock_ = 4.f;
        return;
    }
    if (nearInscription())
    {
        inscriptionRead_ = true;
        notice_ = puzzle_.stoneKind == StonePuzzleKind::Balance
            ? "Light stones to match the total; E toggles each stone."
            : "You record the inscription's sequence.";
        messageClock_ = 4.f;
        return;
    }
    if (nearGate(game))
    { notice_ = solved_ == 3 ? "The gate inscription explains the code." : "The gate is sealed by three stones."; messageClock_ = 3.f; }
}

void GameView::resetHunter(const GameState& game)
{
    interiorHunter_=false; interiorHouse_=-1; hunterY_=0.f;
    const auto routes=hunter_.routes;
    const auto vigilance=hunter_.vigilance;
    hunter_ = makeHunter(game.maze, game.mazeSeed.value_or(0), captureCount_, shrines_, shift_, safehouses_);
    if(captureCount_>0){hunter_.routes=routes;hunter_.vigilance=vigilance;}
    monster_ = hunter_.cell; monsterFrom_ = monster_;
}

void GameView::emitVoiceNoise(int slot,float level)
{
    if(slot<0 || slot>=maxCoopPlayers || !std::isfinite(level) || level<.01f) return;
    voiceNoiseTime_[slot]=.35f;voiceNoiseLevel_[slot]=std::clamp(level,0.f,1.f);
}

RemotePlayer GameView::localPlayer() const noexcept
{
    return { x_, z_, angle_, soundMoving_ || (crouched_ && bobBlend_ > .1f), soundRunning_, initialized_, elevation_, jumpHeight_, crouched_, seatedHouse_>=0 };
}

void GameView::setRemotePlayer(int slot, const RemotePlayer& player) noexcept
{
    if (slot < 0 || slot >= maxCoopPlayers) return;
    auto& current = remote_[slot]; auto& visual = remoteVisual_[slot];
    const float visualDx = player.x - visual.x, visualDz = player.z - visual.z;
    if (!current.present || visualDx * visualDx + visualDz * visualDz > 9.f) visual = player;
    current = player;
    if (!initialized_ || !player.present) return;
    if (!networkGuest_ && slot > 0)
    {
        const auto cell = MazePosition{ static_cast<int>(player.z), static_cast<int>(player.x) };
        const int house = safehouseAt(cell, safehouses_);
        if (house >= 0) checkpoints_[slot] = house;
    }
    for (int i = 0; i < 3; ++i)
    {
        const float dx = player.x - (shrines_[i].col + .5f), dz = player.z - (shrines_[i].row + .5f);
        if (dx * dx + dz * dz < .55f * .55f) discoveredClues_[i] = true;
    }
}

void GameView::remoteDisconnected(int slot) noexcept
{
    if (slot < 0)
    {
        for (int i = 0; i < maxCoopPlayers; ++i) remoteDisconnected(i);
        return;
    }
    if (slot >= maxCoopPlayers) return;
    remote_[slot].present = remoteVisual_[slot].present = false;
    remoteCaptures_[slot] = 0; remoteCaptureCooldown_[slot] = 0.f; checkpoints_[slot] = -1;
}

std::string GameView::worldSnapshot() const
{
    unsigned clues = 0;
    for (unsigned i = 0; i < discoveredClues_.size(); ++i) if (discoveredClues_[i]) clues |= 1u << i;
    std::ostringstream out;
    out.precision(8);
    out << x_ << ' ' << z_ << ' ' << angle_ << ' ' << soundMoving_ << ' ' << soundRunning_ << ' ' << elevation_ << ' ' << jumpHeight_ << ' ' << crouched_ << ' ' << (seatedHouse_>=0);
    for (int slot = 1; slot < maxCoopPlayers; ++slot)
    {
        const auto& player = remote_[slot];
        out << ' ' << player.present << ' ' << player.x << ' ' << player.z << ' ' << player.angle
            << ' ' << player.moving << ' ' << player.running << ' ' << player.elevation << ' ' << player.jumpHeight << ' ' << player.crouched << ' ' << player.seated;
    }
    out << ' '
        << monsterFrom_.row << ' ' << monsterFrom_.col << ' ' << monster_.row << ' ' << monster_.col << ' '
        << monsterClock_ << ' ' << monsterStepDuration_ << ' ' << monsterHeading_ << ' ' << creatureGait_ << ' '
        << elapsed_ << ' ' << ambientTime_ << ' ' << static_cast<int>(hunter_.mode) << ' ' << static_cast<int>(shiftPhase_) << ' '
        << shift_.row << ' ' << shift_.col << ' ' << shiftProgress_ << ' '
        << solved_ << ' ' << activeStones_ << ' ' << inscriptionRead_ << ' ' << gateOpen_ << ' '
        << (gateInput_.empty() ? "-" : gateInput_) << ' ' << clues;
    for (int slot = 1; slot < maxCoopPlayers; ++slot) out << ' ' << remoteCaptures_[slot];
    for (int slot = 0; slot < maxCoopPlayers; ++slot) out << ' ' << checkpoints_[slot];
    out << ' ' << doorMask();
    for (const auto& balls : refugeBalls_) for (const auto& b : balls)
        out << ' ' << b.x << ' ' << b.y << ' ' << b.z << ' ' << b.vx << ' ' << b.vy << ' ' << b.vz << ' ' << b.carrier << ' ' << b.scoreA << ' ' << b.scoreB << ' ' << b.throwWindup;
    for (const auto& machine : refugeSlots_)
        out << ' ' << machine.spins << ' ' << machine.stopTime << ' ' << machine.symbols[0] << ' ' << machine.symbols[1] << ' ' << machine.symbols[2] << ' ' << machine.actor;
    for (const auto count : drinks_) out << ' ' << count;
    out << ' ' << interiorHunter_ << ' ' << hunterX_ << ' ' << hunterY_ << ' ' << hunterZ_;
    out << ' ' << doorEvent_ << ' ' << doorSource_ << ' ' << doorSlam_;
    for(std::size_t house=0;house<doors_.size();++house) for(int side=0;side<4;++side)
        out << ' ' << doors_[house][side].rattle << ' ' << doorStrikes_[house*4+side];
    out << ' ' << openedCaches_;
    for(int actor=0;actor<maxCoopPlayers;++actor)
    {
        const auto& p=packs_[actor];out << ' ' << p.coins << ' ' << p.districts << ' ' << p.recalls << ' ' << p.restores << ' ' << p.echoes << ' ' << adventurePanels_[actor];
        for(auto n:p.materials)out << ' ' << n;for(auto n:p.items)out << ' ' << n;for(auto t:p.effects)out << ' ' << t;
        out << ' ' << p.echoTime << ' ' << p.echo.row << ' ' << p.echo.col << ' ' << p.useCooldown;
    }
    for(const auto& hand:poker_)
    {
        out << ' ' << hand.rounds << ' ' << hand.held << ' ' << hand.score << ' ' << hand.phase << ' ' << hand.readyAt;
        for(int c:hand.cards) out << ' ' << c;
    }
    return out.str();
}

bool GameView::applyWorldSnapshot(GameState& game, const std::string& data, int selfSlot)
{
    if (!initialized_ || game.maze.empty() || selfSlot < 1 || selfSlot >= maxCoopPlayers) return false;
    std::istringstream in(data);
    std::array<RemotePlayer, maxCoopPlayers> players{};
    std::array<int, maxCoopPlayers> presence{}, moving{}, running{}, crouching{}, seating{};
    std::array<std::uint32_t, maxCoopPlayers> captures{};
    std::array<int, maxCoopPlayers> checkpoints{};
    MazePosition from{}, monster{}, shift{};
    float clock, duration, heading, gait, elapsed, ambient, progress;
    int hunterMode, phase, solved, inscription, gate;
    unsigned clues, activeStones, doorsMask;
    std::string input, extra;
    presence[0] = 1;
    if (!(in >> players[0].x >> players[0].z >> players[0].angle >> moving[0] >> running[0] >> players[0].elevation >> players[0].jumpHeight >> crouching[0] >> seating[0])) return false;
    for (int slot = 1; slot < maxCoopPlayers; ++slot)
        if (!(in >> presence[slot] >> players[slot].x >> players[slot].z >> players[slot].angle >> moving[slot] >> running[slot] >> players[slot].elevation >> players[slot].jumpHeight >> crouching[slot] >> seating[slot])) return false;
    if (!(in >> from.row >> from.col >> monster.row >> monster.col >> clock >> duration >> heading >> gait
        >> elapsed >> ambient >> hunterMode >> phase >> shift.row >> shift.col >> progress >> solved >> activeStones >> inscription >> gate
        >> input >> clues)) return false;
    for (int slot = 1; slot < maxCoopPlayers; ++slot) if (!(in >> captures[slot])) return false;
    for (int slot = 0; slot < maxCoopPlayers; ++slot) if (!(in >> checkpoints[slot])) return false;
    if (!(in >> doorsMask)) return false;
    auto balls = refugeBalls_;
    for (auto& houseBalls : balls) for (auto& b : houseBalls)
    {
        if (!(in >> b.x >> b.y >> b.z >> b.vx >> b.vy >> b.vz >> b.carrier >> b.scoreA >> b.scoreB >> b.throwWindup)) return false;
        if(!std::isfinite(b.throwWindup) || b.throwWindup<0.f || b.throwWindup>.281f || (b.throwWindup>0.f && b.carrier<0))return false;
        if (!std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.z) || !std::isfinite(b.vx) ||
            !std::isfinite(b.vy) || !std::isfinite(b.vz) || std::abs(b.x) > 3.f || std::abs(b.z) > 3.f ||
            b.y < 0.f || b.y > 3.f || b.carrier < -1 || b.carrier >= maxCoopPlayers ||
            std::abs(b.vx) > 20.f || std::abs(b.vy) > 20.f || std::abs(b.vz) > 20.f) return false;
    }
    auto machines = refugeSlots_;
    for (auto& machine : machines)
    {
        if (!(in >> machine.spins >> machine.stopTime >> machine.symbols[0] >> machine.symbols[1] >> machine.symbols[2] >> machine.actor)) return false;
        if (machine.actor<0 || machine.actor>=maxCoopPlayers || !std::isfinite(machine.stopTime) || machine.stopTime < 0.f) return false;
        for (const int symbol : machine.symbols) if (symbol < 0 || symbol > 3) return false;
    }
    std::array<std::uint32_t,maxCoopPlayers> drinks{};
    for (auto& count : drinks) if (!(in >> count)) return false;
    int interior; float hx,hy,hz;
    if (!(in >> interior >> hx >> hy >> hz) || (interior!=0 && interior!=1) ||
        !std::isfinite(hx) || !std::isfinite(hy) || !std::isfinite(hz) || hy<0.f || hy>refugeUpperFloor ||
        (interior && (hx<0.f || hz<0.f || hx>=game.maze.size().cols || hz>=game.maze.size().rows))) return false;
    std::uint32_t doorEvent;int doorSource,doorSlam;
    if(!(in>>doorEvent>>doorSource>>doorSlam) || doorSource < -1 || doorSource>=static_cast<int>(doors_.size()*4)
        || (doorSlam!=0 && doorSlam!=1) || (doorEvent && doorSource<0)) return false;
    auto replicatedDoors=doors_; auto strikes=doorStrikes_;
    for(std::size_t house=0;house<doors_.size();++house) for(int side=0;side<4;++side)
    {
        auto& rattle=replicatedDoors[house][side].rattle;auto& count=strikes[house*4+side];
        if(!(in>>rattle>>count) || !std::isfinite(rattle) || rattle<0.f || rattle>1.f || count>2) return false;
    }
    std::uint64_t opened;
    if(!(in>>opened) || (opened>>caches_.size())!=0) return false;
    auto packs=packs_;auto panels=adventurePanels_;
    for(int actor=0;actor<maxCoopPlayers;++actor)
    {
        auto& p=packs[actor];
        if(!(in>>p.coins>>p.districts>>p.recalls>>p.restores>>p.echoes>>panels[actor]))return false;
        for(auto& n:p.materials)if(!(in>>n))return false;for(auto& n:p.items)if(!(in>>n))return false;for(auto& t:p.effects)if(!(in>>t))return false;
        if(!(in>>p.echoTime>>p.echo.row>>p.echo.col>>p.useCooldown) || !Adventure::valid(p) || panels[actor]<0 || panels[actor]>4) return false;
        if(!(p.echo.row==-1 && p.echo.col==-1) && (!game.maze.contains(p.echo) || game.maze.at(p.echo.row,p.echo.col)==MazeCell::Wall))return false;
        if(p.echoTime>0 && p.echo.row<0)return false;
    }
    auto poker=poker_;
    for(auto& hand:poker)
    {
        if(!(in>>hand.rounds>>hand.held>>hand.score>>hand.phase>>hand.readyAt)) return false;
        for(auto& c:hand.cards) if(!(in>>c)) return false;
        if(!RefugePoker::valid(hand) || !std::isfinite(hand.readyAt) || hand.readyAt<0.f) return false;
    }
    if (in >> extra) return false;
    const unsigned int doorLimit = doors_.size() < 8
        ? (1u << (doors_.size() * 4)) : 0xffffffffu;
    if (doorsMask >= doorLimit) return false;
    const auto finite = [](float value) { return std::isfinite(value); };
    const auto size = game.maze.size();
    const auto validCell = [&](MazePosition p) { return game.maze.contains(p); };
    for (int slot = 0; slot < maxCoopPlayers; ++slot)
    {
        const auto& player = players[slot];
        if (!finite(player.jumpHeight) || player.jumpHeight < 0.f || player.jumpHeight > .6f || (crouching[slot]!=0 && crouching[slot]!=1) || (seating[slot]!=0 && seating[slot]!=1) || !finite(player.elevation) || player.elevation < 0.f || player.elevation > refugeUpperFloor || !finite(player.x) || !finite(player.z) || !finite(player.angle) || player.x < 0 || player.z < 0 ||
            player.x >= size.cols || player.z >= size.rows || (presence[slot] != 0 && presence[slot] != 1) ||
            (moving[slot] != 0 && moving[slot] != 1) || (running[slot] != 0 && running[slot] != 1) ||
            checkpoints[slot] < -1 || checkpoints[slot] >= static_cast<int>(safehouses_.size())) return false;
    }
    if (!validCell(from) || !validCell(monster) ||
        !finite(clock) || !finite(duration) || !finite(heading) || !finite(gait) || !finite(elapsed) ||
        !finite(ambient) || !finite(progress) || clock < 0 || duration <= 0 || duration > 5 || elapsed < 0 || ambient < 0 ||
        progress < 0 || progress > 1 || hunterMode < 0 || hunterMode > 2 || phase < 0 || phase > 3 || solved < 0 || solved > 3 || clues > 7 || activeStones > 7 ||
        (inscription != 0 && inscription != 1) || (gate != 0 && gate != 1) ||
        (phase == 0 ? (shift.row != -1 || shift.col != -1) : !validCell(shift)) ||
        (input != "-" && (input.size() > 3 || input.find_first_not_of("123456789") != std::string::npos))) return false;

    const auto oldShift = shift_;
    const auto oldPhase = shiftPhase_;
    if (oldPhase == ShiftPhase::Open && game.maze.contains(oldShift))
        game.maze.at(oldShift.row, oldShift.col) = MazeCell::Wall;
    shift_ = shift; shiftPhase_ = static_cast<ShiftPhase>(phase); shiftProgress_ = progress;
    if (shiftPhase_ == ShiftPhase::Open) game.maze.at(shift.row, shift.col) = MazeCell::Passage;
    if (!same(oldShift, shift_) || oldPhase != shiftPhase_)
    {
        if (game.maze.contains(oldShift)) invalidateChunksAround(oldShift);
        if (game.maze.contains(shift_)) invalidateChunksAround(shift_);
    }
    for (int slot = 0; slot < maxCoopPlayers; ++slot)
    {
        if (slot == selfSlot || !presence[slot]) { remoteDisconnected(slot); continue; }
        players[slot].moving = moving[slot] != 0; players[slot].running = running[slot] != 0; players[slot].present = true; players[slot].crouched=crouching[slot]!=0; players[slot].seated=seating[slot]!=0;
        setRemotePlayer(slot, players[slot]);
    }
    checkpoints_ = checkpoints; networkSelfSlot_ = selfSlot;
    for (std::size_t house = 0; house < doors_.size(); ++house)
        for (int side = 0; side < 4; ++side)
        {
            auto& door = doors_[house][side];
            const bool open = (doorsMask & (1u << (house * 4 + side))) != 0;
            door.rattle=replicatedDoors[house][side].rattle;
            door.open = open;
        }
    doorEvent_=doorEvent;doorSource_=doorSource;doorSlam_=doorSlam!=0;doorStrikes_=strikes;
    refugeBalls_ = std::move(balls);
    for(std::size_t house=0;house<machines.size();++house)
        if(machines[house].spins!=refugeSlots_[house].spins && machines[house].actor==selfSlot) alignSlotView(static_cast<int>(house));
    refugeSlots_ = std::move(machines);
    interiorHunter_=interior!=0; hunterX_=hx; hunterY_=hy; hunterZ_=hz;
    drinks_=drinks;
    poker_=poker;
    for(std::size_t i=0;i<caches_.size();++i)if((opened&(std::uint64_t{1}<<i)) && !(openedCaches_&(std::uint64_t{1}<<i)))cacheOpenClocks_[i]=1.2f;
    for(int i=0;i<6;++i)if(packs[selfSlot].items[i]<packs_[selfSlot].items[i])
    {adventureMotionClock_=1.2f;adventureMotionItem_=i;drinkClock_=0.f;}
    packs_=packs;adventurePanels_=panels;openedCaches_=opened;
    if(packs_[selfSlot].recalls!=seenRecall_)
    {seenRecall_=packs_[selfSlot].recalls;recallAdventurer(selfSlot,game);notice_="The Recall rune brings you back to your trading post.";messageClock_=4.f;}
    if(packs_[selfSlot].restores!=seenRestore_)
    {seenRestore_=packs_[selfSlot].restores;stamina_=1.f;fear_=std::max(0.f,fear_-.3f);notice_="Verdant draught restores your breath.";messageClock_=4.f;}
    if (drinks_[selfSlot] != seenDrink_)
    {
        seenDrink_=drinks_[selfSlot]; const int bar=nearbyBar(); if(bar>=0) sitAtBar(bar); stamina_=1.f; fear_=std::max(0.f,fear_-.15f); drinkClock_=4.f;
        notice_="You drink. The glass clinks; the hunter can hear it."; messageClock_=5.f;
    }
    monsterFrom_ = from; monster_ = monster; monsterClock_ = clock; monsterStepDuration_ = duration;
    monsterHeading_ = heading; creatureGait_ = gait; elapsed_ = elapsed; ambientTime_ = ambient;
    hunter_.mode = static_cast<HunterMode>(hunterMode);
    solved_ = solved; activeStones_ = activeStones; inscriptionRead_ = inscription != 0; gateOpen_ = gate != 0;
    gateInput_ = input == "-" ? "" : input;
    for (unsigned i = 0; i < discoveredClues_.size(); ++i) discoveredClues_[i] = (clues & (1u << i)) != 0;
    if (captures[selfSlot] != seenRemoteCaptures_)
    {
        seenRemoteCaptures_ = captures[selfSlot];
        const int house = checkpoints_[selfSlot];
        const MazePosition respawn = house >= 0 ? safehouses_[house] : MazePosition{ 0, 0 };
        seatedHouse_=-1;drinkClock_=0.f; x_ = respawn.col + .5f; z_ = respawn.row + .5f; elevation_ = 0.f;
        angle_ = open(game.maze, respawn.row + 1, respawn.col) ? 0.f : pi / 2.f; pitch_ = 0.f;
        game.player = respawn; stamina_ = 1.f; exhausted_ = false;
        notice_ = house >= 0 ? "It caught you. You wake in the safehouse." : "It caught you. You wake again at the entrance.";
        messageClock_ = 5.f;
    }
    return true;
}

void GameView::handleRemoteEvent(int slot, const sf::Event& event, GameState& game)
{
    if (slot < 0 || slot >= maxCoopPlayers || !remote_[slot].present) return;
    if(const auto* key=event.getIf<sf::Event::KeyPressed>();key && key->code==sf::Keyboard::Key::Q)
    {adventurePanels_[slot]=0;return;}
    const auto* ballKey = event.getIf<sf::Event::KeyPressed>();
    if (ballKey && (ballKey->code == sf::Keyboard::Key::G || ballKey->code == sf::Keyboard::Key::K))
    { playRefugeBall(ballKey->code == sf::Keyboard::Key::G, slot); return; }
    interactionSlot_=slot;
    const float savedStamina=stamina_, savedFear=fear_;
    const float savedElevation = elevation_;
    elevation_ = remote_[slot].elevation;
    const float savedX = x_, savedZ = z_, savedAngle = angle_;
    x_ = remote_[slot].x; z_ = remote_[slot].z; angle_ = remote_[slot].angle;
    const int stone = nearbyShrine();
    if (stone >= 0) discoveredClues_[stone] = true;
    handleEvent(event, game);
    x_ = savedX; z_ = savedZ; angle_ = savedAngle; elevation_ = savedElevation; stamina_=savedStamina; fear_=savedFear; interactionSlot_=networkSelfSlot_;
}

void GameView::updateInteriorHunter(const GameState& game,float seconds)
{
    const int house=interiorHunter_?interiorHouse_:safehouseAt(monster_,safehouses_);
    if(house<0)return;
    const auto centre=safehouses_[house];
    if(!interiorHunter_)
    {
        if(hunter_.mode==HunterMode::Patrol || hunter_.memory<=0
            || hunter_.lastKnownElevation<1.5f || safehouseAt(hunter_.lastKnown,safehouses_)!=house)return;
        const float blend=std::clamp(monsterClock_/monsterStepDuration_,0.f,1.f);
        hunterX_=monsterFrom_.col+.5f+(monster_.col-monsterFrom_.col)*blend;
        hunterZ_=monsterFrom_.row+.5f+(monster_.row-monsterFrom_.row)*blend;
        if(safehouseAt({static_cast<int>(hunterZ_),static_cast<int>(hunterX_)},safehouses_)!=house)return;
        interiorHunter_=true;interiorHouse_=house;interiorPhase_=0;hunterY_=0.f;
        interiorGoalX_=hunter_.lastKnown.col+.5f;interiorGoalZ_=hunter_.lastKnown.row+.5f;
        interiorGoalY_=hunter_.lastKnownElevation;interiorMemory_=12.f;
    }
    HunterState observer=hunter_;observer.elevation=hunterY_;
    observer.cell={static_cast<int>(hunterZ_),static_cast<int>(hunterX_)};
    int strongest=0;float nearest=1e9f;
    for(int slot=0;slot<maxCoopPlayers;++slot)
    {
        const auto player=playerAt(slot);
        if(!player.present)continue;
        const MazePosition at{static_cast<int>(player.z),static_cast<int>(player.x)};
        const HunterTarget target{at,player.moving,player.running,
            voiceNoiseTime_[slot]>0.f?std::clamp(static_cast<int>(2.f+voiceNoiseLevel_[slot]*30.f),4,12):player.jumpHeight>.01f?4:0,
            player.crouched,packs_[slot].effects[0]>0.f,player.elevation};
        const int signal=senseHunterTarget(game.maze,observer,target,safehouses_,~doorMask());
        const float distance=(player.x-hunterX_)*(player.x-hunterX_)+(player.z-hunterZ_)*(player.z-hunterZ_);
        if(signal>strongest || (signal>0 && signal==strongest && distance<nearest))
        {
            strongest=signal;nearest=distance;
            interiorGoalX_=signal==2?player.x:at.col+.5f;
            interiorGoalZ_=signal==2?player.z:at.row+.5f;interiorGoalY_=player.elevation;
            hunter_.lastKnown=at;hunter_.lastKnownElevation=player.elevation;
        }
    }
    if(strongest<2)for(const auto& pack:packs_)if(pack.echoTime>0.f)
    {
        const HunterTarget echo{pack.echo,false,false,14,false,true,0.f};
        if(senseHunterTarget(game.maze,observer,echo,safehouses_,~doorMask())>0)
        {strongest=1;interiorGoalX_=pack.echo.col+.5f;interiorGoalZ_=pack.echo.row+.5f;interiorGoalY_=0.f;hunter_.lastKnown=pack.echo;hunter_.lastKnownElevation=0.f;break;}
    }
    interiorMemory_=strongest>0?8.f:std::max(0.f,interiorMemory_-seconds);
    // Follow remembered evidence, never the live position of a hidden player.
    if(interiorPhase_==4 && (interiorMemory_<=0.f || interiorGoalY_<1.5f))interiorPhase_=5;
    const float hx=centre.col+.5f,hz=centre.row+.5f;
    float gx=hunterX_,gz=hunterZ_;
    switch(interiorPhase_)
    {
    case 0: gz=hz+.20f*refugeScale; break;
    case 1: gx=hx+.98f*refugeScale; gz=hz+.20f*refugeScale; break;
    case 2: gx=hx+.98f*refugeScale; gz=hz+1.16f*refugeScale; break;
    case 3: case 5: gx=hx+.45f*refugeScale; gz=hz+1.16f*refugeScale; break;
    case 4: gx=std::min(interiorGoalX_,hx+.60f*refugeScale); gz=interiorGoalZ_; break;
    case 6: gx=hx+.98f*refugeScale; gz=hz+1.16f*refugeScale; break;
    case 7: gx=hx+.98f*refugeScale; gz=hz+.20f*refugeScale; break;
    }
    const float dx=gx-hunterX_,dz=gz-hunterZ_,distance=std::sqrt(dx*dx+dz*dz);
    const float step=std::min(distance,seconds*.95f);
    if(distance>.001f)
    {
        hunterX_+=dx/distance*step; hunterZ_+=dz/distance*step;
        monsterTargetHeading_=std::atan2(dx,dz); creatureGait_+=seconds*7.f;
    }
    hunterY_=refugeFloorHeight(hunterX_,hunterZ_,hunterY_,safehouses_);
    hunter_.cell=monster_=monsterFrom_={static_cast<int>(hunterZ_),static_cast<int>(hunterX_)};
    hunter_.mode=strongest==2?HunterMode::Chase:HunterMode::Search;
    hunter_.elevation=hunterY_;
    if (distance<=step+.001f && interiorPhase_!=4)
    {
        if(interiorPhase_==7)
        {
            interiorHunter_=false;interiorHouse_=-1;hunterY_=hunter_.elevation=0.f;
            if(interiorMemory_<=0.f)
            {hunter_.memory=0;hunter_.mode=HunterMode::Patrol;hunter_.lastKnownElevation=0.f;}
        }
        else ++interiorPhase_;
    }
}

void GameView::updateMonster(GameState& game, float seconds, bool playerMoving, bool playerRunning)
{
    if (elapsed_ < 12.f) { monsterClock_ = 0.f; movementHeard_ = runningHeard_ = false; return; }
    directorClock_ += seconds;
    if (!interiorHunter_ && directorClock_ >= 55.f)
    {
        directorClock_ = 0.f;
        std::array<HunterTarget, maxCoopPlayers> group{};
        std::size_t count = 0; group[count++] = { game.player, playerMoving, playerRunning };
        for (int slot = 1; slot < maxCoopPlayers; ++slot)
            if (remote_[slot].present) group[count++] = { { static_cast<int>(remote_[slot].z), static_cast<int>(remote_[slot].x) },
                remote_[slot].moving, remote_[slot].running };
        if (relocateDistantHunter(game.maze, hunter_, group.data(), count, shrines_, safehouses_,
            game.mazeSeed.value_or(0), ++directorEvent_))
        {
            monster_ = monsterFrom_ = hunter_.cell; monsterClock_ = 0.f;
            monsterHeading_ = monsterTargetHeading_ = 0.f;
        }
    }
    for (auto& cooldown : remoteCaptureCooldown_) cooldown = std::max(0.f, cooldown - seconds);
    movementHeard_ = movementHeard_ || playerMoving;
    runningHeard_ = runningHeard_ || playerRunning;
    monsterClock_ += seconds;
    if (!interiorHunter_ && monsterClock_ >= monsterStepDuration_)
    {
        monsterClock_ -= monsterStepDuration_;
        monsterFrom_ = monster_;
        const auto previousMode = hunter_.mode;
        std::array<HunterTarget, maxCoopPlayers*2 + 4> targets{};
        std::size_t count = 0;
        targets[count++] = { game.player, movementHeard_ && !crouched_, runningHeard_, voiceNoiseTime_[networkSelfSlot_]>0.f?std::clamp(static_cast<int>(2.f+voiceNoiseLevel_[networkSelfSlot_]*30.f),4,12):jumpHeight_>.01f?4:0, crouched_,packs_[networkSelfSlot_].effects[0]>0.f,elevation_ };
        for (int slot = 1; slot < maxCoopPlayers; ++slot)
        {
            const auto& player = remote_[slot];
            if (player.present) targets[count++] = { { static_cast<int>(player.z), static_cast<int>(player.x) },
                player.moving, player.running, voiceNoiseTime_[slot]>0.f?std::clamp(static_cast<int>(2.f+voiceNoiseLevel_[slot]*30.f),4,12):player.jumpHeight>.01f?4:0, player.crouched,packs_[slot].effects[0]>0.f,player.elevation };
        }
        for (std::size_t house=0;house<refugeNoise_.size();++house)
            if (refugeNoise_[house]>0.f) targets[count++]={safehouses_[house],true,true,10,false,true};
        for(const auto& p:packs_) if(p.echoTime>0.f) targets[count++]={p.echo,false,false,14,false,true};
        hunter_.elevation=hunterY_;
        advanceHunterGroup(game.maze, hunter_, targets.data(), count, safehouses_,~doorMask());
        // A closed doorway delays entry. Repeated strikes force it open;
        // the route planner never cuts through the cabin walls.
        for (std::size_t house=0;house<safehouses_.size();++house)
        {
            const auto centre=safehouses_[house];
            if (insideSafehouse(monsterFrom_,centre)==insideSafehouse(hunter_.cell,centre)) continue;
            const auto outside=insideSafehouse(monsterFrom_,centre)?hunter_.cell:monsterFrom_;
            const int side=outside.col<centre.col-2?0:outside.col>centre.col+2?1:outside.row<centre.row-2?2:3;
            auto& door=doors_[house][side];
            if (door.swing < .85f)
            {
                const auto index=house*4+side;
                if (++doorStrikes_[index]>=3u) { if (!door.open) toggleDoor(static_cast<int>(house),side); doorStrikes_[index]=0; }
                door.rattle=1.f; ++doorEvent_; doorSlam_=true;
                doorSource_=static_cast<int>(index);
                hunter_.cell=monsterFrom_; hunter_.previous=monsterFrom_;
                notice_="It is forcing the door. Find another way out."; messageClock_=3.f;
            }
        }
        movementHeard_ = runningHeard_ = false;
        monster_ = hunter_.cell;
        if (!same(monster_, monsterFrom_))
            monsterTargetHeading_ = std::atan2(static_cast<float>(monster_.col - monsterFrom_.col),
                static_cast<float>(monster_.row - monsterFrom_.row));
        else if(hunter_.searchingArea && hunter_.linger>0)
            monsterTargetHeading_+=.7f; // An audible pause and a readable look around.
        monsterStepDuration_ = hunter_.mode == HunterMode::Chase ? .84f : 1.15f;
        if (hunter_.mode == HunterMode::Chase && previousMode != HunterMode::Chase)
        {
            notice_ = "It has found you."; messageClock_ = 3.f;
        }
    }
    updateInteriorHunter(game,seconds);
    const float turn = std::remainder(monsterTargetHeading_ - monsterHeading_, 2.f * pi);
    monsterHeading_ += std::clamp(turn, -seconds * 4.f, seconds * 4.f);
    if (!same(monster_, monsterFrom_)) creatureGait_ += seconds * (hunter_.mode == HunterMode::Chase ? 9.f : 6.f);
    const float blend = std::clamp(monsterClock_ / monsterStepDuration_, 0.f, 1.f);
    const float mx = interiorHunter_ ? hunterX_ : monsterFrom_.col + .5f + (monster_.col - monsterFrom_.col) * blend;
    const float mz = interiorHunter_ ? hunterZ_ : monsterFrom_.row + .5f + (monster_.row - monsterFrom_.row) * blend;
    const float dx = x_ - mx, dz = z_ - mz;
    for (int slot = 1; slot < maxCoopPlayers; ++slot)
    {
        auto& player = remote_[slot];
        const float remoteDx = player.x - mx, remoteDz = player.z - mz;
        const MazePosition playerCell{ static_cast<int>(player.z), static_cast<int>(player.x) };
        if (player.present && std::abs(player.elevation + player.jumpHeight - hunterY_) < .65f && remoteCaptureCooldown_[slot] <= 0.f &&
            remoteDx * remoteDx + remoteDz * remoteDz < .38f * .38f)
        {
            ++remoteCaptures_[slot];
            const int house = checkpoints_[slot];
            const MazePosition respawn = house >= 0 ? safehouses_[house] : MazePosition{ 0, 0 };
            player.x = respawn.col + .5f; player.z = respawn.row + .5f; remoteCaptureCooldown_[slot] = 2.f;
            notice_ = "A companion was caught."; messageClock_ = 4.f;
        }
    }
    if (std::abs(elevation_ + jumpHeight_ - hunterY_) < .65f && dx * dx + dz * dz < .38f * .38f)
    {
        const int house = checkpoints_[0];
        const MazePosition respawn = house >= 0 ? safehouses_[house] : MazePosition{ 0, 0 };
        seatedHouse_=-1;drinkClock_=0.f; x_ = respawn.col + .5f; z_ = respawn.row + .5f; elevation_ = 0.f;
        angle_ = open(game.maze, respawn.row + 1, respawn.col) ? 0.f : pi / 2.f; pitch_ = 0.f;
        game.player = respawn; gateInput_.clear();
        ++captureCount_; resetHunter(game);
        // Capture restarts the encounter clock, so preserve remaining interaction
        // durations rather than leaving cabinets/cards locked to the old epoch.
        for(auto& machine:refugeSlots_) machine.stopTime=std::max(0.f,machine.stopTime-elapsed_);
        for(auto& hand:poker_) hand.readyAt=std::max(0.f,hand.readyAt-elapsed_);
        elapsed_ = monsterClock_ = directorClock_ = 0.f; monsterStepDuration_ = 1.15f;
        monsterHeading_ = monsterTargetHeading_ = 0.f; creatureGait_ = 0.f;
        stamina_ = 1.f; exhausted_ = false; movementHeard_ = runningHeard_ = false;
        bobBlend_ = sprintBlend_ = 0.f;
        notice_ = house >= 0 ? "It caught you. You wake in the safehouse." : "It caught you. You wake again at the entrance.";
        messageClock_ = 5.f;
    }
    // The creature remembers occupied refuges. It circles them, tests closed
    // doors and slams an open one shut. Doorway routing and strikes allow entry.
    if (elapsed_ >= 12.f && messageClock_ <= 0.f)
    {
        rattleClock_ += seconds;
        if (rattleClock_ >= 6.f)
        {
            rattleClock_ = 0.f;
            for (std::size_t house = 0; house < safehouses_.size(); ++house)
            {
                // A remembered sighting/noise can justify testing a refuge.
                // Silent occupants behind closed walls do not reveal themselves.
                if(hunter_.memory<=0 || hunter_.mode==HunterMode::Patrol
                    || !insideSafehouse(hunter_.lastKnown,safehouses_[house]))continue;
                const int dr = hunter_.cell.row - safehouses_[house].row;
                const int dc = hunter_.cell.col - safehouses_[house].col;
                if (dr * dr + dc * dc > 20) continue;
                int chosen = -1;
                for (int side = 0; side < 4 && chosen < 0; ++side)
                    if (doors_[house][side].open) chosen = side;
                if (chosen < 0) chosen = static_cast<int>((game.mazeSeed.value_or(0) + house + scareCount_) % 4u);
                if (doors_[house][chosen].open)
                    toggleDoor(static_cast<int>(house), chosen, true);
                else
                {
                    auto& door = doors_[house][chosen];
                    door.rattle = 1.f;
                    ++doorEvent_; doorSlam_ = false;
                    doorSource_=static_cast<int>(house*4+chosen);
                    const auto frame = safehouseDoorFrame(safehouses_[house], chosen);
                    const float doorDx = frame.centreX - x_, doorDz = frame.centreZ - z_;
                    const float distance = std::sqrt(doorDx * doorDx + doorDz * doorDz);
                    doorPan_ = distance > .001f
                        ? std::clamp((-doorDx * std::cos(angle_) + doorDz * std::sin(angle_)) / distance, -1.f, 1.f) : 0.f;
                    notice_ = "Something tests the safehouse door. The door will not hold forever.";
                    messageClock_ = 4.f;
                }
                fear_ = std::min(1.f, fear_ + .18f);
                break;
            }
        }
    }
}

void GameView::look(float horizontalPixels, float verticalPixels)
{
    if (!initialized_) return;
    handInertia_.x=std::clamp(handInertia_.x-horizontalPixels*.00015f,-.022f,.022f);
    handInertia_.y=std::clamp(handInertia_.y+verticalPixels*.00012f,-.018f,.018f);
    angle_ -= std::clamp(horizontalPixels, -120.f, 120.f) * .0025f;
    pitch_ = std::clamp(pitch_ - std::clamp(verticalPixels, -120.f, 120.f) * .0025f, -1.f, 1.f);
}

void GameView::update(GameState& game, float seconds)
{
    const auto down = [](sf::Keyboard::Key key) { return sf::Keyboard::isKeyPressed(key); };
    GameInput input;
    input.turn = (down(sf::Keyboard::Key::Right) ? 1.f : 0.f) - (down(sf::Keyboard::Key::Left) ? 1.f : 0.f);
    input.forward = (down(sf::Keyboard::Key::W) || down(sf::Keyboard::Key::Up) ? 1.f : 0.f)
        - (down(sf::Keyboard::Key::S) || down(sf::Keyboard::Key::Down) ? 1.f : 0.f);
    input.strafe = (down(sf::Keyboard::Key::D) ? 1.f : 0.f) - (down(sf::Keyboard::Key::A) ? 1.f : 0.f);
    input.run = down(sf::Keyboard::Key::LShift);
    input.crouch = down(sf::Keyboard::Key::LControl) || down(sf::Keyboard::Key::C);
    input.jump = down(sf::Keyboard::Key::Space);
    update(game, seconds, input);
}

void GameView::update(GameState& game, float seconds, const GameInput& input)
{
    if (game.phase != GamePhase::Playing || game.maze.empty()) return;
    if (!initialized_) initialize(game);
    seconds = std::clamp(seconds, 0.f, .05f);
    for (int slot = 0; slot < maxCoopPlayers; ++slot)
    {
        const auto& player = remote_[slot]; auto& visual = remoteVisual_[slot];
        if (!player.present) continue;
        const float blend = 1.f - std::exp(-12.f * seconds);
        visual.x += (player.x - visual.x) * blend;
        visual.z += (player.z - visual.z) * blend;
        visual.elevation += (player.elevation - visual.elevation) * blend;
        visual.jumpHeight += (player.jumpHeight-visual.jumpHeight)*blend; visual.crouched=player.crouched; visual.seated=player.seated;
        visual.angle += std::remainder(player.angle - visual.angle, 2.f * pi) * blend;
        visual.moving = player.moving; visual.running = player.running;
    }
    bool held=false;
    for(std::size_t house=0;house<refugeBalls_.size();++house)if(refugeBalls_[house][1].carrier==networkSelfSlot_)
    {
        held=true;const auto& ball=refugeBalls_[house][1];
        lastHeldBall_={safehouses_[house].col+.5f+ball.x,refugeUpperFloor+ball.y,safehouses_[house].row+.5f+ball.z};
    }
    handInertia_=handInertia_*std::exp(-seconds*9.f);
    if(heldBallLast_ && !held) throwClock_=.4f;
    ballHoldBlend_+=((held?1.f:0.f)-ballHoldBlend_)*std::min(1.f,seconds*10.f);
    heldBallLast_=held; throwClock_=std::max(0.f,throwClock_-seconds);
    drinkClock_ = std::max(0.f, drinkClock_ - seconds);
    for (auto& cooldown : drinkCooldowns_) cooldown=std::max(0.f,cooldown-seconds);
    for(auto& timer:voiceNoiseTime_)timer=std::max(0.f,timer-seconds);
    for (auto& noise : refugeNoise_) noise=std::max(0.f,noise-seconds);
    elapsed_ += seconds; messageClock_ = std::max(0.f, messageClock_ - seconds);
    updateAdventure(game,seconds);
    const auto previousStorm = sampleStorm(ambientTime_, game.mazeSeed.value_or(0)).event;
    ambientTime_ += seconds;
    const auto currentStorm = sampleStorm(ambientTime_, game.mazeSeed.value_or(0)).event;
    if (currentStorm != previousStorm) ++thunderEvent_;
    angle_ -= std::clamp(input.turn, -1.f, 1.f) * 1.8f * seconds;
    if(seatedHouse_>=0 && input.jump) { seatedHouse_=-1;drinkClock_=0.f; }
    seatedBlend_ += ((seatedHouse_>=0?1.f:0.f)-seatedBlend_)*std::min(1.f,seconds*8.f);
    bool pulling=false;
    for(const auto& machine:refugeSlots_)
    { const float t=elapsed_-(machine.stopTime-2.f);pulling=pulling || (machine.actor==networkSelfSlot_ && machine.spins && t>=0.f && t<1.95f); }
    const float forward = seatedHouse_>=0 || pulling?0.f:std::clamp(input.forward, -1.f, 1.f);
    const float strafe = seatedHouse_>=0 || pulling?0.f:std::clamp(input.strafe, -1.f, 1.f);
    const float length = std::sqrt(forward * forward + strafe * strafe);
    crouched_ = input.crouch && jumpHeight_ <= .001f;
    crouchBlend_ += ((crouched_ ? 1.f : 0.f) - crouchBlend_) * std::min(1.f, seconds * 12.f);
    if (input.jump && !pulling && !jumpHeld_ && !crouched_ && jumpHeight_ <= .001f && stamina_ > .08f)
    { jumpVelocity_ = 2.6f; stamina_ = std::max(0.f, stamina_ - .08f); }
    jumpHeld_ = input.jump;
    if (jumpVelocity_ != 0.f || jumpHeight_ > 0.f)
    {
        jumpVelocity_ -= 8.f * seconds;
        jumpHeight_ = std::max(0.f, jumpHeight_ + jumpVelocity_ * seconds);
        if (jumpHeight_ <= 0.f) { jumpVelocity_ = 0.f; }
    }
    const bool running = length > 0.f && input.run && !exhausted_ && !crouched_;
    const float oldX = x_, oldZ = z_;
    if (length > 0.f)
    {
        const float speed = (crouched_ ? .65f : running ? 2.15f : 1.45f) * (packs_[networkSelfSlot_].effects[1]>0.f?1.3f:1.f) * seconds / length;
        const float dx = (std::sin(angle_) * forward - std::cos(angle_) * strafe) * speed;
        const float dz = (std::cos(angle_) * forward + std::sin(angle_) * strafe) * speed;
        if (walkable(game, x_ + dx, z_)) x_ += dx;
        if (walkable(game, x_, z_ + dz)) z_ += dz;
        MazePosition cell{ static_cast<int>(z_), static_cast<int>(x_) };
        if (!same(cell, game.player))
        {
            game.player = cell;
            if (game.statistics.successfulMoves < std::numeric_limits<std::uint64_t>::max()) ++game.statistics.successfulMoves;
            if (game.maze.at(cell.row, cell.col) == MazeCell::Exit && gateOpen_) { game.phase = GamePhase::Won; return; }
        }
    }
    elevation_ = refugeFloorHeight(x_, z_, elevation_, safehouses_);
    const MazePosition actualCell{static_cast<int>(z_),static_cast<int>(x_)};
    if(game.maze.contains(actualCell)) game.player=actualCell;
    const bool moved = std::abs(x_ - oldX) + std::abs(z_ - oldZ) > .00001f;
    const int currentHouse = safehouseAt(game.player, safehouses_);
    if (currentHouse >= 0 && checkpoints_[networkSelfSlot_] != currentHouse)
    {
        checkpoints_[networkSelfSlot_] = currentHouse;
        notice_ = "Refuge checkpoint set. Doors buy time, but the hunter can enter."; messageClock_ = 5.f;
    }
    // A nearby stone displays its engraving, so record it for the gate puzzle.
    // The journal survives a capture but is cleared when the maze is restarted.
    const int observedStone = nearbyShrine();
    if (observedStone >= 0) discoveredClues_[observedStone] = true;
    if (running && moved && packs_[networkSelfSlot_].effects[1]<=0.f)
    {
        stamina_ = std::max(0.f, stamina_ - seconds * .16f);
        if (stamina_ <= 0.f) exhausted_ = true;
    }
    else
    {
        stamina_ = std::min(1.f, stamina_ + seconds * (currentHouse >= 0 ? .22f : .10f));
        if (stamina_ >= .3f) exhausted_ = false;
    }
    const float motionTarget = moved && headMotionEnabled_ ? 1.f : 0.f;
    bobBlend_ += (motionTarget - bobBlend_) * std::min(1.f, seconds * 6.f);
    const float sprintTarget = running && moved && headMotionEnabled_ ? 1.f : 0.f;
    sprintBlend_ += (sprintTarget - sprintBlend_) * std::min(1.f, seconds * 4.f);
    if (moved) bobPhase_ += seconds * (running ? 10.f : 7.f);
    const float targetDarkness = hedgeDarkness(x_, z_, game.mazeSeed.value_or(0));
    darkness_ += (targetDarkness - darkness_) * std::min(1.f, seconds * 1.5f);
    soundMoving_ = moved && !crouched_; soundRunning_ = running && moved;
    if (networkGuest_)
    {
        monsterClock_ = std::min(monsterClock_ + seconds, monsterStepDuration_);
        if (shiftPhase_ == ShiftPhase::Opening || shiftPhase_ == ShiftPhase::Closing)
            shiftProgress_ = std::min(1.f, shiftProgress_ + seconds / 2.f);
    }
    else
    {
        updateMonster(game, seconds, moved, running && moved);
        updateShift(game, seconds);
    }
    if (!networkGuest_) updateRefugeGames(seconds);
    updateDoors(seconds);
    const auto transmission=acousticTransmission(game.maze,game.player,monster_,safehouses_,~doorMask());
    const float floorMuffle=std::abs(elevation_-hunterY_)>1.f?.65f:1.f;
    hunterSoundGain_+=(transmission.gain*floorMuffle-hunterSoundGain_)*std::min(1.f,seconds*8.f);
    hunterSoundClarity_+=(transmission.clarity*floorMuffle-hunterSoundClarity_)*std::min(1.f,seconds*8.f);
    updateFear(game, seconds);
    updateScares(game, seconds);
}

void GameView::playRefugeBall(bool basketball, int slot)
{
    if (slot < 0 || slot >= maxCoopPlayers) return;
    const auto player = slot == networkSelfSlot_ ? localPlayer() : remote_[slot];
    if (!player.present || player.elevation < refugeUpperFloor - .05f) return;
    const int house = safehouseAt({ static_cast<int>(player.z), static_cast<int>(player.x) }, safehouses_);
    if (house < 0) return;
    auto& b = refugeBalls_[house][basketball ? 1 : 0];
    const float px = player.x - (safehouses_[house].col + .5f);
    const float pz = player.z - (safehouses_[house].row + .5f);
    if (b.carrier == slot)
    {
        // Keep possession through anticipation; the host releases at the end.
        if(b.throwWindup==0.f)b.throwWindup=.28f;
        return;
    }
    const float dx = b.x - px, dz = b.z - pz;
    if (b.carrier >= 0 || dx * dx + dz * dz > .85f * .85f || b.y > .95f) return;
    if (basketball) {b.carrier = slot;b.throwWindup=0.f;}
    else { b.vx = std::sin(player.angle) * 3.6f; b.vz = std::cos(player.angle) * 3.6f; b.vy = .55f; }
}

void GameView::updateRefugeGames(float seconds)
{
    // Host owns possession, bounces and scores; guests receive one shared state.
    for (std::size_t house = 0; house < refugeBalls_.size(); ++house)
    for (int kind = 0; kind < 2; ++kind)
    {
        auto& b = refugeBalls_[house][kind];
        float simulationSeconds=seconds;
        if (b.carrier >= 0)
        {
            const auto player = b.carrier == networkSelfSlot_ ? localPlayer() : remote_[b.carrier];
            const float px = player.x - (safehouses_[house].col + .5f);
            const float pz = player.z - (safehouses_[house].row + .5f);
            if (!player.present || player.elevation < refugeUpperFloor - .05f || std::abs(px) > 2.f || std::abs(pz) > 2.f)
            { b.carrier = -1; b.throwWindup=0.f;b.vx = b.vy = b.vz = 0.f; }
            else
            {
                const bool releasing=b.throwWindup>0.f;
                const float spent=std::min(simulationSeconds,b.throwWindup);
                b.throwWindup=std::max(0.f,b.throwWindup-spent);simulationSeconds-=spent;
                const float prepare=releasing?std::sin(interactionSmooth(1.f-b.throwWindup/.28f)*pi):0.f;
                const float reach=.52f-.10f*prepare;
                b.x = std::clamp(px + std::sin(player.angle) * reach, -1.95f, .75f);
                b.z = std::clamp(pz + std::cos(player.angle) * reach, -1.95f, 1.95f);
                b.y = .54f+.07f*prepare - (player.crouched?.30f:0.f) + player.jumpHeight;
                if(!releasing || b.throwWindup>0.f)continue;
                if(b.carrier==networkSelfSlot_)
                {
                    lastHeldBall_={safehouses_[house].col+.5f+b.x,refugeUpperFloor+b.y,safehouses_[house].row+.5f+b.z};
                    throwClock_=.4f;heldBallLast_=false;
                }
                b.carrier=-1;b.vx=std::sin(player.angle)*2.4f;b.vz=std::cos(player.angle)*2.4f;b.vy=3.f;
            }
        }
        // Small fixed substeps keep hoop crossings and fast wall impacts stable.
        int steps = std::max(1, static_cast<int>(std::ceil(simulationSeconds / .008f)));
        const float dt = simulationSeconds / steps;
        for (int i = 0; i < steps; ++i)
        {
            const float previousY = b.y;
            b.vy -= 5.f * dt; b.x += b.vx * dt; b.y += b.vy * dt; b.z += b.vz * dt;
            bool scored = false;
            if (kind == 1 && previousY > 1.12f && b.y <= 1.12f && b.vy < 0.f &&
                (b.x + .55f) * (b.x + .55f) + (b.z - 1.53f) * (b.z - 1.53f) < .22f * .22f)
            { ++b.scoreA; scored = true; }
            if (kind == 0 && std::abs(b.z) > 1.92f && std::abs(b.x + .55f) < .55f && b.y < .45f)
            { if (b.z > 0.f) ++b.scoreA; else ++b.scoreB; scored = true; }
            if (scored)
            {
                b.x = -.55f; b.y = .13f; b.z = 0.f; b.vx = b.vy = b.vz = 0.f;
                notice_ = kind ? "BASKET! Upstairs hoop score: " + std::to_string(b.scoreA)
                    : "GOAL! Upstairs football: " + std::to_string(b.scoreA) + " - " + std::to_string(b.scoreB);
                messageClock_ = 4.f; break;
            }
            if (b.y < .13f) { b.y = .13f; b.vy = std::abs(b.vy) > .25f ? std::abs(b.vy) * .65f : 0.f; }
            if (b.y > 1.55f) { b.y = 1.55f; b.vy = -std::abs(b.vy) * .5f; }
            if (b.x < -2.f || b.x > .82f) { b.x = std::clamp(b.x, -2.f, .82f); b.vx *= -.72f; }
            if (std::abs(b.z) > 2.f) { b.z = std::clamp(b.z, -2.f, 2.f); b.vz *= -.72f; }
            const float drag = std::exp(-(b.y <= .131f ? 1.5f : .12f) * dt);
            b.vx *= drag; b.vz *= drag;
        }
    }
}

void GameView::updateFear(GameState& game, float seconds)
{
    float threat = 0.f;
    if (elapsed_ >= 12.f)
    {
        const float blend = std::clamp(monsterClock_ / monsterStepDuration_, 0.f, 1.f);
        const float mx = interiorHunter_ ? hunterX_ : monsterFrom_.col + .5f + (monster_.col - monsterFrom_.col) * blend;
        const float mz = interiorHunter_ ? hunterZ_ : monsterFrom_.row + .5f + (monster_.row - monsterFrom_.row) * blend;
        const float dx = x_ - mx, dz = z_ - mz;
        threat = std::clamp(1.f - std::sqrt(dx * dx + dz * dz) / 11.f, 0.f, 1.f);
    }
    const bool sheltered = safehouseAt(game.player, safehouses_) >= 0;
    const float target = std::clamp(threat * .66f + darkness_ * .22f
        + (hunter_.mode == HunterMode::Chase ? .12f : 0.f) - (sheltered ? .23f : 0.f), 0.f, 1.f);
    fear_ += (target - fear_) * std::min(1.f, seconds * (sheltered ? .9f : 1.6f));
}

void GameView::updateScares(GameState& game, float seconds)
{
    if (safehouseAt(game.player, safehouses_) >= 0) { scareClock_ = 0.f; return; }
    scareClock_ += seconds;
    const std::uint32_t seed = game.mazeSeed.value_or(0);
    const float interval = 20.f + static_cast<float>((seed ^ (scareCount_ * 2654435761u)) % 15u);
    if (scareClock_ < interval) return;
    scareClock_ = 0.f;
    ++scareCount_;
    const unsigned event = (seed ^ (scareCount_ * 0x9e3779b9u)) % 4u;
    whisperPan_ = static_cast<float>(static_cast<int>((seed >> 5) % 201u) - 100) / 100.f;
    ++whisperEvent_;
    if (event == 0) notice_ = "A whisper coils through the hedge. It knows your name.";
    else if (event == 1) notice_ = "Somewhere in the dark, a door slams.";
    else if (event == 2) notice_ = "The hedge leans closer. Something is breathing.";
    else notice_ = "You hear wet footsteps keeping pace with you.";
    fear_ = std::min(1.f, fear_ + .26f);
    messageClock_ = 4.f;
}

void GameView::updateShift(GameState& game, float seconds)
{
    constexpr float transitionSeconds = 2.f;
    if (shiftPhase_ == ShiftPhase::Opening || shiftPhase_ == ShiftPhase::Closing)
    {
        shiftProgress_ = std::min(1.f, shiftProgress_ + seconds / transitionSeconds);
        if (shiftProgress_ < 1.f) return;
        if (shiftPhase_ == ShiftPhase::Opening)
        {
            game.maze.at(shift_.row, shift_.col) = MazeCell::Passage;
            shiftPhase_ = ShiftPhase::Open; shiftClock_ = 0.f;
            shiftDelay_ = 8.f + static_cast<float>((game.mazeSeed.value_or(0) ^ (shiftCount_ * 2246822519u)) % 21u);
            invalidateChunksAround(shift_);
        }
        else
        {
            const auto finished = shift_;
            shift_ = { -1, -1 }; shiftPhase_ = ShiftPhase::Idle; shiftClock_ = 0.f;
            shiftDelay_ = 10.f + static_cast<float>((game.mazeSeed.value_or(0) ^ (shiftCount_ * 3266489917u)) % 26u);
            invalidateChunksAround(finished);
        }
        return;
    }
    shiftClock_ += seconds;
    if (shiftClock_ < shiftDelay_) return;
    if (shiftPhase_ == ShiftPhase::Open)
    {
        const bool playerOverlaps = x_ + .17f > shift_.col && x_ - .17f < shift_.col + 1.f
            && z_ + .17f > shift_.row && z_ - .17f < shift_.row + 1.f;
        bool remoteOverlaps = false;
        for (const auto& player : remote_)
            remoteOverlaps = remoteOverlaps || (player.present && player.x + .17f > shift_.col && player.x - .17f < shift_.col + 1.f
                && player.z + .17f > shift_.row && player.z - .17f < shift_.row + 1.f);
        if (playerOverlaps || remoteOverlaps || same(shift_, game.player) || same(shift_, monster_) || same(shift_, monsterFrom_)) return;
        game.maze.at(shift_.row, shift_.col) = MazeCell::Wall;
        shiftPhase_ = ShiftPhase::Closing; shiftProgress_ = 0.f;
        invalidateChunksAround(shift_);
        notice_ = "The hedge is closing."; messageClock_ = 3.f;
        return;
    }
    std::vector<MazePosition> candidates, nearbyCandidates;
    const auto size = game.maze.size();
    for (int row = 1; row < size.rows - 1; ++row) for (int col = 1; col < size.cols - 1; ++col)
        if (game.maze.at(row, col) == MazeCell::Wall &&
            ((open(game.maze,row-1,col) && open(game.maze,row+1,col)) ||
             (open(game.maze,row,col-1) && open(game.maze,row,col+1))))
        {
            const MazePosition cell{ row, col };
            if(storyRuinReserved(cell,storyRuin_))continue;
            candidates.push_back(cell);
            int distance = std::abs(row - game.player.row) + std::abs(col - game.player.col);
            for (const auto& player : remote_) if (player.present)
                distance = std::min(distance, std::abs(row - static_cast<int>(player.z))
                    + std::abs(col - static_cast<int>(player.x)));
            // Make changes perceptible, but never begin one directly beside a player.
            if (distance >= 3 && distance <= 12) nearbyCandidates.push_back(cell);
        }
    if (candidates.empty()) { shiftClock_ = 0.f; return; }
    const auto& choices = nearbyCandidates.empty() ? candidates : nearbyCandidates;
    const auto index = (game.mazeSeed.value_or(0) + ++shiftCount_ * 2654435761u) % choices.size();
    shift_ = choices[index]; shiftPhase_ = ShiftPhase::Opening; shiftProgress_ = 0.f;
    invalidateChunksAround(shift_);
    notice_ = "Somewhere in the hedge, branches move."; messageClock_ = 3.f;
}

void GameView::invalidateChunksAround(MazePosition cell)
{
    constexpr int chunkSize = 8;
    for (int row = cell.row - 1; row <= cell.row + 1; ++row)
        for (int col = cell.col - 1; col <= cell.col + 1; ++col)
        {
            if (row < 0 || col < 0) continue;
            const int cr = row / chunkSize, cc = col / chunkSize;
            if (cr < chunkRows_ && cc < chunkCols_)
                dirtyChunks_[static_cast<std::size_t>(cr * chunkCols_ + cc)] = true;
        }
}

void GameView::prepareChunks(const GameState& game)
{
    constexpr int chunkSize = 8;
    const auto size = game.maze.size();
    const int rows = (size.rows + chunkSize - 1) / chunkSize;
    const int cols = (size.cols + chunkSize - 1) / chunkSize;
    if (!clearChunks_ && rows == chunkRows_ && cols == chunkCols_) return;
    for (const auto list : hedgeChunks_) if (list) glDeleteLists(list, 1);
    for (const auto list : groundChunks_) if (list) glDeleteLists(list, 2);
    for (const auto list : foliageChunks_) if (list) glDeleteLists(list, 1);
    chunkRows_ = rows; chunkCols_ = cols;
    hedgeChunks_.assign(static_cast<std::size_t>(rows * cols), 0);
    groundChunks_.assign(hedgeChunks_.size(), 0);
    foliageChunks_.assign(hedgeChunks_.size(), 0);
    dirtyChunks_.assign(hedgeChunks_.size(), true);
    visibleChunks_.assign(hedgeChunks_.size(), true);
    clearChunks_ = false;
}

void GameView::updateChunkVisibility(const GameState& game)
{
    prepareChunks(game);
    GLfloat projection[16], modelView[16], clip[16]{};
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
    // OpenGL matrices are column-major; clip = projection * model-view.
    for (int col = 0; col < 4; ++col) for (int row = 0; row < 4; ++row)
        for (int k = 0; k < 4; ++k)
            clip[col * 4 + row] += projection[k * 4 + row] * modelView[col * 4 + k];
    float planes[6][4]{};
    for (int component = 0; component < 4; ++component)
    {
        const int offset = component * 4;
        for (int axis = 0; axis < 3; ++axis)
        {
            planes[axis * 2][component] = clip[offset + 3] + clip[offset + axis];
            planes[axis * 2 + 1][component] = clip[offset + 3] - clip[offset + axis];
        }
    }
    const auto size = game.maze.size();
    constexpr int chunkSize = 8;
    // The extra margin includes boundary hedge boxes and wind-blown foliage.
    constexpr float margin = 1.25f;
    for (int cr = 0; cr < chunkRows_; ++cr) for (int cc = 0; cc < chunkCols_; ++cc)
    {
        const float minX = cc * chunkSize - margin;
        const float maxX = std::min(size.cols, (cc + 1) * chunkSize) + margin;
        const float minZ = cr * chunkSize - margin;
        const float maxZ = std::min(size.rows, (cr + 1) * chunkSize) + margin;
        bool visible = true;
        for (const auto& p : planes)
        {
            const float x = p[0] >= 0.f ? maxX : minX;
            const float y = p[1] >= 0.f ? 2.5f : -.1f;
            const float z = p[2] >= 0.f ? maxZ : minZ;
            if (p[0] * x + p[1] * y + p[2] * z + p[3] < 0.f)
            { visible = false; break; }
        }
        visibleChunks_[static_cast<std::size_t>(cr * chunkCols_ + cc)] = visible;
    }
}

void GameView::drawGroundChunks(const GameState& game, int r0, int r1, int c0, int c1)
{
    constexpr int chunkSize = 8;
    prepareChunks(game);
    const auto size = game.maze.size();
    for (int cr = r0 / chunkSize; cr <= r1 / chunkSize; ++cr)
        for (int cc = c0 / chunkSize; cc <= c1 / chunkSize; ++cc)
        {
            const auto index = static_cast<std::size_t>(cr * chunkCols_ + cc);
            if (!visibleChunks_[index]) continue;
            if (!groundChunks_[index])
            {
                groundChunks_[index] = glGenLists(2);
                if (!groundChunks_[index]) continue;
                // Partition the existing ground quads by material; no new model.
                for(int floorPass=0;floorPass<2;++floorPass)
                {
                    glNewList(groundChunks_[index]+floorPass,GL_COMPILE);glBegin(GL_QUADS);
                    const int lastRow=std::min(size.rows,(cr+1)*chunkSize);
                    const int lastCol=std::min(size.cols,(cc+1)*chunkSize);
                    for(int row=cr*chunkSize;row<lastRow;++row)for(int col=cc*chunkSize;col<lastCol;++col)
                    {
                        const bool timber=safehouseAt({row,col},safehouses_)>=0;
                        if(timber!=(floorPass==1))continue;
                        const unsigned grain=(static_cast<unsigned>(row)*92821u^static_cast<unsigned>(col)*68917u)%15u;
                        const float shade=grain/255.f;
                        const float districtTint[4][3]={{.94f,1.f,.93f},{1.f,.89f,.77f},{.79f,.96f,1.f},{.91f,.84f,.96f}};
                        const auto& tint=districtTint[storyDistrict({row,col},size)];
                        glColor3f((.85f+shade)*(timber?1.f:tint[0]),(.85f+shade)*(timber?1.f:tint[1]),(.82f+shade)*(timber?1.f:tint[2]));
                        subdividedQuad(static_cast<float>(col),0,static_cast<float>(row),static_cast<float>(col+1),0,static_cast<float>(row),
                            static_cast<float>(col+1),0,static_cast<float>(row+1),static_cast<float>(col),0,static_cast<float>(row+1),0,1,0,2);
                    }
                    glEnd();glEndList();
                }
            }
            glCallList(groundChunks_[index]);
            if(worldShader_)
            {
                worldShader_->setUniform("materialMaps",woodMapsReady_);worldShader_->setUniform("wetness",0.f);
                worldShader_->setUniform("footprintsEnabled",false);
                worldShader_->setUniform("refugeLighting",safehouseAt(game.player,safehouses_)>=0?1.f:0.f);
                if(woodMapsReady_){worldShader_->setUniform("normalMap",woodNormal_);worldShader_->setUniform("materialMap",woodMaterial_);}
                sf::Shader::bind(worldShader_.get());
            }
            glBindTexture(GL_TEXTURE_2D,shelterTexture_?shelterTexture_:groundTexture_);
            glCallList(groundChunks_[index]+1);
            if(worldShader_)
            {
                worldShader_->setUniform("materialMaps",groundMapsReady_);worldShader_->setUniform("wetness",.65f);
                worldShader_->setUniform("footprintsEnabled",footprintsReady_);
                worldShader_->setUniform("refugeLighting",0.f);
                if(groundMapsReady_){worldShader_->setUniform("normalMap",groundNormal_);worldShader_->setUniform("materialMap",groundMaterial_);}
                sf::Shader::bind(worldShader_.get());
            }
            glBindTexture(GL_TEXTURE_2D,groundTexture_);
        }
}

void GameView::drawHedgeChunks(const GameState& game, int r0, int r1, int c0, int c1)
{
    constexpr int chunkSize = 8;
    prepareChunks(game);
    const auto size = game.maze.size();
    const int cols = chunkCols_;
    if (!hedgeTexture_) hedgeTexture_ = makeHedgeTexture();
    if (!foliageTextureAttempted_)
    {
        foliageTexture_ = loadSurfaceTexture(executableDirectory() / "assets" / "textures" / "foliage.png");
        foliageTextureAttempted_ = true;
    }
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, hedgeTexture_);
    for (int cr = r0 / chunkSize; cr <= r1 / chunkSize; ++cr)
        for (int cc = c0 / chunkSize; cc <= c1 / chunkSize; ++cc)
        {
            const auto index = static_cast<std::size_t>(cr * cols + cc);
            if (!visibleChunks_[index]) continue;
            if (!hedgeChunks_[index]) hedgeChunks_[index] = glGenLists(1);
            if (!hedgeChunks_[index]) continue;
            if (dirtyChunks_[index])
            {
                glNewList(hedgeChunks_[index], GL_COMPILE);
                glBegin(GL_QUADS);
                const int lastRow = std::min(size.rows, (cr + 1) * chunkSize);
                const int lastCol = std::min(size.cols, (cc + 1) * chunkSize);
                for (int row = cr * chunkSize; row < lastRow; ++row)
                    for (int col = cc * chunkSize; col < lastCol; ++col)
                    {
                        const unsigned variation = static_cast<unsigned>(row) * 73856093u
                            ^ static_cast<unsigned>(col) * 19349663u ^ game.mazeSeed.value_or(0);
                        const float shade = static_cast<float>(variation % 24) / 255.f;
                        glColor3f(.75f + shade, .75f + shade, .70f + shade);
                        const bool movingWall = same({row, col}, shift_) &&
                            (shiftPhase_ == ShiftPhase::Opening || shiftPhase_ == ShiftPhase::Closing);
                        const auto visiblePassage = [&](int r, int c)
                        {
                            return open(game.maze, r, c) || (same({r, c}, shift_) &&
                                (shiftPhase_ == ShiftPhase::Opening || shiftPhase_ == ShiftPhase::Closing));
                        };
                        if(storyRuinBlocked({row,col},storyRuin_))continue;
                        if (game.maze.at(row, col) == MazeCell::Wall && !movingWall)
                        {
                            const float l = static_cast<float>(col), r = l + 1.f;
                            const float n = static_cast<float>(row), f = n + 1.f;
                            const float h = 1.8f + static_cast<float>(variation % 5) * .035f;
                            if (visiblePassage(row - 1, col)) subdividedQuad(l,0,n,r,0,n,r,h,n,l,h,n,0,0,-1,4);
                            if (visiblePassage(row + 1, col)) subdividedQuad(r,0,f,l,0,f,l,h,f,r,h,f,0,0,1,4);
                            if (visiblePassage(row, col - 1)) subdividedQuad(l,0,f,l,0,n,l,h,n,l,h,f,-1,0,0,4);
                            if (visiblePassage(row, col + 1)) subdividedQuad(r,0,n,r,0,f,r,h,f,r,h,n,1,0,0,4);
                            quad(l,h,n,r,h,n,r,h,f,l,h,f,0,1,0);
                            glColor3f(.85f + shade, .85f + shade, .80f + shade);
                            quad(l+.12f,1.95f,n+.12f,r-.12f,1.95f,n+.12f,
                                 r-.12f,1.95f,f-.12f,l+.12f,1.95f,f-.12f,0,1,0);
                        }
                        else if (!movingWall)
                        {
                            // Enclose passages at the grid edge without changing collision data.
                            if (row == 0) emitBox(col + .5f, -.5f, .5f, 1.9f);
                            if (row == size.rows - 1) emitBox(col + .5f, size.rows + .5f, .5f, 1.9f);
                            if (col == 0) emitBox(-.5f, row + .5f, .5f, 1.9f);
                            if (col == size.cols - 1) emitBox(size.cols + .5f, row + .5f, .5f, 1.9f);
                        }
                    }
                glEnd();
                glEndList();
                if (foliageTexture_ && !foliageChunks_[index]) foliageChunks_[index] = glGenLists(1);
                if (foliageChunks_[index])
                {
                    glNewList(foliageChunks_[index], GL_COMPILE); glBegin(GL_QUADS);
                    for (int row = cr * chunkSize; row < lastRow; ++row)
                        for (int col = cc * chunkSize; col < lastCol; ++col)
                        {
                            if (storyRuinBlocked({row,col},storyRuin_) || game.maze.at(row, col) != MazeCell::Wall ||
                                (same({row, col}, shift_) && (shiftPhase_ == ShiftPhase::Opening || shiftPhase_ == ShiftPhase::Closing))) continue;
                            const unsigned variation = static_cast<unsigned>(row) * 73856093u
                                ^ static_cast<unsigned>(col) * 19349663u ^ game.mazeSeed.value_or(0);
                            const float shade = static_cast<float>(variation % 20u) / 255.f;
                            glColor3f(.78f + shade, .80f + shade, .74f + shade);
                            const auto exposed = [&](int r, int c)
                            {
                                return open(game.maze, r, c) || (same({r,c}, shift_) &&
                                    (shiftPhase_ == ShiftPhase::Opening || shiftPhase_ == ShiftPhase::Closing));
                            };
                            const auto face = [&](float x, float z, float nx, float nz, unsigned salt)
                            {
                                for (int n = 0; n < 4; ++n)
                                {
                                    const unsigned h = variation ^ (salt * 0x9e3779b9u) ^ (static_cast<unsigned>(n) * 0x85ebca6bu);
                                    const float along = (n % 2 == 0 ? -.23f : .23f) + static_cast<float>(h % 7u) * .01f;
                                    const float y = .12f + n * .51f + static_cast<float>((h >> 4) % 5u) * .015f;
                                    const float slant = (h & 1u) ? .10f : -.10f;
                                    foliageCard(x, z, nx, nz, along, y, .55f, .48f, slant);
                                }
                            };
                            if (exposed(row-1,col)) face(col+.5f, static_cast<float>(row), 0.f,-1.f,1u);
                            if (exposed(row+1,col)) face(col+.5f, row+1.f, 0.f,1.f,2u);
                            if (exposed(row,col-1)) face(static_cast<float>(col), row+.5f, -1.f,0.f,3u);
                            if (exposed(row,col+1)) face(col+1.f, row+.5f, 1.f,0.f,4u);
                        }
                    glEnd(); glEndList();
                }
                dirtyChunks_[index] = false;
            }
            glCallList(hedgeChunks_[index]);
        }
    glDisable(GL_TEXTURE_2D);
}

void GameView::drawFoliageChunks(const GameState& game, int r0, int r1, int c0, int c1)
{
    if (!foliageTexture_) return;
    constexpr int chunkSize = 8;
    prepareChunks(game);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, foliageTexture_);
    glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, .4f);
    for (int cr = r0 / chunkSize; cr <= r1 / chunkSize; ++cr)
        for (int cc = c0 / chunkSize; cc <= c1 / chunkSize; ++cc)
        {
            const auto index = static_cast<std::size_t>(cr * chunkCols_ + cc);
            if (!visibleChunks_[index]) continue;
            const auto list = foliageChunks_[index];
            if (list) glCallList(list);
        }
    glDisable(GL_ALPHA_TEST); glDisable(GL_TEXTURE_2D);
}

SoundscapeFrame GameView::soundFrame() const
{
    SoundscapeFrame frame;
    frame.active = initialized_; frame.moving = soundMoving_; frame.running = soundRunning_;
    const float hushPhase=std::fmod(ambientTime_,59.f);
    frame.ambienceHush=hunter_.mode==HunterMode::Chase?0.f:std::clamp(1.f-std::abs(hushPhase-22.f)/2.f,0.f,1.f);
    frame.hunterGain=hunterSoundGain_; frame.hunterClarity=hunterSoundClarity_;
    frame.thunderEvent = thunderEvent_;
    float nearestSlot=64.f;
    for (std::size_t house=0;house<safehouses_.size();++house)
    {
        const float dx=safehouses_[house].col+.5f-.85f*refugeScale-x_;
        const float dz=safehouses_[house].row+.5f-.85f*refugeScale-z_;
        const float distance=dx*dx+dz*dz;
        if (distance>=nearestSlot) continue;
        nearestSlot=distance;
        const auto& machine=refugeSlots_[house];
        frame.slotEvent=machine.spins;
        frame.slotSpinning=elapsed_<machine.stopTime;
        frame.slotJackpot=machine.spins && machine.symbols[0]==machine.symbols[1] && machine.symbols[1]==machine.symbols[2];
        frame.slotPan=distance>.001f?std::clamp((-dx*std::cos(angle_)+dz*std::sin(angle_))/std::sqrt(distance),-1.f,1.f):0.f;
    }
    for (int slot=0;slot<maxCoopPlayers;++slot)
    {
        const auto player=slot==networkSelfSlot_?localPlayer():remote_[slot];
        frame.pokerEvent+=poker_[slot].rounds*2u-(poker_[slot].phase==1?1u:0u);
        if(player.present && poker_[slot].phase && elapsed_<poker_[slot].readyAt)
        {
            const float dx=player.x-x_,dz=player.z-z_,distance=std::sqrt(dx*dx+dz*dz);
            const float gain=std::clamp(1.f-distance/8.f,0.f,1.f);
            if(gain>frame.pokerGain)
            {frame.pokerGain=gain;frame.pokerPan=distance>.01f?std::clamp((-dx*std::cos(angle_)+dz*std::sin(angle_))/distance,-1.f,1.f):0.f;}
        }
        if (player.present && (player.x-x_)*(player.x-x_)+(player.z-z_)*(player.z-z_)<64.f) frame.drinkEvent+=drinks_[slot];
    }
    frame.fear = fear_;
    frame.doorEvent = doorEvent_; frame.doorPan = doorPan_; frame.doorSlam = doorSlam_;
    if(doorSource_>=0 && doorSource_<static_cast<int>(doors_.size()*4))
    {
        const auto source=safehouseDoorFrame(safehouses_[doorSource_/4],doorSource_%4);
        const float dx=source.centreX-x_,dz=source.centreZ-z_,distance=std::sqrt(dx*dx+dz*dz);
        frame.doorPan=distance>.01f?std::clamp((-dx*std::cos(angle_)+dz*std::sin(angle_))/distance,-1.f,1.f):0.f;
        frame.doorGain=std::clamp(1.f-distance/14.f,0.f,1.f)*(elevation_>1.f?.65f:1.f);
    }
    frame.whisperEvent = whisperEvent_; frame.whisperPan = whisperPan_;
    float nearestEcho=100.f;
    for(const auto& pack:packs_)if(pack.echoTime>0.f)
    {
        const float dx=pack.echo.col+.5f-x_,dz=pack.echo.row+.5f-z_,distance=dx*dx+dz*dz;
        if(distance<nearestEcho)
        {nearestEcho=distance;frame.whisperEvent=whisperEvent_+pack.echoes;frame.whisperPan=distance>.001f?std::clamp((-dx*std::cos(angle_)+dz*std::sin(angle_))/std::sqrt(distance),-1.f,1.f):0.f;}
    }
    if (elapsed_ >= 12.f)
    {
        const float blend = std::clamp(monsterClock_ / monsterStepDuration_, 0.f, 1.f);
        const float mx = interiorHunter_ ? hunterX_ : monsterFrom_.col + .5f + (monster_.col - monsterFrom_.col) * blend;
        const float mz = interiorHunter_ ? hunterZ_ : monsterFrom_.row + .5f + (monster_.row - monsterFrom_.row) * blend;
        const float dx = mx - x_, dz = mz - z_;
        const float distance = std::sqrt(dx*dx + dz*dz);
        frame.threat = std::clamp(1.f - distance / 11.f, 0.f, 1.f);
        if (distance > .001f) frame.hunterPan = std::clamp((-dx * std::cos(angle_) + dz * std::sin(angle_)) / distance, -1.f, 1.f);
        frame.hunterMoving = !same(monster_, monsterFrom_);
        frame.hunterChasing = hunter_.mode == HunterMode::Chase;
        frame.hunterStepSeconds = monsterStepDuration_;
    }
    if (shiftPhase_ == ShiftPhase::Opening || shiftPhase_ == ShiftPhase::Closing)
    {
        const float dx = shift_.col + .5f - x_, dz = shift_.row + .5f - z_;
        const float distance = std::sqrt(dx*dx + dz*dz);
        frame.shifting = std::clamp(1.f - distance / 8.f, 0.f, 1.f);
        if (distance > .001f) frame.shiftingPan = std::clamp((-dx * std::cos(angle_) + dz * std::sin(angle_)) / distance, -1.f, 1.f);
    }
    return frame;
}

void GameView::setSoundAvailability(int status) noexcept { soundStatus_ = status; }

void GameView::toggleHeadMotion()
{
    headMotionEnabled_ = !headMotionEnabled_;
    if (!headMotionEnabled_) bobBlend_ = sprintBlend_ = 0.f;
    notice_ = headMotionEnabled_ ? "Head motion on." : "Head motion off.";
    messageClock_ = 2.f;
}

void GameView::toggleRayTracing()
{
    rayTraceEnabled_ = !rayTraceEnabled_;
    notice_ = postReady_ ? (rayTraceEnabled_ ? "Ray-traced lighting on." : "Ray-traced lighting off.")
        : "Ray-traced lighting is unavailable on this display driver.";
    messageClock_ = 2.5f;
}

void GameView::setSoundMuted(bool muted)
{
    soundMuted_ = muted;
    notice_ = muted ? "Sound muted." : "Sound on.";
    messageClock_ = 2.f;
}

void GameView::drawDoors(const GameState& game)
{
    (void)game;
    if (doors_.empty()) return;
    const bool textured = shelterTexture_ != 0;
    if (textured)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, shelterTexture_);
    }
    for (std::size_t house = 0; house < doors_.size(); ++house)
    {
        const float hx = safehouses_[house].col + .5f, hz = safehouses_[house].row + .5f;
        if ((hx - x_) * (hx - x_) + (hz - z_) * (hz - z_) > 144.f) continue;
        for (int side = 0; side < 4; ++side)
        {
            const auto& door = doors_[house][side];
            const auto frame = safehouseDoorFrame(safehouses_[house], side);
            const float rattle = door.rattle > 0.f
                ? std::sin(ambientTime_ * 43.f + side * 1.7f) * 4.5f * door.rattle : 0.f;
            glPushMatrix();
            glTranslatef(frame.hingeX, 0.f, frame.hingeZ);
            glRotatef(frame.openAngle * door.swing + rattle, 0.f, 1.f, 0.f);
            if (textured) glColor3f(.74f, .70f, .62f); else glColor3f(.19f, .16f, .12f);
            if (frame.spansZ)
                part(0.f, 0.f, safehouseDoorWidth * .5f, .075f, 1.64f, safehouseDoorWidth);
            else
                part(safehouseDoorWidth * .5f, 0.f, 0.f, safehouseDoorWidth, 1.64f, .075f);
            glDisable(GL_TEXTURE_2D);
            // A worn iron handle catches the lantern light.
            glColor3f(.34f, .27f, .16f);
            if (frame.spansZ) part(0.f, .92f, safehouseDoorWidth * .82f, .115f, .065f, .06f);
            else part(safehouseDoorWidth * .82f, .92f, 0.f, .06f, .065f, .115f);
            if (textured) glEnable(GL_TEXTURE_2D);
            glPopMatrix();
        }
    }
    glDisable(GL_TEXTURE_2D);
}

bool GameView::preparePostProcessing()
{
    postReady_ = false;
    if (!postShader_) return false;
    auto& api = framebufferApi();
    if (!api.genFramebuffers) return false;
    const auto size = viewport_;
    if (size.x == 0 || size.y == 0) return false;
    if (sceneFbo_ && postSize_ == size) { postReady_ = true; return true; }
    releasePostTargets();
    if (!sceneColor_.resize(size)) return false;
    sceneColor_.setSmooth(true);
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    api.activeTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenTextures(1, &sceneDepth_);
    if (!sceneDepth_) return false;
    glBindTexture(GL_TEXTURE_2D, sceneDepth_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(size.x),
        static_cast<GLsizei>(size.y), 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    api.genFramebuffers(1, &sceneFbo_);
    if (!sceneFbo_)
    {
        glDeleteTextures(1, &sceneDepth_); sceneDepth_ = 0;
        return false;
    }
    api.bindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    api.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
        sceneColor_.getNativeHandle(), 0);
    api.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepth_, 0);
    if (api.checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        api.bindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
        releasePostTargets();
        return false;
    }
    api.bindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
    postSize_ = size;
    postReady_ = true;
    return true;
}

void GameView::releasePostTargets()
{
    auto& api = framebufferApi();
    if (sceneFbo_ && api.deleteFramebuffers) api.deleteFramebuffers(1, &sceneFbo_);
    sceneFbo_ = 0;
    if (sceneDepth_) { glDeleteTextures(1, &sceneDepth_); sceneDepth_ = 0; }
    postSize_ = {};
    postReady_ = false;
}

void GameView::drawPostProcess(const GameState& game, float flash)
{
    if (!postReady_ || !postShader_) return;
    auto& api = framebufferApi();
    const int width = static_cast<int>(viewport_.x), height = static_cast<int>(viewport_.y);
    postShader_->setUniform("scene", 0);
    postShader_->setUniform("depthTex", 1);
    postShader_->setUniform("resolution", sf::Glsl::Vec2(static_cast<float>(width), static_cast<float>(height)));
    postShader_->setUniform("time", ambientTime_);
    postShader_->setUniform("fear", fear_);
    postShader_->setUniform("threat", soundFrame().threat);
    postShader_->setUniform("flash", flash);
    postShader_->setUniform("darkness", darkness_);
    postShader_->setUniform("rayTrace", rayTraceEnabled_ ? 1.f : 0.f);
    postShader_->setUniform("nearPlane", .05f);
    postShader_->setUniform("farPlane", 24.f);
    postShader_->setUniform("tanHalfFov", .65f*(1.f+.035f*sprintBlend_));
    postShader_->setUniform("aspect", height > 0 ? static_cast<float>(width) / height : 1.f);
    postShader_->setUniform("seed", static_cast<float>(game.mazeSeed.value_or(0) % 997u));
    GLfloat view[16];glGetFloatv(GL_MODELVIEW_MATRIX,view);
    const float toWorld[]={view[0],view[4],view[8],view[1],view[5],view[9],view[2],view[6],view[10]};
    const float toCamera[]={view[0],view[1],view[2],view[4],view[5],view[6],view[8],view[9],view[10]};
    postShader_->setUniform("cameraToWorld",sf::Glsl::Mat3(toWorld));
    postShader_->setUniform("worldToCamera",sf::Glsl::Mat3(toCamera));
    postShader_->setUniform("eyeWorld",sf::Glsl::Vec3(
        -(view[0]*view[12]+view[1]*view[13]+view[2]*view[14]),
        -(view[4]*view[12]+view[5]*view[13]+view[6]*view[14]),
        -(view[8]*view[12]+view[9]*view[13]+view[10]*view[14])));
    postShader_->setUniform("groundMaps",groundMapsReady_?1.f:0.f);
    postShader_->setUniform("groundMaterial",2);
    postShader_->setUniform("groundNormal",3);
    sf::Glsl::Vec2 refugeCenters[4]={{-100.f,-100.f},{-100.f,-100.f},{-100.f,-100.f},{-100.f,-100.f}};
    for(std::size_t i=0;i<std::min(safehouses_.size(),std::size_t{4});++i)
        refugeCenters[i]={safehouses_[i].col+.5f,safehouses_[i].row+.5f};
    postShader_->setUniformArray("refugeCenters",refugeCenters,4);

    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_FOG);
    glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST);
    glDepthMask(GL_FALSE);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    api.activeTexture(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE); glPushMatrix(); glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, sceneColor_.getNativeHandle());
    api.activeTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, sceneDepth_);
    api.activeTexture(GL_TEXTURE0+2);glBindTexture(GL_TEXTURE_2D,groundMaterial_.getNativeHandle());
    api.activeTexture(GL_TEXTURE0+3);glBindTexture(GL_TEXTURE_2D,groundNormal_.getNativeHandle());
    sf::Shader::bind(postShader_.get());
    glColor4f(1.f, 1.f, 1.f, 1.f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.f, 0.f); glVertex3f(0.f, 0.f, 0.f);
    glTexCoord2f(1.f, 0.f); glVertex3f(1.f, 0.f, 0.f);
    glTexCoord2f(1.f, 1.f); glVertex3f(1.f, 1.f, 0.f);
    glTexCoord2f(0.f, 1.f); glVertex3f(0.f, 1.f, 0.f);
    glEnd();
    sf::Shader::bind(nullptr);
    api.activeTexture(GL_TEXTURE0+3);glBindTexture(GL_TEXTURE_2D,0);
    api.activeTexture(GL_TEXTURE0+2);glBindTexture(GL_TEXTURE_2D,0);
    api.activeTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    api.activeTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_TEXTURE); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

int GameView::nearbyStorySite()const noexcept
{
    if(elevation_>.3f)return -1;
    for(int i=0;i<4;++i)
    {
        const auto& site=storySites_[i];const float dx=x_-(site.cell.col+site.x),dz=z_-(site.cell.row+site.z);
        if(dx*dx+dz*dz<.85f*.85f)return i;
    }
    return -1;
}

void GameView::prepareStoryAssets(sf::RenderTarget& target)
{
    if(!storyAssetsAttempted_)
    {
        sf::Texture signWood;
        (void)signWood.loadFromFile(executableDirectory()/"assets"/"textures"/"pbr"/"wood-diffuse.png");
        const auto models=executableDirectory()/"assets"/"models";
        const char* modelNames[]={"story-luggage","story-toolbox","story-player","story-ruin","story-sign"};
        for(int i=0;i<5;++i)
        {storyMeshes_[i]=loadCreatureMesh(models/(std::string(modelNames[i])+".nxm"));if(i<4)storyTextures_[i]=loadSurfaceTexture(models/(std::string(modelNames[i])+".png"),1024);}
        const auto textures=executableDirectory()/"assets"/"textures";
        footprintsReady_=footprintLeft_.loadFromFile(textures/"story-foot-left.png") && footprintRight_.loadFromFile(textures/"story-foot-right.png");
        if(footprintsReady_)for(auto* foot:{&footprintLeft_,&footprintRight_}){foot->setSmooth(true);(void)foot->generateMipmap();}
        for(int i=0;i<4;++i)
        {
            auto board=std::make_unique<sf::RenderTexture>();
            if(!board->resize({512,256}))continue;
            board->clear({40,34,28});
            if(signWood.getSize().x)
            {sf::Sprite wood(signWood);wood.setScale({512.f/signWood.getSize().x,256.f/signWood.getSize().y});wood.setColor({120,110,95});board->draw(wood);}
            sf::Text title(heading_.getFont(),storyDistrictName(i),32);title.setPosition({16,15});title.setFillColor({225,211,176});board->draw(title);
            sf::Text erased(heading_.getFont(),"SAFE ROUTE",36);erased.setPosition({18,76});erased.setFillColor({135,130,112});board->draw(erased);
            for(int scratch=0;scratch<3;++scratch)
            {sf::RectangleShape line({340.f,3.f});line.setPosition({16.f,92.f+scratch*9.f});line.setRotation(sf::degrees(scratch==1?-3.f:2.f));line.setFillColor({52,40,30});board->draw(line);}
            const char* warnings[]={"THE TICKETS SAY TOMORROW","CHANGE YOUR ESCAPE ROUTE","REMOVE THE BATTERIES","LEAVE BEFORE THE THIRD KNOCK"};
            sf::Text warning(heading_.getFont(),warnings[i],22);warning.setPosition({16,157});warning.setFillColor({212,190,141});board->draw(warning);
            sf::Text label(heading_.getFont(),"F / READ THE NOTE",20);label.setPosition({16,208});label.setFillColor({166,160,137});board->draw(label);
            board->display();board->setSmooth(true);storySigns_[i]=std::move(board);
        }
        for(int i=0;i<4;++i)
        {
            auto sign=std::make_unique<sf::RenderTexture>();if(!sign->resize({768,256}))continue;
            const sf::Color inks[]={{33,65,43},{71,47,28},{27,48,77},{65,25,48}};
            sign->clear(inks[i]);
            sf::Text title(heading_.getFont(),Adventure::shopNames[i],44);title.setPosition({24,30});title.setFillColor({237,216,160});sign->draw(title);
            sf::Text service(heading_.getFont(),Adventure::shopSpecialties[i],24);service.setPosition({24,110});service.setFillColor({211,215,193});sign->draw(service);
            sf::Text help(heading_.getFont(),"O / TRADE     N / BREW     P / POKER",27);help.setPosition({24,180});help.setFillColor({237,216,160});sign->draw(help);
            sign->display();sign->setSmooth(true);adventureSigns_[i]=std::move(sign);
        }
        storyAssetsAttempted_=true;
        (void)target.setActive(true);target.resetGLStates();
    }
}

void GameView::drawStory(const GameState& game)
{
    sf::Shader::bind(nullptr);glEnable(GL_TEXTURE_2D);glColor3f(.85f,.83f,.78f);
    if(storyRuin_)
    {
        const auto p=storyRuin_->cell;
        if(std::abs(p.col+.5f-x_)<14.f && std::abs(p.row+.5f-z_)<14.f)
        {
            glBindTexture(GL_TEXTURE_2D,storyTextures_[3]);glPushMatrix();glTranslatef(p.col+.5f,0.f,p.row+.5f);
            if(storyRuin_->alongRows)glRotatef(90.f,0,1,0);
            if(storyMeshes_[3])glCallList(storyMeshes_[3]);glPopMatrix();
        }
    }
    for(int i=0;i<4;++i)
    {
        const auto& site=storySites_[i];const float x=site.cell.col+site.x,z=site.cell.row+site.z;
        if((x-x_)*(x-x_)+(z-z_)*(z-z_)>225.f)continue;
        const int prop=i==3?2:i;
        glBindTexture(GL_TEXTURE_2D,storyTextures_[prop]);glPushMatrix();glTranslatef(x,.008f,z);glRotatef(site.angle*180.f/pi,0,1,0);
        if(storyMeshes_[prop])glCallList(storyMeshes_[prop]);glPopMatrix();
        if(storySigns_[i] && storyMeshes_[4])
        {
            sf::Texture::bind(&storySigns_[i]->getTexture());
            const float wallX=site.cell.col+.5f+(site.x-.5f)*1.3f,wallZ=site.cell.row+.5f+(site.z-.5f)*1.3f;
            const MazePosition behind{static_cast<int>(wallZ-.10f*std::cos(site.angle)),static_cast<int>(wallX-.10f*std::sin(site.angle))};
            const bool mounted=!game.maze.contains(behind) || game.maze.at(behind.row,behind.col)==MazeCell::Wall;
            glPushMatrix();glTranslatef(wallX,mounted?.55f:.04f,wallZ);glRotatef(site.angle*180.f/pi,0,1,0);
            if(!mounted)glRotatef(-12.f,1,0,0);glCallList(storyMeshes_[4]);glPopMatrix();sf::Texture::bind(nullptr);
        }
    }
    glDisable(GL_TEXTURE_2D);glColor3f(1,1,1);
}

void GameView::draw(sf::RenderTarget& target, const GameState& game)
{
    if (!initialized_ || game.maze.empty()) return;
    prepareStoryAssets(target);
    if (!shaderAttempted_)
    {
        shaderAttempted_ = true;
        if (sf::Shader::isAvailable())
        {
            auto shader = std::make_unique<sf::Shader>();
            const auto shaders = executableDirectory() / "assets" / "shaders";
            if (shader->loadFromFile(shaders / "world.vert", shaders / "world.frag"))
            {
                shader->setUniform("surface", sf::Shader::CurrentTexture);
                worldShader_ = std::move(shader);
            }
            auto foliage = std::make_unique<sf::Shader>();
            if (foliage->loadFromFile(shaders / "foliage.vert", shaders / "world.frag"))
            {
                foliage->setUniform("surface", sf::Shader::CurrentTexture);
                foliageShader_ = std::move(foliage);
            }
            auto creature = std::make_unique<sf::Shader>();
            if (creature->loadFromFile(shaders / "creature.vert", shaders / "world.frag"))
            {
                creature->setUniform("surface", sf::Shader::CurrentTexture);
                creatureShader_ = std::move(creature);
            }
            auto post = std::make_unique<sf::Shader>();
            if (post->loadFromFile(shaders / "post.vert", shaders / "post.frag"))
                postShader_ = std::move(post);
        }
    }
    target.pushGLStates();
    const bool usePost = preparePostProcessing();
    GLint previousFramebuffer = 0;
    if (usePost)
    {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
        framebufferApi().bindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    }
    glViewport(0, 0, static_cast<GLsizei>(viewport_.x), static_cast<GLsizei>(viewport_.y));
    glDepthMask(GL_TRUE); glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND); glEnable(GL_NORMALIZE);
    // Prepare the creature on the first gameplay frame, before a chase can expose a load hitch.
    if (!creatureMeshAttempted_)
    {
        const auto models = executableDirectory() / "assets" / "models";
        creatureMesh_ = loadCreatureMesh(models / "stalker.nxm");
        if (creatureMesh_) creatureTexture_ = loadSurfaceTexture(models / "stalker.jpg");
        creatureMeshAttempted_ = true;
    }
    if (!survivorMeshAttempted_)
    {
        const auto models = executableDirectory() / "assets" / "models";
        const auto mesh = loadAnimatedMesh(models / "survivor.nxa");
        survivorMesh_ = mesh.first; survivorFrames_ = mesh.frames; survivorDuration_ = mesh.duration;
        if (survivorMesh_) survivorTexture_ = loadSurfaceTexture(models / "survivor.png", 1024);
        survivorMeshAttempted_ = true;
    }
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    const float fearSway = fear_ * (.0035f * std::sin(ambientTime_ * 1.7f)
        + .0018f * std::sin(ambientTime_ * 4.3f + 1.1f));
    const float eyeHeight = elevation_ + jumpHeight_ - crouchBlend_ * .30f + seatedBlend_*.06f + .72f + .009f * bobBlend_ * std::sin(bobPhase_ * 2.f) + fearSway;
    const float roll = .35f * bobBlend_ * std::sin(bobPhase_)
        + fear_ * 1.15f * std::sin(ambientTime_ * 1.13f + .7f);
    const double nearPlane = .05, top = nearPlane * .65 * (1.0 + .035 * sprintBlend_);
    const double right = top * static_cast<double>(viewport_.x) / viewport_.y;
    glFrustum(-right, right, -top, top, nearPlane, 24.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glRotatef(-roll, 0.f, 0.f, 1.f);
    glRotatef(-pitch_ * 180.f / pi, 1.f, 0.f, 0.f);
    glRotatef(180.f - angle_ * 180.f / pi, 0.f, 1.f, 0.f);
    glTranslatef(-x_, -eyeHeight, -z_);
    glEnable(GL_FOG); glFogi(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, .048f + .025f * darkness_ + .003f * std::sin(ambientTime_ * .17f));
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_LIGHT1); glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    const auto storm = sampleStorm(ambientTime_, game.mazeSeed.value_or(0));
    const float flash = storm.flash;
    const float districtFog[4][3]={{.027f,.038f,.039f},{.040f,.034f,.028f},{.026f,.040f,.047f},{.034f,.030f,.043f}};
    const float fx=std::clamp((x_-game.maze.size().cols*.5f+1.5f)/3.f,0.f,1.f);
    const float fz=std::clamp((z_-game.maze.size().rows*.5f+1.5f)/3.f,0.f,1.f);
    GLfloat fog[4]={0,0,0,1};
    for(int channel=0;channel<3;++channel)
        fog[channel]=(districtFog[0][channel]*(1-fx)+districtFog[1][channel]*fx)*(1-fz)
            +(districtFog[2][channel]*(1-fx)+districtFog[3][channel]*fx)*fz+flash*(.10f+channel*.03f);
    glFogfv(GL_FOG_COLOR, fog);
    glClearColor(fog[0], fog[1], fog[2], 1.f); glClear(GL_COLOR_BUFFER_BIT);
    const float ambientScale = 1.f - darkness_ * .45f;
    const GLfloat ambient[] = { .10f * ambientScale + flash * .16f,
        .13f * ambientScale + flash * .18f, .12f * ambientScale + flash * .22f, 1.f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    const float threat = soundFrame().threat;
    const float interference = std::pow(std::max(0.f, std::sin(ambientTime_ * 7.7f + std::sin(ambientTime_ * 1.3f))), 6.f);
    const float flicker = (.82f + .12f * std::sin(ambientTime_ * 5.1f) + .06f * std::sin(ambientTime_ * 13.7f))
        * (1.f - (.32f * threat + .18f * fear_) * interference);
    const GLfloat lamp[] = { 1.f * flicker, .82f * flicker, .55f * flicker, 1.f };
    const GLfloat lampPos[] = { x_, eyeHeight, z_, 1.f };
    const GLfloat lampDir[] = { std::sin(angle_) * std::cos(pitch_), std::sin(pitch_), std::cos(angle_) * std::cos(pitch_) };
    glLightfv(GL_LIGHT0, GL_POSITION, lampPos); glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, lampDir);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lamp); glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 52.f - darkness_ * 9.f);
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 3.f); glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, .10f);
    const GLfloat moon[] = { .17f + flash * .55f, .24f + flash * .58f, .28f + flash * .62f, 1.f };
    const GLfloat moonPos[] = { -1.f, 3.f, -1.f, 0.f };
    glLightfv(GL_LIGHT1, GL_POSITION, moonPos); glLightfv(GL_LIGHT1, GL_DIFFUSE, moon);
    int nearestHouse = -1; float nearestHouseDistance = 64.f;
    for (std::size_t i = 0; i < safehouses_.size(); ++i)
    {
        const float dx = safehouses_[i].col + .5f - x_, dz = safehouses_[i].row + .5f - z_;
        const float distance = dx * dx + dz * dz;
        if (distance < nearestHouseDistance) { nearestHouse = static_cast<int>(i); nearestHouseDistance = distance; }
    }
    const float lanternFlicker = .88f + .08f * std::sin(ambientTime_ * 4.1f) + .04f * std::sin(ambientTime_ * 9.7f);
    const GLfloat shelterLight[] = { nearestHouse >= 0 ? .85f * lanternFlicker : 0.f,
        nearestHouse >= 0 ? .53f * lanternFlicker : 0.f, nearestHouse >= 0 ? .24f * lanternFlicker : 0.f, 1.f };
    const GLfloat shelterPosition[] = { nearestHouse >= 0 ? safehouses_[nearestHouse].col + .5f : 0.f,
        1.5f, nearestHouse >= 0 ? safehouses_[nearestHouse].row + .5f : 0.f, 1.f };
    glEnable(GL_LIGHT2); glLightfv(GL_LIGHT2, GL_POSITION, shelterPosition); glLightfv(GL_LIGHT2, GL_DIFFUSE, shelterLight);
    glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, .28f); glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, .13f);
    drawNightSky(x_, eyeHeight, z_, flash, ambientTime_, game.mazeSeed.value_or(0));
    updateChunkVisibility(game);
    const auto size = game.maze.size();
    const int r0 = std::max(0, static_cast<int>(z_) - 10), r1 = std::min(size.rows - 1, static_cast<int>(z_) + 10);
    const int c0 = std::max(0, static_cast<int>(x_) - 10), c1 = std::min(size.cols - 1, static_cast<int>(x_) + 10);
    // Subdivide the ground so the moving flashlight is evaluated throughout the path.
    if (!groundTextureAttempted_)
    {
        const auto textures=executableDirectory()/"assets"/"textures";
        if(groundAlbedo_.loadFromFile(textures/"pbr"/"ground-diffuse.png"))groundTexture_=groundAlbedo_.getNativeHandle();
        if(!groundTexture_)groundTexture_=loadSurfaceTexture(textures/"ground.png");
        groundMapsReady_=groundAlbedo_.getSize().x>0 && groundNormal_.loadFromFile(textures/"pbr"/"ground-nor_gl.png")
            && groundMaterial_.loadFromFile(textures/"pbr"/"ground-arm.png");
        for(auto* map:{&groundAlbedo_,&groundNormal_,&groundMaterial_})
        {if(map->getSize().x){map->setSmooth(true);map->setRepeated(true);(void)map->generateMipmap();}}
        groundTextureAttempted_ = true;
    }
    if (!shelterTextureAttempted_)
    {
        const auto textures=executableDirectory()/"assets"/"textures";
        if(woodAlbedo_.loadFromFile(textures/"pbr"/"wood-diffuse.png"))shelterTexture_=woodAlbedo_.getNativeHandle();
        if(!shelterTexture_)shelterTexture_=loadSurfaceTexture(textures/"shelter-wood.png");
        woodMapsReady_=woodAlbedo_.getSize().x>0 && woodNormal_.loadFromFile(textures/"pbr"/"wood-nor_gl.png")
            && woodMaterial_.loadFromFile(textures/"pbr"/"wood-arm.png");
        for(auto* map:{&woodAlbedo_,&woodNormal_,&woodMaterial_})
        {if(map->getSize().x){map->setSmooth(true);map->setRepeated(true);(void)map->generateMipmap();}}
        shelterTextureAttempted_ = true;
    }
    if (worldShader_)
    {
        worldShader_->setUniform("darkness", darkness_);
        worldShader_->setUniform("flicker", flicker);
        worldShader_->setUniform("flash", flash);
        worldShader_->setUniform("textured", groundTexture_ != 0);
        worldShader_->setUniform("wetness", .65f);
        worldShader_->setUniform("materialMaps",groundMapsReady_);
        worldShader_->setUniform("normalStrength",.55f);
        worldShader_->setUniform("refugeLighting",0.f);
        worldShader_->setUniform("footprintsEnabled",footprintsReady_);
        if(footprintsReady_)
        {
            worldShader_->setUniform("footprintLeft",footprintLeft_);worldShader_->setUniform("footprintRight",footprintRight_);
            sf::Glsl::Vec4 footsteps[24];for(auto& step:footsteps)step={-100.f,-100.f,1.f,0.f};
            for(std::size_t i=0;i<std::min(storyFootsteps_.size(),std::size_t{24});++i)
            {const auto step=storyFootsteps_[i];footsteps[i]={step.x,step.z,step.cosine,step.sine};}
            worldShader_->setUniformArray("footsteps",footsteps,24);
        }
        if(groundMapsReady_)
        {worldShader_->setUniform("normalMap",groundNormal_);worldShader_->setUniform("materialMap",groundMaterial_);}
        sf::Shader::bind(worldShader_.get());
    }
    if (groundTexture_) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, groundTexture_); }
    drawGroundChunks(game, r0, r1, c0, c1);
    glDisable(GL_TEXTURE_2D);
    if (worldShader_)
    {
        worldShader_->setUniform("textured", true);
        worldShader_->setUniform("wetness", .06f);
        worldShader_->setUniform("materialMaps",false);
        worldShader_->setUniform("footprintsEnabled",false);
    }
    drawHedgeChunks(game, r0, r1, c0, c1);
    if (foliageShader_)
    {
        foliageShader_->setUniform("darkness", darkness_);
        foliageShader_->setUniform("flicker", flicker);
        foliageShader_->setUniform("flash", flash);
        foliageShader_->setUniform("textured", true);
        foliageShader_->setUniform("wetness", .10f);
        foliageShader_->setUniform("time", ambientTime_);
        sf::Shader::bind(foliageShader_.get());
    }
    drawFoliageChunks(game, std::max(0, static_cast<int>(z_) - 6), std::min(size.rows - 1, static_cast<int>(z_) + 6),
        std::max(0, static_cast<int>(x_) - 6), std::min(size.cols - 1, static_cast<int>(x_) + 6));
    if (worldShader_ || foliageShader_) sf::Shader::bind(nullptr);
    if (shiftPhase_ == ShiftPhase::Opening || shiftPhase_ == ShiftPhase::Closing)
    {
        const float height = 1.9f * (shiftPhase_ == ShiftPhase::Opening ? 1.f - shiftProgress_ : shiftProgress_);
        if (height > .001f)
        {
            glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, hedgeTexture_);
            glColor3f(.8f, .8f, .75f); box(shift_.col + .5f, shift_.row + .5f, .5f, height);
            glDisable(GL_TEXTURE_2D);
        }
    }
    const bool inRefuge = safehouseAt(game.player, safehouses_) >= 0;
    const int playerHouse=safehouseAt(game.player,safehouses_);
    const GLfloat themeLights[4][4]={{.40f,.52f,.36f,1},{.56f,.39f,.25f,1},{.35f,.43f,.58f,1},{.55f,.34f,.43f,1}};
    const GLfloat* refugeAmbient=themeLights[playerHouse<0?0:playerHouse%4];
    if (inRefuge) glLightModelfv(GL_LIGHT_MODEL_AMBIENT, refugeAmbient);
    for (const auto house : safehouses_)
    {
        const int theme=safehouseAt(house,safehouses_)%4;
        const GLfloat trim[4][3]={{.20f,.55f,.31f},{.75f,.42f,.16f},{.22f,.45f,.76f},{.67f,.22f,.39f}};
        const float hx = house.col + .5f, hz = house.row + .5f;
        if ((hx-x_)*(hx-x_) + (hz-z_)*(hz-z_) > 144.f) continue;
        glPushMatrix(); glTranslatef(hx, 0.f, hz); glScalef(refugeScale, 1.f, refugeScale); glTranslatef(-hx, 0.f, -hz);
        if(worldShader_)
        {
            worldShader_->setUniform("materialMaps",woodMapsReady_);
            worldShader_->setUniform("wetness",0.f);
            worldShader_->setUniform("normalStrength",.75f);
            worldShader_->setUniform("refugeLighting",inRefuge?1.f:0.f);
            const sf::Glsl::Vec3 tints[]={{.85f,1.12f,.85f},{1.14f,.95f,.78f},{.78f,.96f,1.32f},{1.16f,.79f,1.08f}};
            worldShader_->setUniform("refugeTint",tints[theme]);
            if(woodMapsReady_){worldShader_->setUniform("normalMap",woodNormal_);worldShader_->setUniform("materialMap",woodMaterial_);}
            sf::Shader::bind(worldShader_.get());
        }
        if (shelterTexture_) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, shelterTexture_); }
        if (shelterTexture_) glColor3f(.8f, .78f, .72f); else glColor3f(.21f, .19f, .15f);
        for (float dx : { -1.25f, 1.25f }) for (float dz : { -1.25f, 1.25f })
            part(hx + dx, 0.f, hz + dz, .10f, 3.75f, .10f);
        if (shelterTexture_) glColor3f(.92f, .89f, .82f); else glColor3f(.24f, .22f, .18f);
        for (float side : { -1.f, 1.f }) for (float end : { -1.f, 1.f })
        {
            part(hx + side * 1.45f, 0.f, hz + end * 1.02f, .10f, 1.73f, .86f);
            part(hx + end * 1.02f, 0.f, hz + side * 1.45f, .86f, 1.73f, .10f);
        }
        // Gallery leaves an east stairwell open. North landing bridges it.
        part(hx - .38f, refugeUpperFloor - .10f, hz, 2.04f, .10f, 2.76f);
        part(hx + .98f, refugeUpperFloor - .10f, hz + 1.22f, .68f, .10f, .30f);
        for (int step = 0; step < 16; ++step)
        {
            const float height = refugeUpperFloor * (step + 1) / 16.f;
            part(hx + .98f, 0.f, hz + .30f + (step + .5f) * .85f / 16.f,
                .65f, height, .85f / 16.f);
        }
        for (float side : { -1.f, 1.f })
        {
            part(hx + side * 1.40f, refugeUpperFloor, hz, .08f, 1.75f, 2.80f);
            part(hx, refugeUpperFloor, hz + side * 1.40f, 2.80f, 1.75f, .08f);
        }
        // A visible banister guards the gallery edge beside the stairs.
        part(hx + .63f, refugeUpperFloor + .48f, hz - .15f, .055f, .06f, 2.0f);
        for (int post = 0; post < 6; ++post)
            part(hx + .63f, refugeUpperFloor, hz - 1.1f + post * .38f, .045f, .5f, .045f);
        part(hx, 3.65f, hz, 2.80f, .12f, 2.80f);
        sf::Shader::bind(nullptr);
        glDisable(GL_TEXTURE_2D);
        // Painted teal and coral trim and festoon bulbs on both floors.
        for (int floor = 0; floor < 2; ++floor)
        {
            const float base = floor * refugeUpperFloor;
            glColor3fv(trim[theme]); part(hx, base + .05f, hz - 1.37f, 2.70f, .15f, .04f);
            glColor3f(trim[theme][0]*.8f,trim[theme][1]*.8f,trim[theme][2]*.8f); part(hx - 1.37f, base + .05f, hz, .04f, .15f, 2.70f);
            glDisable(GL_LIGHTING);
            for (int bulb = 0; bulb < 9; ++bulb)
            {
                glColor3f(.35f+trim[theme][0]*.6f,.35f+trim[theme][1]*.6f,.25f+trim[theme][2]*.65f);
                ellipsoid(hx - 1.18f + bulb * .29f, base + 1.53f, hz - 1.30f, .035f, .055f, .035f);
            }
            glEnable(GL_LIGHTING);
        }
        glColor3f(.18f, .17f, .14f); stoneCap(hx, hz, 1.42f, 3.77f, 4.16f);
        const GLfloat glow[] = { .55f * lanternFlicker, .30f * lanternFlicker, .10f * lanternFlicker, 1.f };
        const GLfloat dark[] = { 0.f, 0.f, 0.f, 1.f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, glow);
        glColor3f(.9f, .63f, .25f); part(hx, 1.47f, hz, .13f, .20f, .13f);
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, dark);
        // Colourful refuge furniture stays clear of the four doorways.
        glColor3f(trim[theme][0]*.65f,trim[theme][1]*.65f,trim[theme][2]*.65f); part(hx, .018f, hz, 1.65f, .025f, 1.65f);
        if (!refugePropsAttempted_)
        {
            const auto models = executableDirectory() / "assets" / "models";
            const char* files[] = { "slots-cabinet.nxm", "slots-lever.nxm", "slots-stool.nxm", "bar-counter.nxm" };
            for (int prop=0; prop<4; ++prop) refugePropMeshes_[prop] = loadCreatureMesh(models / files[prop]);
            for (int reel=0;reel<3;++reel) for (int symbol=0;symbol<4;++symbol)
                reelMeshes_[reel][symbol]=loadCreatureMesh(models/("slots-reel-"+std::to_string(reel)+"-"+std::to_string(symbol)+".nxm"));
            slotsTexture_ = loadSurfaceTexture(models / "slots.png", 1024);
            barTexture_ = loadSurfaceTexture(models / "bar.png", 1024);
            barStoolMesh_=loadCreatureMesh(models/"bar-stool.nxm");
            drinkMesh_=loadCreatureMesh(models/"drink-cup.nxm");
            drinkTexture_=loadSurfaceTexture(models/"drink.png",1024);
            if(sf::Shader::isAvailable())
            {
                auto shader=std::make_unique<sf::Shader>();
                if(shader->loadFromFile(executableDirectory()/"assets"/"shaders"/"reel.vert",executableDirectory()/"assets"/"shaders"/"reel.frag"))reelShader_=std::move(shader);
            }
            refugePropsAttempted_ = true;
        }
        glColor3f(1.f,1.f,1.f);
        glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, slotsTexture_);
        glPushMatrix(); glTranslatef(hx - .85f, 0.f, hz - .85f); glScalef(.45f,1.f,.45f);
        if (refugePropMeshes_[0]) glCallList(refugePropMeshes_[0]);
        const auto& machine = refugeSlots_[safehouseAt(house, safehouses_)];
        for (int reel=0;reel<3;++reel)
        {
            const bool spinning=elapsed_<machine.stopTime-(2-reel)*.25f;
            const float travel=slotReelTravel(elapsed_,machine.stopTime,reel,machine.symbols[reel]);
            const int symbol=spinning?static_cast<int>(travel)%4:machine.symbols[reel];
            glPushMatrix();
            if(reelShader_)
            {
                reelShader_->setUniform("surface",0);
                reelShader_->setUniform("scroll",spinning?travel-std::floor(travel):0.f);
                const int tiles[]={7,8,10,11};
                const int nextTile=tiles[(symbol+1)%4];
                reelShader_->setUniform("nextCell",sf::Glsl::Vec2(static_cast<float>(nextTile%5),static_cast<float>(nextTile/5)));
                sf::Shader::bind(reelShader_.get());
            }
            if (reelMeshes_[reel][symbol]) glCallList(reelMeshes_[reel][symbol]);
            if(reelShader_) sf::Shader::bind(nullptr);
            glPopMatrix();
        }
        const float pull = slotLeverDegrees(elapsed_,machine.stopTime);
        // Pivot is retained from the downloaded Lever node, not a replacement mesh.
        glPushMatrix(); glTranslatef(.6701586f,.6501679f,-.1783184f);
        glRotatef(pull,1.f,0.f,0.f);
        glTranslatef(-.6701586f,-.6501679f,.1783184f);
        if (refugePropMeshes_[1]) glCallList(refugePropMeshes_[1]);
        glPopMatrix(); glPopMatrix();
        glBindTexture(GL_TEXTURE_2D, barTexture_);
        glPushMatrix(); glTranslatef(hx - .85f,0.f,hz + .85f); glRotatef(90.f,0.f,1.f,0.f); glScalef(1.f,.70f,1.f);
        if (refugePropMeshes_[3]) glCallList(refugePropMeshes_[3]);
        glPopMatrix();
        glBindTexture(GL_TEXTURE_2D,slotsTexture_);
        glPushMatrix();glTranslatef(hx-.85f+1.10f/refugeScale,0.f,hz+.85f);
        if(barStoolMesh_) glCallList(barStoolMesh_);
        glPopMatrix();
        if (!(seatedHouse_==safehouseAt(house,safehouses_) && drinkClock_>0.f))
        {
            glBindTexture(GL_TEXTURE_2D,drinkTexture_);
            glPushMatrix();glTranslatef(hx-.85f,.64f,hz+.85f);
            glColor3f(.65f,.80f,.87f);if(drinkMesh_) glCallList(drinkMesh_);glColor3f(1.f,1.f,1.f);glPopMatrix();
        }
        glDisable(GL_TEXTURE_2D);
        glPopMatrix();
    }
    drawStory(game);
    drawAdventure(game);
    drawDoors(game);
    for (std::size_t house = 0; house < refugeBalls_.size(); ++house)
    {
        const float hx = safehouses_[house].col + .5f, hz = safehouses_[house].row + .5f;
        if ((hx-x_)*(hx-x_) + (hz-z_)*(hz-z_) > 144.f) continue;
        glPushMatrix(); glTranslatef(hx, refugeUpperFloor, hz);
        // A small upstairs games court: two goals and a wall-mounted hoop.
        glColor3f(.04f, .30f, .24f); part(-.55f, .003f, 0.f, 2.70f, .015f, 3.90f);
        glDisable(GL_LIGHTING); glColor3f(.83f, .88f, .65f); glLineWidth(2.f);
        glBegin(GL_LINE_LOOP); glVertex3f(-1.8f,.025f,-1.85f); glVertex3f(.7f,.025f,-1.85f);
        glVertex3f(.7f,.025f,1.85f); glVertex3f(-1.8f,.025f,1.85f); glEnd();
        glBegin(GL_LINES); glVertex3f(-1.8f,.025f,0.f); glVertex3f(.7f,.025f,0.f); glEnd();
        glEnable(GL_LIGHTING);
        for (float end : {-1.f, 1.f})
        {
            glColor3f(end < 0 ? .15f : .95f, .55f, end < 0 ? .95f : .18f);
            part(-1.10f, 0.f, end * 1.98f, .04f, .48f, .04f);
            part(0.f, 0.f, end * 1.98f, .04f, .48f, .04f);
            part(-.55f, .48f, end * 1.98f, 1.14f, .04f, .04f);
            glBegin(GL_LINES);
            for (int strand = 0; strand < 9; ++strand)
            { glVertex3f(-1.1f + strand * .1375f,0.f,end*2.f); glVertex3f(-1.1f + strand*.1375f,.48f,end*2.f); }
            glEnd();
        }
        glColor3f(.91f,.85f,.63f); part(-.55f,.94f,1.94f,.85f,.55f,.05f);
        glColor3f(.95f,.30f,.08f); glLineWidth(3.f); glBegin(GL_LINE_LOOP);
        for (int vertex=0; vertex<32; ++vertex)
        { const float a=vertex*2.f*pi/32.f; glVertex3f(-.55f+std::cos(a)*.22f,1.12f,1.53f+std::sin(a)*.22f); }
        glEnd(); glLineWidth(1.f);
        glColor3f(.85f,.85f,.75f); glBegin(GL_LINES);
        for (int strand=0; strand<12; ++strand)
        { const float a=strand*2.f*pi/12.f; glVertex3f(-.55f+std::cos(a)*.22f,1.12f,1.53f+std::sin(a)*.22f);
          glVertex3f(-.55f+std::cos(a)*.15f,.88f,1.53f+std::sin(a)*.15f); }
        glEnd();
        for (int kind=0; kind<2; ++kind)
        {
            const auto& b=refugeBalls_[house][kind];
            glColor3f(kind ? .95f : .90f, kind ? .34f : .91f, kind ? .06f : .84f);
            ellipsoid(b.x,b.y,b.z,.13f,.13f,.13f);
            glDisable(GL_LIGHTING); glColor3f(.10f,.08f,.06f);
            for (int axis=0; axis<3; ++axis)
            {
                glBegin(GL_LINE_LOOP);
                for (int vertex=0;vertex<24;++vertex)
                {
                    const float a=vertex*2.f*pi/24.f, u=std::cos(a)*.132f,v=std::sin(a)*.132f;
                    glVertex3f(b.x+(axis==0?0:u),b.y+(axis==1?0:(axis==0?u:v)),b.z+(axis==2?0:v));
                }
                glEnd();
            }
            glEnable(GL_LIGHTING);
        }
        glPopMatrix();
    }
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    // Place the tablet at the side of the entrance, facing into the passage.
    const bool entranceSouth = open(game.maze, 1, 0);
    glPushMatrix(); glTranslatef(inscriptionX_, 0.f, inscriptionZ_);
    glRotatef(entranceSouth ? 90.f : 0.f, 0.f, 1.f, 0.f);
    glColor3f(.22f, .24f, .22f); part(0.f, 0.f, 0.f, .16f, .53f, .12f);
    glColor3f(.30f, .31f, .28f); part(0.f, .48f, 0.f, .42f, .35f, .08f);
    if ((x_-inscriptionX_)*(x_-inscriptionX_) + (z_-inscriptionZ_)*(z_-inscriptionZ_) < 16.f)
    {
        glDisable(GL_LIGHTING); glColor3f(.66f, .68f, .55f);
        glPushMatrix(); glTranslatef(0.f, .56f, .042f);
        for (int i = 0; i < 3; ++i)
        {
            glPushMatrix(); glTranslatef((i - 1) * .125f, 0.f, 0.f); glScalef(.39f, .39f, .39f);
            shrineGlyph(order_[i], 0); glPopMatrix();
        }
        glPopMatrix(); glEnable(GL_LIGHTING);
    }
    glPopMatrix();
    for (int i = 0; i < 3; ++i)
    {
        const float sx = shrines_[i].col + .5f, sz = shrines_[i].row + .5f;
        // Stones do not block a one-cell-wide passage. Avoid drawing their
        // opaque interior over the camera when the player walks through one.
        if (std::abs(sx - x_) < .22f && std::abs(sz - z_) < .22f) continue;
        glColor3f(.22f, .24f, .21f); box(sx, sz, .21f, .11f);
        glColor3f(.30f, .32f, .28f); box(sx, sz, .16f, .78f);
        stoneCap(sx, sz, .16f, .78f, .92f);
        const float dx = sx - x_, dz = sz - z_;
        if (dx * dx + dz * dz < 36.f)
        {
            const bool activated = (activeStones_ & (1u << i)) != 0;
            const float brightness = activated ? .85f : .55f;
            glDisable(GL_LIGHTING);
            glColor3f(.18f + colors[i][0] * brightness, .18f + colors[i][1] * brightness,
                .18f + colors[i][2] * brightness);
            // Four inscribed faces remain legible from any connected passage.
            for (int face = 0; face < 4; ++face)
            {
                glPushMatrix(); glTranslatef(sx, .47f, sz);
                glRotatef(face * 90.f, 0.f, 1.f, 0.f); glTranslatef(0.f, 0.f, .162f);
                shrineGlyph(i, clues_[i]); glPopMatrix();
            }
            glEnable(GL_LIGHTING);
        }
    }
    if (flash > .05f)
    {
        // The direction and branching remain stable during a strike, but change
        // with each seeded storm event. Depth testing hides it behind the hedge.
        std::mt19937 lightning(game.mazeSeed.value_or(0) ^ storm.event * 0x9e3779b9u);
        const float bearing = static_cast<float>(lightning() % 65536u) / 65536.f * 2.f * pi;
        const float bx = x_ + 15.f * std::sin(bearing), bz = z_ + 15.f * std::cos(bearing);
        const float tangentX = std::cos(bearing), tangentZ = -std::sin(bearing);
        std::array<float, 8> offsets{};
        for (std::size_t i = 1; i < offsets.size(); ++i)
            offsets[i] = offsets[i-1] + (static_cast<int>(lightning() % 201u) - 100) * .004f;
        glDisable(GL_LIGHTING); glDisable(GL_FOG); glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
        glColor4f(.72f, .81f, 1.f, flash * .85f); glLineWidth(2.f); glBegin(GL_LINE_STRIP);
        for (std::size_t i = 0; i < offsets.size(); ++i)
            glVertex3f(bx + offsets[i] * tangentX, 9.5f - static_cast<float>(i) * .85f,
                bz + offsets[i] * tangentZ);
        glEnd(); glLineWidth(1.f); glBegin(GL_LINE_STRIP);
        glVertex3f(bx + offsets[3] * tangentX, 6.95f, bz + offsets[3] * tangentZ);
        glVertex3f(bx + (offsets[3] + .48f) * tangentX, 6.15f, bz + (offsets[3] + .48f) * tangentZ);
        glVertex3f(bx + (offsets[3] + .70f) * tangentX, 5.25f, bz + (offsets[3] + .70f) * tangentZ);
        glEnd(); glDepthMask(GL_TRUE); glDisable(GL_BLEND); glEnable(GL_FOG); glEnable(GL_LIGHTING);
    }
    const float ex = size.cols - .5f, ez = size.rows - .5f;
    glPushMatrix(); glTranslatef(ex, 0.f, ez);
    if (open(game.maze, size.rows - 1, size.cols - 2)) glRotatef(90.f, 0.f, 1.f, 0.f);
    glColor3f(.38f, .34f, .22f);
    part(-.39f, 0.f, 0.f, .12f, 1.5f, .12f);
    part(.39f, 0.f, 0.f, .12f, 1.5f, .12f);
    part(0.f, 1.42f, 0.f, .9f, .10f, .12f);
    if (!gateOpen_)
    {
        glColor3f(.22f, .20f, .17f);
        for (int bar = -2; bar <= 2; ++bar) part(bar * .14f, 0.f, 0.f, .035f, 1.4f, .04f);
        part(0.f, .60f, 0.f, .75f, .06f, .05f);
    }
    glPopMatrix();
    const float blend = std::clamp(monsterClock_ / monsterStepDuration_, 0.f, 1.f);
    const float mx = interiorHunter_ ? hunterX_ : monsterFrom_.col + .5f + (monster_.col - monsterFrom_.col) * blend;
    const float mz = interiorHunter_ ? hunterZ_ : monsterFrom_.row + .5f + (monster_.row - monsterFrom_.row) * blend;
    if ((mx-x_)*(mx-x_) + (mz-z_)*(mz-z_) < 144.f)
    {
        if (creatureMesh_ && creatureTexture_)
        {
            if (creatureShader_)
            {
                creatureShader_->setUniform("darkness", darkness_);
                creatureShader_->setUniform("flicker", flicker);
                creatureShader_->setUniform("flash", flash);
                creatureShader_->setUniform("textured", true);
                creatureShader_->setUniform("wetness", .08f);
                creatureShader_->setUniform("gait", creatureGait_);
                creatureShader_->setUniform("time", ambientTime_);
                creatureShader_->setUniform("stride", same(monster_, monsterFrom_) || elapsed_ < 12.f ? 0.f : 1.f);
                creatureShader_->setUniform("chasing", hunter_.mode == HunterMode::Chase ? 1.f : 0.f);
                sf::Shader::bind(creatureShader_.get());
            }
            glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, creatureTexture_);
            glColor3f(.85f, .85f, .85f);
            glPushMatrix(); glTranslatef(mx, hunterY_, mz);
            glRotatef(monsterHeading_ * 180.f / pi, 0.f, 1.f, 0.f);
            glCallList(creatureMesh_); glPopMatrix();
            glDisable(GL_TEXTURE_2D);
            if (creatureShader_) sf::Shader::bind(nullptr);
        }
        else
        {
            if (!monsterTextureAttempted_)
            {
                monsterTexture_ = loadSurfaceTexture(executableDirectory() / "assets" / "textures" / "creature.png");
                monsterTextureAttempted_ = true;
            }
            if (monsterTexture_)
            {
                const float dx = mx-x_, dz = mz-z_;
                const float distance = std::max(.01f, std::sqrt(dx*dx + dz*dz));
                const float alignment = std::max(0.f, (dx*std::sin(angle_) + dz*std::cos(angle_)) / distance);
                const float illumination = std::min(1.f, .38f + .5f * std::pow(alignment, 8.f) / (1.f + distance*.12f) + flash*.25f);
                glDisable(GL_LIGHTING); glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, monsterTexture_);
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
                glColor4f(illumination, illumination, illumination, 1.f);
                glPushMatrix(); glTranslatef(mx, hunterY_, mz);
                glRotatef(std::atan2(x_ - mx, z_ - mz) * 180.f / pi, 0.f, 1.f, 0.f);
                glBegin(GL_QUADS);
                glTexCoord2f(0.f, 1.f); glVertex3f(-.62f, 0.f, 0.f);
                glTexCoord2f(1.f, 1.f); glVertex3f(.62f, 0.f, 0.f);
                glTexCoord2f(1.f, 0.f); glVertex3f(.62f, 1.65f, 0.f);
                glTexCoord2f(0.f, 0.f); glVertex3f(-.62f, 1.65f, 0.f);
                glEnd(); glPopMatrix();
                glDepthMask(GL_TRUE); glDisable(GL_BLEND); glDisable(GL_TEXTURE_2D); glEnable(GL_LIGHTING);
            }
            else
            {
                glPushMatrix(); glTranslatef(mx, hunterY_, mz);
                glRotatef(std::atan2(x_ - mx, z_ - mz) * 180.f / pi, 0.f, 1.f, 0.f);
                glColor3f(.12f, .10f, .085f);
                const float sway = elapsed_ >= 12.f ? std::sin(elapsed_ * 7.f) * .08f : 0.f;
                part(-.10f, 0.f, sway, .11f, .62f, .13f);
                part(.10f, 0.f, -sway, .11f, .62f, .13f);
                part(0.f, .54f, 0.f, .34f, .55f, .20f);
                part(-.23f, .38f, -sway, .09f, .68f, .10f);
                part(.23f, .38f, sway, .09f, .68f, .10f);
                part(0.f, 1.08f, .01f, .24f, .27f, .20f);
                const GLfloat eyes[] = { .8f, .1f, .035f, 1.f }; glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, eyes);
                glColor3f(.9f, .2f, .08f);
                part(-.055f, 1.23f, .12f, .035f, .035f, .025f);
                part(.055f, 1.23f, .12f, .035f, .035f, .025f);
                const GLfloat dark[] = { 0,0,0,1 }; glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, dark);
                glPopMatrix();
            }
        }
    }
    for (int slot = 0; slot < maxCoopPlayers; ++slot)
    {
        if (!remote_[slot].present) continue;
        const auto& visual = remoteVisual_[slot];
        const float dx = visual.x - x_, dz = visual.z - z_;
        const float distance = dx * dx + dz * dz;
        if (distance > .30f * .30f && distance < 144.f)
        {
            glPushMatrix(); glTranslatef(visual.x, visual.elevation+visual.jumpHeight, visual.z);
            if (visual.crouched || visual.seated) glScalef(1.f,.64f,1.f);
            glRotatef(visual.angle * 180.f / pi, 0.f, 1.f, 0.f);
            if (survivorMesh_ && survivorTexture_ && survivorFrames_ > 1 && survivorDuration_ > 0.f)
            {
                const float speed = visual.running ? 1.25f : .72f;
                const float time = visual.moving ? std::fmod(ambientTime_ * speed + slot * .31f, survivorDuration_) : 0.f;
                const auto movingFrames = survivorFrames_ - 1;
                const auto frame = visual.moving ? std::min(movingFrames - 1,
                    static_cast<unsigned int>(time / survivorDuration_ * movingFrames)) : movingFrames;
                const GLfloat tint[4][3] = { { .86f,.90f,.83f }, { .94f,.84f,.77f }, { .81f,.87f,.96f }, { .94f,.90f,.77f } };
                glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, survivorTexture_);
                glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, .4f); glColor3fv(tint[slot]);
                glCallList(survivorMesh_ + frame);
                glDisable(GL_ALPHA_TEST); glDisable(GL_TEXTURE_2D); glPopMatrix();
                continue;
            }
            const float stride = visual.moving ? .08f * std::sin(ambientTime_ * (visual.running ? 10.f : 7.f)) : 0.f;
            glColor3f(.18f, .20f, .18f);
            ellipsoid(-.09f, .26f, stride, .085f, .26f, .085f);
            ellipsoid(.09f, .26f, -stride, .085f, .26f, .085f);
            glColor3f(.13f, .14f, .13f);
            ellipsoid(-.09f, .06f, stride + .03f, .085f, .06f, .13f);
            ellipsoid(.09f, .06f, -stride + .03f, .085f, .06f, .13f);
            const GLfloat coats[4][3] = { { .27f,.30f,.26f }, { .30f,.25f,.21f }, { .23f,.27f,.32f }, { .32f,.29f,.21f } };
            glColor3fv(coats[slot]); ellipsoid(0.f, .72f, 0.f, .21f, .29f, .14f);
            ellipsoid(-.23f, .68f, -stride, .075f, .23f, .075f);
            ellipsoid(.23f, .68f, stride, .075f, .23f, .075f);
            glColor3f(.21f, .23f, .20f); ellipsoid(0.f, 1.06f, 0.f, .135f, .16f, .12f);
            glColor3f(.48f, .39f, .31f); ellipsoid(0.f, 1.06f, .10f, .085f, .105f, .035f);
            const GLfloat glow[] = { .34f, .20f, .07f, 1.f }, dark[] = { 0, 0, 0, 1 };
            glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, glow);
            glColor3f(.9f, .65f, .25f); part(.25f, .35f, .04f, .08f, .12f, .08f);
            glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, dark); glPopMatrix();
        }
    }
    if (!handsAttempted_)
    {
        const auto models=executableDirectory()/"assets"/"models";
        handMeshes_[0]=loadCreatureMesh(models/"hands-l.nxm");
        handMeshes_[1]=loadCreatureMesh(models/"hands-r.nxm");
        for(int side=0;side<2;++side)
        {
            const std::string name=side==0?"hands-l":"hands-r";
            std::ifstream open(models/(name+".nxm"),std::ios::binary),grip(models/(name+"-grip.nxm"),std::ios::binary);
            handOpen_[side]=readCreatureMesh(open);handGrip_[side]=readCreatureMesh(grip);
            if(handOpen_[side] && handGrip_[side] && (handOpen_[side]->indices!=handGrip_[side]->indices || handOpen_[side]->vertices.size()!=handGrip_[side]->vertices.size())) handGrip_[side].reset();
            std::ifstream rig(models/(side==0?"arms-l.nxr":"arms-r.nxr"),std::ios::binary);
            handRigs_[side]=HandMotion::readRig(rig);
        }
        handTexture_=loadSurfaceTexture(models/"survivor.png",1024);
        handSkinTexture_=loadSurfaceTexture(models/"hands-skin.png",2048);
        handsAttempted_=true;
    }
    // First-person source meshes follow the camera, never a procedural hand.
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_FOG); sf::Shader::bind(nullptr);
    const GLfloat handAmbient[]={.72f,.62f,.52f,1.f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,handAmbient);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,handTexture_); glColor3f(1.f,1.f,1.f);
    const auto toCamera=[&](InteractionPoint p)
    {
        const float dx=p.x-x_,dy=p.y-eyeHeight,dz=p.z-z_;
        const float forward=dx*std::sin(angle_)+dz*std::cos(angle_);
        return InteractionPoint{-dx*std::cos(angle_)+dz*std::sin(angle_),dy*std::cos(pitch_)-forward*std::sin(pitch_),-forward*std::cos(pitch_)-dy*std::sin(pitch_)};
    };
    int heldHouse=-1;
    for(std::size_t house=0;house<refugeBalls_.size();++house)
        if(refugeBalls_[house][1].carrier==networkSelfSlot_) heldHouse=static_cast<int>(house);
    for (int side=0;side<2;++side)
    {
        const float sway=std::sin(bobPhase_+side*1.2f)*bobBlend_*.007f;
        InteractionPoint pose{(side==0?-.16f:.16f)+handInertia_.x,-.12f+sway-crouchBlend_*.012f+handInertia_.y,-.31f};
        float gripRotation=0.f,gripAmount=.5f*ballHoldBlend_,wristPitch=0.f;
        if(heldHouse>=0)
        {
            const auto& ball=refugeBalls_[heldHouse][1];
            const auto centre=safehouses_[heldHouse];
            const auto at=toCamera({centre.col+.5f+ball.x,refugeUpperFloor+ball.y,centre.row+.5f+ball.z});
            pose.x+=(at.x+(side==0?-.13f:.13f)-pose.x)*ballHoldBlend_;
            pose.y+=(at.y-.035f-pose.y)*ballHoldBlend_;pose.z+=(at.z+.09f-pose.z)*ballHoldBlend_;
            gripRotation=(side==0?-55.f:55.f)*ballHoldBlend_;gripAmount=.35f*ballHoldBlend_;
        }
        else if(throwClock_>0.f)
        {
            const float phase=std::clamp((.4f-throwClock_)/.4f,0.f,1.f);
            const float withdraw=1.f-interactionSmooth(phase);
            const float follow=interactionSmooth(std::sin(phase*pi));
            const auto at=toCamera({lastHeldBall_.x,lastHeldBall_.y,lastHeldBall_.z});
            pose.x+=(at.x+(side==0?-.13f:.13f)-pose.x)*withdraw;
            pose.y+=(at.y-.035f-pose.y)*withdraw+.055f*follow;
            pose.z+=(at.z+.09f-pose.z)*withdraw-.10f*follow;
            gripRotation=(side==0?-55.f:55.f)*withdraw;
            gripAmount=.35f*(1.f-interactionSmooth(std::min(1.f,phase*3.f)));
        }
        if(side==1 && adventureMotionClock_>0.f)
        {
            const float u=std::clamp((1.2f-adventureMotionClock_)/1.2f,0.f,1.f),pulse=std::sin(interactionSmooth(u)*pi);
            if(adventureMotionItem_==-1 && lootingCache_>=0)
            {
                const auto cell=caches_[lootingCache_].cell;const auto contact=toCamera({cell.col+.5f,.36f,cell.row+.5f});
                const float reach=interactionSmooth(u/.22f)*(1.f-interactionSmooth((u-.58f)/.42f));
                pose.x+=(contact.x-pose.x)*reach;pose.y+=(contact.y+.02f-pose.y)*reach;pose.z+=(contact.z+.06f-pose.z)*reach;
                gripAmount=.25f*reach;wristPitch=-25.f*reach;
            }
            else if(adventureMotionItem_<=2)
            {pose={.14f-.1f*pulse,-.18f+.16f*pulse,-.36f+.05f*pulse};gripAmount=.95f;gripRotation=-25.f*pulse;wristPitch=-65.f*pulse;}
            else {pose.y+=.08f*pulse;pose.z-=.18f*pulse;gripAmount=.1f;wristPitch=35.f*pulse;}
        }
        else if(side==1 && drinkClock_>0.f)
        {
            const float sip=std::sin(interactionSmooth((4.f-drinkClock_)/4.f)*pi);
            pose={.14f-.10f*sip,-.15f+.12f*sip,-.36f+.06f*sip};
            gripRotation=-35.f*sip;wristPitch=-55.f*sip;gripAmount=.55f*std::sin(interactionSmooth((4.f-drinkClock_)/4.f)*pi);
        }
        else if(side==1 && nearbySlotHandle()>=0)
        {
            const int house=nearbySlotHandle(); const auto& machine=refugeSlots_[house];
            const float t=elapsed_-(machine.stopTime-2.f);
            if(t>=0.f && t<1.95f && machine.actor==networkSelfSlot_)
            {
                const auto knob=toCamera(slotKnobPoint(safehouses_[house],elapsed_,machine.stopTime));
                const float reach=slotHandReach(elapsed_,machine.stopTime);
                pose.x+=(knob.x-.005f-pose.x)*reach; pose.y+=(knob.y+.025f-pose.y)*reach;pose.z+=(knob.z+.055f-pose.z)*reach;
                gripRotation=25.f*reach;
                gripAmount=slotHandGrip(elapsed_,machine.stopTime);
                wristPitch=slotLeverDegrees(elapsed_,machine.stopTime)*.45f*reach;
            }
        }
        if(handRigs_[side] && handSkinTexture_)
        {
            using namespace HandMotion;
            const float lateral=side==0?-1.f:1.f;
            const V shoulder{lateral*.22f,-.27f+sway*.3f,.015f};
            V direction=axisAngle({1,0,0},wristPitch*.01745329252f)*V{0,.25f,-1.f};
            V across{-lateral,0,0};float handRoll=gripRotation*.01745329252f;
            if(side==1 && (drinkClock_>0.f || (adventureMotionClock_>0.f && adventureMotionItem_>=0 && adventureMotionItem_<=2)))
            {
                const float sip=adventureMotionClock_>0.f?std::sin(interactionSmooth((1.2f-adventureMotionClock_)/1.2f)*pi):std::sin(interactionSmooth((4.f-drinkClock_)/4.f)*pi);
                const R cup=axisAngle({0,0,1},handRoll)*axisAngle({1,0,0},(adventureMotionClock_>0.f?-65.f:-55.f)*sip*.01745329252f);
                direction=cup*V{-1,0,0};across=cup*V{0,1,0};handRoll=0.f;
            }
            skin(*handRigs_[side],handPoses_[side],shoulder,{pose.x,pose.y,pose.z},direction,
                across,gripAmount,handRoll);
            glBindTexture(GL_TEXTURE_2D,handSkinTexture_);
            glBegin(GL_TRIANGLES);
            for(const auto index:handRigs_[side]->indices)
            {
                const auto& v=handPoses_[side].vertices[index];
                glTexCoord2fv(v.uv);glNormal3fv(v.normal);glVertex3fv(v.position);
            }
            glEnd();glBindTexture(GL_TEXTURE_2D,handTexture_);
        }
        glPushMatrix();glTranslatef(pose.x,pose.y,pose.z);glRotatef(gripRotation,0.f,0.f,1.f);
        if(side==1 && adventureMotionClock_>0.f && adventureMotionItem_>=0 && adventureMotionItem_<=2 && adventureMeshes_[0])
        {
            const float sip=std::sin(interactionSmooth((1.2f-adventureMotionClock_)/1.2f)*pi);
            glPushMatrix();glRotatef(-65.f*sip,1,0,0);glTranslatef(.025f,-.06f,.025f);glScalef(.35f,.35f,.35f);
            glBindTexture(GL_TEXTURE_2D,adventureTextures_[0]);glCallList(adventureMeshes_[0]);glBindTexture(GL_TEXTURE_2D,handTexture_);glPopMatrix();
        }
        else if(side==1 && drinkClock_>0.f && drinkMesh_)
        {
            glPushMatrix();glTranslatef(.025f,-.045f,.05f);glScalef(.65f,.65f,.65f);
            const float sip=std::sin(interactionSmooth((4.f-drinkClock_)/4.f)*pi);
            glRotatef(-55.f*sip,1.f,0.f,0.f);
            glBindTexture(GL_TEXTURE_2D,drinkTexture_);glColor3f(.65f,.80f,.87f);glCallList(drinkMesh_);
            glColor3f(1.f,1.f,1.f);glBindTexture(GL_TEXTURE_2D,handTexture_);glPopMatrix();
        }
        glRotatef(35.f+wristPitch,1.f,0.f,0.f);glRotatef(180.f,0.f,1.f,0.f);
        if(handRigs_[side] && handSkinTexture_) { /* Source rig was drawn in camera space above. */ }
        else if(handOpen_[side] && handGrip_[side])
        {
            const auto& open=*handOpen_[side];const auto& grip=*handGrip_[side];
            const float fingerBlend=std::clamp(gripAmount,0.f,1.f);
            glBegin(GL_TRIANGLES);
            for(const auto index:open.indices)
            {
                const auto& a=open.vertices[index];const auto& b=grip.vertices[index];
                glTexCoord2fv(a.uv);
                glNormal3f(a.normal[0]+(b.normal[0]-a.normal[0])*fingerBlend,a.normal[1]+(b.normal[1]-a.normal[1])*fingerBlend,a.normal[2]+(b.normal[2]-a.normal[2])*fingerBlend);
                glVertex3f(a.position[0]+(b.position[0]-a.position[0])*fingerBlend,a.position[1]+(b.position[1]-a.position[1])*fingerBlend,a.position[2]+(b.position[2]-a.position[2])*fingerBlend);
            }
            glEnd();
        }
        else if (handMeshes_[side]) glCallList(handMeshes_[side]);
        glPopMatrix();
    }
    glDisable(GL_TEXTURE_2D); glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient); glEnable(GL_FOG);
    glPopMatrix();
    if (usePost)
    {
        framebufferApi().bindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
        glViewport(0, 0, static_cast<GLsizei>(viewport_.x), static_cast<GLsizei>(viewport_.y));
        drawPostProcess(game, flash);
    }
    target.popGLStates();
    // Raw 3D rendering changes state behind SFML's cache; re-establish its 2D state.
    target.resetGLStates();

    heading_.setPosition({ 24.f, 16.f }); target.draw(heading_);
    staminaLabel_.setPosition({ static_cast<float>(viewport_.x) - 134.f, 18.f }); target.draw(staminaLabel_);
    sf::RectangleShape breathTrack({ 110.f, 4.f });
    breathTrack.setPosition({ static_cast<float>(viewport_.x) - 134.f, 42.f });
    breathTrack.setFillColor({ 65, 72, 62, 210 }); target.draw(breathTrack);
    sf::RectangleShape breathFill({ 110.f * stamina_, 4.f });
    breathFill.setPosition(breathTrack.getPosition());
    breathFill.setFillColor(exhausted_ ? sf::Color{ 170, 100, 78 } : sf::Color{ 190, 199, 166 });
    target.draw(breathFill);
    voiceLabel_.setString(voiceStatus_);voiceLabel_.setPosition({static_cast<float>(viewport_.x)-240.f,65.f});target.draw(voiceLabel_);
    soundLabel_.setString(soundMuted_ ? "SOUND OFF" : soundStatus_ < 0 ? "NO AUDIO" : soundStatus_ > 0 ? "SOUND ON" : "SOUND ...");
    soundLabel_.setPosition({ static_cast<float>(viewport_.x) - 134.f, 49.f }); target.draw(soundLabel_);
    const int currentTradingPost=adventureHouse();
    seedText_.setString(std::string(currentTradingPost>=0?Adventure::shopNames[currentTradingPost%4]:storyDistrictName(storyDistrict(game.player,game.maze.size())))+" / SEED  " + std::to_string(game.mazeSeed.value_or(0))
        + (networkStatusText_.empty() ? "" : "     /     " + networkStatusText_));
    seedText_.setPosition({ 25.f, 47.f }); target.draw(seedText_);
    if (gateOpen_) objective_.setString("THE GATE IS OPEN   /   STEP THROUGH");
    else if (solved_ == 3) objective_.setString("FIND THE GATE   /   READ ITS INSCRIPTION");
    else if (!inscriptionRead_) objective_.setString("EXAMINE THE ENTRANCE INSCRIPTION   /   E");
    else if (puzzle_.stoneKind == StonePuzzleKind::Balance)
        objective_.setString("LIGHT STONES TO TOTAL " + std::to_string(puzzle_.balanceTarget) + "   /   E TO TOGGLE");
    else objective_.setString("TOUCH THE STONES IN ORDER   /   " + std::to_string(solved_) + "/3");
    objective_.setPosition({ 24.f, static_cast<float>(viewport_.y) - 103.f }); if(safehouseAt(game.player,safehouses_)<0) target.draw(objective_);
    if (inscriptionRead_)
    {
        std::string journal = puzzle_.stoneKind == StonePuzzleKind::Balance ? "GATE ORDER  " : "SEQUENCE  ";
        for (int i = 0; i < 3; ++i)
        {
            if (i) journal += "   /   ";
            const int stone = order_[i];
            journal += names[stone];
            journal += " ";
            journal += discoveredClues_[stone] ? std::to_string(clues_[stone]) : "?";
            if (activeStones_ & (1u << stone)) journal += "*";
        }
        journal_.setString(journal);
        journal_.setPosition({ 24.f, static_cast<float>(viewport_.y) - 76.f }); if(safehouseAt(game.player,safehouses_)<0) target.draw(journal_);
    }
    hint_.setString("WASD move / E use / I satchel / O trade / N craft / Tab atlas");
    hint_.setPosition({ 24.f, static_cast<float>(viewport_.y) - 45.f }); target.draw(hint_);
    const int nearby = nearbyShrine();
    const int door = elevation_ < .1f ? nearbyDoor() : -1;
    if (nearGate(game) && solved_ == 3 && !gateOpen_)
    {
        feedback_.setString("GATE CODE  " + gateInput_ + std::string("___").substr(gateInput_.size()) + "  /  1-9, Enter, Backspace");
        feedback_.setPosition({ 24.f, 78.f }); target.draw(feedback_);
        const char* rule = gateRule_ == GateRule::Forward ? "INSCRIPTION: Read the stone numbers in order."
            : gateRule_ == GateRule::Reverse ? "INSCRIPTION: Read the stone numbers in reverse order."
            : gateRule_ == GateRule::Advance ? "INSCRIPTION: Add 1 to each number; 9 becomes 1."
            : gateRule_ == GateRule::Rotate ? "INSCRIPTION: Read second, then third, then first."
            : "INSCRIPTION: Subtract each number from 10, in order.";
        feedback_.setString(rule); feedback_.setPosition({ 24.f, 106.f }); target.draw(feedback_);
        if (messageClock_ > 0.f)
        {
            feedback_.setString(notice_); feedback_.setPosition({ 24.f, 134.f }); target.draw(feedback_);
        }
    }
    else if(nearbyBar()>=0)
    {
        feedback_.setString(seatedHouse_>=0?"BAR / E drink / P poker / Q stand":"BAR / E sit and drink / P poker");
        feedback_.setPosition({24.f,78.f});target.draw(feedback_);
    }
    else if (nearbySlotHandle() >= 0)
    {
        const auto& machine = refugeSlots_[nearbySlotHandle()];
        const bool spinning = elapsed_ < machine.stopTime;
        const bool jackpot = machine.spins && machine.symbols[0] == machine.symbols[1] && machine.symbols[1] == machine.symbols[2];
        feedback_.setString(spinning ? "LUCKY LANTERN / Reels spinning..." : jackpot ? "JACKPOT! / E pull handle / Free play" : "LUCKY LANTERN / E pull handle / Match three / Free play");
        feedback_.setPosition({24.f,78.f}); target.draw(feedback_);
    }
    else if (door >= 0)
    {
        const bool open = doors_[door / 4][door % 4].open;
        feedback_.setString(open ? "SAFEHOUSE DOOR  /  E to close" : "SAFEHOUSE DOOR  /  E to open");
        feedback_.setPosition({ 24.f, 78.f }); target.draw(feedback_);
        if (messageClock_ > 0.f)
        {
            feedback_.setString(notice_); feedback_.setPosition({ 24.f, 106.f }); target.draw(feedback_);
        }
    }
    else if (nearby >= 0)
    {
        const char* action = puzzle_.stoneKind == StonePuzzleKind::Balance && solved_ != 3
            ? ((activeStones_ & (1u << nearby)) ? "E to extinguish" : "E to light") : "E to touch";
        feedback_.setString(std::string(names[nearby]) + " STONE  /  engraved " + std::to_string(clues_[nearby]) + "  /  " + action);
        feedback_.setPosition({ 24.f, 78.f }); target.draw(feedback_);
        if (messageClock_ > 0.f)
        {
            feedback_.setString(notice_); feedback_.setPosition({ 24.f, 106.f }); target.draw(feedback_);
        }
    }
    else if (nearInscription() && !inscriptionRead_)
    {
        feedback_.setString("ENTRANCE INSCRIPTION  /  E to read");
        feedback_.setPosition({ 24.f, 78.f }); target.draw(feedback_);
        if (messageClock_ > 0.f)
        {
            feedback_.setString(notice_); feedback_.setPosition({ 24.f, 106.f }); target.draw(feedback_);
        }
    }
    else if(nearbyStorySite()>=0)
    {
        feedback_.setString("ABANDONED BELONGINGS / F to read note / J journal");
        feedback_.setPosition({24.f,78.f});target.draw(feedback_);
        if(messageClock_>0.f){feedback_.setString(notice_);feedback_.setPosition({24.f,106.f});target.draw(feedback_);}
    }
    else if (safehouseAt(game.player, safehouses_) >= 0)
    {
        const int house = safehouseAt(game.player, safehouses_);
        feedback_.setString(elevation_ > refugeUpperFloor - .1f
            ? "G pick up / throw basketball | K kick football\nHOOP " + std::to_string(refugeBalls_[house][1].scoreA) + " | FOOTBALL " + std::to_string(refugeBalls_[house][0].scoreA) + " - " + std::to_string(refugeBalls_[house][0].scoreB)
            : "TRADING POST / O shop / N brew / P poker\nE doors and bar / F notes / Games upstairs");
        feedback_.setPosition({ 24.f, 78.f }); target.draw(feedback_);
        if (messageClock_ > 0.f)
        {
            feedback_.setString(notice_); feedback_.setPosition({ 24.f, 136.f }); target.draw(feedback_);
        }
    }
    else if (messageClock_ > 0.f)
    {
        feedback_.setString(notice_); feedback_.setPosition({ 24.f, 78.f }); target.draw(feedback_);
    }
    sf::RectangleShape crosshair({ 4.f, 4.f }); crosshair.setFillColor({ 230, 225, 196, 180 });
    crosshair.setPosition({ viewport_.x * .5f - 2.f, viewport_.y * .5f - 2.f }); target.draw(crosshair);
    drawPoker(target);
    drawAdventureHud(target,game);
    drawDoorWarning(target);
}

void GameView::drawDoorWarning(sf::RenderTarget& target)
{
    int selected=-1;float closest=25.f;
    for(std::size_t house=0;house<doors_.size();++house) for(int side=0;side<4;++side)
    {
        const auto& door=doors_[house][side];
        if((door.open || !doorStrikes_[house*4+side]) && door.rattle<=0.f) continue;
        const auto frame=safehouseDoorFrame(safehouses_[house],side);
        const float dx=frame.centreX-x_,dz=frame.centreZ-z_,distance=dx*dx+dz*dz;
        if(distance<closest) {closest=distance;selected=static_cast<int>(house*4+side);}
    }
    if(selected<0) return;
    const auto& door=doors_[selected/4][selected%4];const unsigned strikes=doorStrikes_[selected];
    const auto source=safehouseDoorFrame(safehouses_[selected/4],selected%4);
    const float dx=source.centreX-x_,dz=source.centreZ-z_;
    const float forward=dx*std::sin(angle_)+dz*std::cos(angle_),right=-dx*std::cos(angle_)+dz*std::sin(angle_);
    const char* direction=std::abs(right)>std::abs(forward)*.7f?(right>0.f?"RIGHT DOOR":"LEFT DOOR"):(forward>0.f?"DOOR AHEAD":"DOOR BEHIND");
    const std::string warning=std::string(direction)+" / "+
        (door.open?"FORCED OPEN - MOVE!":strikes>=2?"SECOND STRIKE - LEAVE NOW":strikes==1?"FIRST STRIKE - FIND AN EXIT":"SOMETHING IS TESTING THE HANDLE");
    const float width=std::min(580.f,viewport_.x-32.f),left=(viewport_.x-width)*.5f,top=viewport_.y*.34f;
    sf::RectangleShape panel({width,48.f});panel.setPosition({left,top});panel.setFillColor({28,14,12,230});
    panel.setOutlineColor(strikes>=2?sf::Color(206,78,48):sf::Color(168,130,76));panel.setOutlineThickness(1.f);target.draw(panel);
    sf::Text text(heading_.getFont(),warning,18);text.setFillColor({245,207,156});text.setPosition({left+14.f,top+12.f});target.draw(text);
}
#include "GameView.Adventure.inc"
