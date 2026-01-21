#include "Game.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <random>

// File-scope: store the player's fixed Y height so we can force horizontal-only motion
static float s_playerFixedY = 0.5f;
static const float floorHalf = 12.0f * 0.5f; // = 6.0f
// put near top of Game.cpp or in Collision.h
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

struct OBB
{
    glm::vec3 center;  // world-space center
    glm::vec3 axis[3]; // orthonormal axes in world space (unit vectors)
    float half[3];     // half-lengths along each axis (world units)
};
// modelMatrix 是实例的完整世界变换矩阵 (translate*rotate*scale)
// bboxMin/max are in model-local coordinates
static OBB BuildOBBFromModel(const glm::vec3 &bboxMin,
                             const glm::vec3 &bboxMax,
                             const glm::mat4 &modelMatrix)
{
    // local center and half extents
    glm::vec3 localCenter = (bboxMin + bboxMax) * 0.5f;
    glm::vec3 localHalf = (bboxMax - bboxMin) * 0.5f;

    // world center
    glm::vec3 worldCenter = glm::vec3(modelMatrix * glm::vec4(localCenter, 1.0f));

    // linear 3x3 part (rotation * scale)
    glm::mat3 M3 = glm::mat3(modelMatrix);

    OBB obb;
    obb.center = worldCenter;

    // for each local axis (unit vectors X,Y,Z), transform by M3:
    // axis vector in world = normalize(M3 * unit)
    // half-length in world = length(M3 * unit) * localHalf[i]
    glm::vec3 ux = glm::vec3(M3 * glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec3 uy = glm::vec3(M3 * glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 uz = glm::vec3(M3 * glm::vec3(0.0f, 0.0f, 1.0f));

    float lenx = glm::length(ux);
    float leny = glm::length(uy);
    float lenz = glm::length(uz);

    // Avoid degenerate axes
    obb.axis[0] = (lenx > 1e-6f) ? ux / lenx : glm::vec3(1, 0, 0);
    obb.axis[1] = (leny > 1e-6f) ? uy / leny : glm::vec3(0, 1, 0);
    obb.axis[2] = (lenz > 1e-6f) ? uz / lenz : glm::vec3(0, 0, 1);

    obb.half[0] = lenx * localHalf.x;
    obb.half[1] = leny * localHalf.y;
    obb.half[2] = lenz * localHalf.z;

    return obb;
}
// returns true if obbA and obbB overlap
static bool OBBIntersectSAT(const OBB &A, const OBB &B)
{
    // vector from A to B
    glm::vec3 T = B.center - A.center;

    // list of 15 test axes: A.axis[0..2], B.axis[0..2], and cross products
    // we will test each axis by projecting both boxes onto it

    // convenience lambdas
    auto projectIntervalRadius = [](const OBB &O, const glm::vec3 &axis) -> float
    {
        // axis assumed unit length
        float r = 0.0f;
        r += O.half[0] * fabs(glm::dot(O.axis[0], axis));
        r += O.half[1] * fabs(glm::dot(O.axis[1], axis));
        r += O.half[2] * fabs(glm::dot(O.axis[2], axis));
        return r;
    };

    // test function for an axis (must be normalized)
    auto testAxis = [&](const glm::vec3 &axis) -> bool
    {
        float axisLen2 = glm::dot(axis, axis);
        if (axisLen2 < 1e-8f)
            return true; // axis degenerate -> skip test (treat as non-separating)
        glm::vec3 axisN = axis / sqrt(axisLen2);
        float dist = fabs(glm::dot(T, axisN));
        float ra = projectIntervalRadius(A, axisN);
        float rb = projectIntervalRadius(B, axisN);
        return dist <= (ra + rb) + 1e-6f; // overlap if true
    };

    // 3 axes A
    for (int i = 0; i < 3; i++)
        if (!testAxis(A.axis[i]))
            return false;
    // 3 axes B
    for (int i = 0; i < 3; i++)
        if (!testAxis(B.axis[i]))
            return false;
    // 9 cross axes
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            glm::vec3 ax = glm::cross(A.axis[i], B.axis[j]);
            if (!testAxis(ax))
                return false;
        }
    }
    // no separating axis found -> intersection
    return true;
}

// safe absolute dot
static inline float AbsDot(const glm::vec3 &a, const glm::vec3 &b)
{
    return fabs(glm::dot(a, b));
}

struct FallingObjectConfig
{
    std::string path;
    glm::vec3 modelScale;
};
glm::vec3 sunDir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.2f));

glm::mat4 lightView = glm::lookAt(
    -sunDir * 10.0f,
    glm::vec3(0.0f),
    glm::vec3(0, 1, 0));

glm::mat4 lightProj = glm::ortho(
    -10.0f, 10.0f,
    -10.0f, 10.0f,
    1.0f, 30.0f);

glm::mat4 lightVP = lightProj * lightView;
static glm::mat4 MakeModelMatrix(const glm::vec3 &position,
                                 const glm::vec3 &rotAxis, float rotAngleRad,
                                 const glm::vec3 &scale)
{
    glm::mat4 m(1.0f);
    m = glm::translate(m, position);
    if (rotAngleRad != 0.0f)
        m = glm::rotate(m, rotAngleRad, rotAxis);
    m = glm::scale(m, scale);
    return m;
}

Game::Game()
    : spawnTimer(0.0f), playerDead(false)
{
    // seed RNG with high-resolution clock
    rng.seed((uint32_t)std::chrono::high_resolution_clock::now().time_since_epoch().count());
}
bool Game::LoadResources(const std::string &assetsDir)
{
    FallingObjectConfig fallingModelsConfig[3] = {
        {assetsDir + "/models/bucket.obj", glm::vec3(0.2f)},
        {assetsDir + "/models/jar.obj", glm::vec3(0.2f)},
        {assetsDir + "/models/teapot.obj", glm::vec3(1.0f)}};
    bool ok = true;
    for (int i = 0; i < 3; ++i)
    {
        fallingModels[i].modelScale = fallingModelsConfig[i].modelScale;
        ok &= fallingModels[i].LoadFromFile(fallingModelsConfig[i].path);
        if (!ok)
        {
            std::cerr << "Failed load falling objects: " << fallingModelsConfig[i].path << std::endl;
        }
    }
    std::string floorPath = assetsDir + "/models/floor.obj";
    ok &= floorModel.LoadFromFile(floorPath);
    if (!ok)
    {
        std::cerr << "Failed load floor: " << floorPath << std::endl;
    }

    // set reasonable scales if model units differ
    floorModel.modelScale = glm::vec3(1.0f);

    // compute floorTop from floorModel bbox (world-local bbox)
    // bboxMin/bboxMax are in model space (apply modelScale when mapping to world)
    float floorModelTop = floorModel.bboxMax.y * floorModel.modelScale.y;
    // If your floor model's origin is center and it's placed with translate y = -0.55,
    // you can compute desired world top. For simplicity, we'll set floorTop to where you want:
    // For example place floorModel so its top is at -0.5:
    float desiredTopY = -0.5f;
    // compute needed base translate and store somewhere or apply in Render.
    // We'll simply store floorTop for collision calculations:
    floorTop = desiredTopY;
    // LoadCatParts(assetsDir);

    return ok;
}
void Game::InitShadowMap()
{
    // ===== Shadow map framebuffer =====
    glGenFramebuffers(1, &depthFBO);

    // depth texture
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_SIZE, SHADOW_SIZE, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float borderColor[] = {1.0, 1.0, 1.0, 1.0};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // attach
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void Game::Reset()
{
    falling.clear();
    collectibles.clear();
    spawnTimer = 0.0f;
    collectSpawnTimer = 2.0f; // first collectible spawn delay
    score = 0;

    // Player stands at 0.5 height
    player.groundY = 0.5f;
    player.pos = glm::vec3(0.0f, player.groundY, 0.0f);
    player.verticalVel = 0.0f;
    player.isGrounded = true;
    player.jumpCooldown = 0.0f;
    player.stamina = 1.0f;

    player.color = glm::vec3(1.0f, 0.8f, 0.1f);
    playerDead = false;
    playerMaxHealth = 3;
    playerHealth = playerMaxHealth;
    hitEffectTimer = 0.0f;

    // after loading floorModel and setting floorModel.modelScale
    float desiredFloorTop = -0.5f; // 你希望地面顶面的 world Y
    float floorTopLocal = floorModel.bboxMax.y * floorModel.modelScale.y;
    float floorYOffset = desiredFloorTop - floorTopLocal;

    // store for collision logic
    this->floorTop = desiredFloorTop;
    this->floorYOffset = floorYOffset;

    // set floor modelMatrix once
    glm::vec3 floorPos(0.0f, floorYOffset, 0.0f);
    floorModel.modelMatrix = MakeModelMatrix(floorPos, glm::vec3(0, 1, 0), 0.0f, floorModel.modelScale);
}

static float randf(std::mt19937 &rng, float a, float b)
{
    std::uniform_real_distribution<float> d(a, b);
    return d(rng);
}

static glm::vec3 RandomColor(std::mt19937 &rng)
{
    return glm::vec3(randf(rng, 0.2f, 1.0f), randf(rng, 0.2f, 1.0f), randf(rng, 0.2f, 1.0f));
}

void Game::SpawnObject()
{
    Falling f;

    // --------- 1) compute floor world bounds (min/max on X,Z) robustly by transforming bbox corners ----------
    float floorMinX, floorMaxX, floorMinZ, floorMaxZ;
    bool haveFloor = false;

    if (floorModel.bboxMax != floorModel.bboxMin) // basic validity
    {
        // produce 8 corners in local space
        glm::vec3 corners[8];
        glm::vec3 bmin = floorModel.bboxMin;
        glm::vec3 bmax = floorModel.bboxMax;
        int idx = 0;
        for (int xi = 0; xi <= 1; ++xi)
            for (int yi = 0; yi <= 1; ++yi)
                for (int zi = 0; zi <= 1; ++zi)
                {
                    corners[idx++] = glm::vec3(
                        xi ? bmax.x : bmin.x,
                        yi ? bmax.y : bmin.y,
                        zi ? bmax.z : bmin.z);
                }

        // transform all corners by floorModel.modelMatrix to world and take min/max X/Z
        bool first = true;
        for (int i = 0; i < 8; ++i)
        {
            glm::vec4 wc = floorModel.modelMatrix * glm::vec4(corners[i], 1.0f);
            if (first)
            {
                floorMinX = floorMaxX = wc.x;
                floorMinZ = floorMaxZ = wc.z;
                first = false;
            }
            else
            {
                floorMinX = std::min(floorMinX, wc.x);
                floorMaxX = std::max(floorMaxX, wc.x);
                floorMinZ = std::min(floorMinZ, wc.z);
                floorMaxZ = std::max(floorMaxZ, wc.z);
            }
        }
        haveFloor = true;
    }

    if (!haveFloor)
    {
        const float floorHalf = 12.0f * 0.5f;
        floorMinX = -floorHalf;
        floorMaxX = floorHalf;
        floorMinZ = -floorHalf;
        floorMaxZ = floorHalf;
    }

    // optional debug
    // std::cout << "[Spawn] floorX=[" << floorMinX << "," << floorMaxX << "] floorZ=[" << floorMinZ << "," << floorMaxZ << "]\n";

    // --------- 2) pick prototype index first so we can compute its horizontal footprint ----------
    f.modelIndex = rng() % 3;
    f.modelScale = fallingModels[f.modelIndex].modelScale;

    // get prototype bbox local corners
    glm::vec3 pmin = fallingModels[f.modelIndex].bboxMin;
    glm::vec3 pmax = fallingModels[f.modelIndex].bboxMax;

    glm::vec3 protoCorners[8];
    int pidx = 0;
    for (int xi = 0; xi <= 1; ++xi)
        for (int yi = 0; yi <= 1; ++yi)
            for (int zi = 0; zi <= 1; ++zi)
            {
                protoCorners[pidx++] = glm::vec3(
                    xi ? pmax.x : pmin.x,
                    yi ? pmax.y : pmin.y,
                    zi ? pmax.z : pmin.z);
            }

    // transform prototype corners by scale (no translation) to find horizontal footprint extents
    float maxAbsX = 0.0f, maxAbsZ = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        glm::vec3 scaled = protoCorners[i] * f.modelScale; // component-wise
        maxAbsX = std::max(maxAbsX, fabs(scaled.x));
        maxAbsZ = std::max(maxAbsZ, fabs(scaled.z));
    }

    // half-extents on X/Z that the prototype may occupy when centered at origin
    float halfX = maxAbsX;
    float halfZ = maxAbsZ;
    const float safety = 0.01f; // small margin
    halfX += safety;
    halfZ += safety;

    // optional debug
    // std::cout << "[Spawn] protoIdx=" << f.modelIndex << " halfX=" << halfX << " halfZ=" << halfZ << "\n";

    // --------- 3) compute safe spawn ranges so that entire footprint lies inside floor bounds ----------
    float spawnMinX = floorMinX + halfX;
    float spawnMaxX = floorMaxX - halfX;
    float spawnMinZ = floorMinZ + halfZ;
    float spawnMaxZ = floorMaxZ - halfZ;

    // if ranges are invalid, shrink to center area (fallback)
    if (spawnMaxX <= spawnMinX)
    {
        float cx = 0.5f * (floorMinX + floorMaxX);
        spawnMinX = cx - 0.5f;
        spawnMaxX = cx + 0.5f;
    }
    if (spawnMaxZ <= spawnMinZ)
    {
        float cz = 0.5f * (floorMinZ + floorMaxZ);
        spawnMinZ = cz - 0.5f;
        spawnMaxZ = cz + 0.5f;
    }

    // optional debug - print once to verify ranges (uncomment if needed)
    // std::cout << "[Spawn] spawnX=[" << spawnMinX << "," << spawnMaxX << "] spawnZ=[" << spawnMinZ << "," << spawnMaxZ << "]\n";

    // --------- 4) choose spawn center and vertical position and initial velocity (no horizontal) ----------
    f.pos.x = randf(rng, spawnMinX, spawnMaxX);
    f.pos.z = randf(rng, spawnMinZ, spawnMaxZ);
    f.pos.y = randf(rng, 5.0f, 8.0f);

    // vertical-only initial velocity
    float initialDown = randf(rng, 0.6f, 1.6f);
    f.vel = glm::vec3(0.0f, -initialDown, 0.0f);

    // no dynamic rotation while falling
    f.rot = 0.0f;
    f.rotAxis = glm::vec3(0.0f, 1.0f, 0.0f);
    f.rotSpeed = 0.0f;

    f.alive = true;

    // store halfExtents for AABB quick test (consistent with computed halfX/halfZ)
    f.halfExtents = glm::vec3(halfX, (pmax.y - pmin.y) * 0.5f * f.modelScale.y + safety, halfZ);

    // initial modelMatrix so it is visible on first frame
    {
        glm::mat4 m(1.0f);
        m = glm::translate(m, f.pos);
        m = glm::scale(m, f.modelScale);
        f.modelMatrix = m;
    }

    falling.push_back(f);
}

static Collectible MakeCollectible(std::mt19937 &rng, float floorTop)
{
    Collectible c;
    const float floorHalf = 12.0f * 0.5f; // same as player clamp
    c.pos.x = randf(rng, -floorHalf + 0.5f, floorHalf - 0.5f);
    c.pos.z = randf(rng, -floorHalf + 0.5f, floorHalf - 0.5f);
    // 立方体高度 0.4，放在地面上方
    float cubeHalf = 0.2f;
    c.pos.y = floorTop + cubeHalf;
    c.color = RandomColor(rng);
    c.lifetime = randf(rng, 6.0f, 10.0f); // 生存时间 6-10 秒
    c.alive = true;
    return c;
}
void Game::Update(float dt, const bool keys[1024], const glm::vec3 &cameraFront, const glm::vec3 &cameraUp)
{
    if (playerDead)
        return;

    static float difficulty = 1.0f;
    difficulty += dt * 0.02f; // 每秒略微变难
    difficulty = glm::clamp(difficulty, 1.0f, 2.5f);

    // 受击红光计时衰减
    if (hitEffectTimer > 0.0f)
    {
        hitEffectTimer -= dt;
        if (hitEffectTimer < 0.0f)
            hitEffectTimer = 0.0f;
    }

    const float floorTop = -0.5f;
    player.Update(dt, keys, cameraFront, cameraUp);

    // Player radius (0.6 scaled cube)
    const float playerHalf = 0.3f;

    // Clamp X, Z positions to stay within floor bounds
    player.pos.x = glm::clamp(player.pos.x, -floorHalf + playerHalf, floorHalf - playerHalf);
    player.pos.z = glm::clamp(player.pos.z, -floorHalf + playerHalf, floorHalf - playerHalf);

    // === 玩家竖直方向物理（重力 + 跳跃 + 落地检测） ===
    const float gravity = -9.8f * 2.0f; // 略加强一点重力手感
    if (!player.isGrounded)
    {
        player.verticalVel += gravity * dt;
        player.pos.y += player.verticalVel * dt;
    }

    // 与地面（groundY）接触的简单判定：脚落到或穿过地面就“落地”
    float footY = player.pos.y;
    if (footY <= player.groundY)
    {
        player.pos.y = player.groundY;
        player.verticalVel = 0.0f;
        player.isGrounded = true;
    }

    // ---------- Spawn and falling updates ----------
    spawnTimer -= dt;
    if (spawnTimer <= 0.0f)
    {
        spawnTimer = randf(rng, 0.4f, 0.9f); // slightly varying spawn interval
        SpawnObject();
    }

    // ---------- Collectibles spawn/update ----------
    collectSpawnTimer -= dt;
    if (collectSpawnTimer <= 0.0f)
    {
        collectSpawnTimer = randf(rng, 3.0f, 6.0f); // 间隔 3-6 秒生成一个
        collectibles.push_back(MakeCollectible(rng, floorTop));
    }

    const float pickupRadius = 0.6f;
    for (auto &c : collectibles)
    {
        if (!c.alive)
            continue;
        c.lifetime -= dt;
        if (c.lifetime <= 0.0f)
        {
            c.alive = false;
            continue;
        }
        // 拾取判定只看 XZ 平面距离（方块在地面上，玩家 Y 高度不同，用 3D 距离会导致永远碰不到）
        glm::vec2 dXZ(c.pos.x - player.pos.x, c.pos.z - player.pos.z);
        float distXZ = glm::length(dXZ);
        if (distXZ <= pickupRadius)
        {
            c.alive = false;
            score += 10;
        }
    }
    collectibles.erase(std::remove_if(collectibles.begin(), collectibles.end(),
                                      [](const Collectible &c)
                                      { return !c.alive; }),
                       collectibles.end());

    // 更新玩家 modelMatrix（把猫脚底对齐地面）
    {
        float scaleY = playerModel.modelScale.y; // uniform or per-axis
        // 保持脚底贴地的同时，叠加玩家跳跃产生的竖直位移
        float yOffset = player.pos.y - player.groundY; // 跳跃时相对于站立高度的增量
        float modelWorldY = floorTop - playerModel.bboxMin.y * scaleY + yOffset;
        glm::vec3 modelPosWorld(player.pos.x, modelWorldY, player.pos.z);
        // 让猫朝向当前视角方向（仅投影到水平面）
        glm::vec3 dirXZ(cameraFront.x, 0.0f, cameraFront.z);
        float len = glm::length(dirXZ);
        if (len < 1e-4f)
            dirXZ = glm::vec3(0.0f, 0.0f, -1.0f); // fallback
        else
            dirXZ /= len;

        // yaw 绕 Y 轴：atan2(x, z)；当 dirXZ = (0,0,-1) 时得到 pi，与之前固定的 180 度一致
        float rotRad = atan2(dirXZ.x, dirXZ.z);
        player.modelMatrix = MakeModelMatrix(modelPosWorld, glm::vec3(0, 1, 0), rotRad, playerModel.modelScale);
    }

    // 计算玩家的 AABB（world-space half extents），用于碰撞检测
    glm::vec3 playerHalfExtents = (playerModel.bboxMax - playerModel.bboxMin) * 0.5f * playerModel.modelScale;
    glm::vec3 playerMin = player.pos - playerHalfExtents;
    glm::vec3 playerMax = player.pos + playerHalfExtents;

    auto computeBoundingSphereRadius = [](const glm::vec3 &half) -> float
    {
        // approximate radius = length of half-extents vector
        return glm::length(half);
    };

    // build player OBB once per frame (use player.modelMatrix and playerModel.bboxMin/Max)
    OBB playerOBB = BuildOBBFromModel(playerModel.bboxMin, playerModel.bboxMax, player.modelMatrix);
    float playerSphereR = computeBoundingSphereRadius(glm::vec3(playerOBB.half[0], playerOBB.half[1], playerOBB.half[2]));

    for (size_t i = 0; i < falling.size(); ++i)
    {
        auto &o = falling[i];
        // build modelMatrix if you expect it prebuilt:
        glm::mat4 mm = o.modelMatrix;
        glm::vec4 center = mm * glm::vec4((fallingModels[o.modelIndex].bboxMin + fallingModels[o.modelIndex].bboxMax) * 0.5f, 1.0f);
    }

    for (auto &o : falling)
    {
        if (!o.alive)
            continue;

        // 1) physics integrate
        o.vel.y += -9.8f * dt * 0.2f * difficulty;
        o.pos.y += o.vel.y * dt;

        o.vel.x = 0.0f;
        o.vel.z = 0.0f;

        // 2) immediately update modelMatrix from current pos/rot/scale
        {
            glm::mat4 mm(1.0f);
            mm = glm::translate(mm, o.pos);
            mm = glm::scale(mm, o.modelScale);
            o.modelMatrix = mm;
        }

        // 3) build object OBB from proto bbox and the up-to-date modelMatrix
        const glm::vec3 &pbMin = fallingModels[o.modelIndex].bboxMin;
        const glm::vec3 &pbMax = fallingModels[o.modelIndex].bboxMax;
        OBB objOBB = BuildOBBFromModel(pbMin, pbMax, o.modelMatrix);

        // (optional) update instance halfExtents from OBB for consistent later use
        o.halfExtents = glm::vec3(objOBB.half[0], objOBB.half[1], objOBB.half[2]);

        // 4) broadphase sphere test vs player (playerOBB must be computed once per frame outside loop)
        float objSphereR = glm::length(glm::vec3(objOBB.half[0], objOBB.half[1], objOBB.half[2]));
        float centersDist = glm::length(objOBB.center - playerOBB.center);
        if (centersDist <= (playerSphereR + objSphereR))
        {
            // narrowphase SAT test (OBB vs OBB)
            if (OBBIntersectSAT(playerOBB, objOBB))
            {
                // 玩家受到一次伤害：扣除 1 点生命值
                if (playerHealth > 0)
                    playerHealth -= 1;

                std::cout << "[Collide] player hit by falling object, health = " << playerHealth << "\n";

                // 触发受击特效
                hitEffectTimer = 0.6f;

                if (playerHealth <= 0)
                {
                    playerDead = true;
                }

                // 该落物失效
                o.alive = false;
                break;
            }
        }

        // 5) ground contact test using OBB bottom (more robust than o.pos +/- halfExtents)
        float objBottomY = objOBB.center.y - objOBB.half[1];
        const float EPS = 1e-4f;
        if (objBottomY <= floorTop + EPS)
        {
            // snap object so its bottom sits exactly on floorTop
            o.pos.y = floorTop + objOBB.half[1];

            // update modelMatrix to reflect snapped position
            glm::mat4 mm(1.0f);
            mm = glm::translate(mm, o.pos);
            mm = glm::rotate(mm, o.rot, o.rotAxis);
            mm = glm::scale(mm, o.modelScale);
            o.modelMatrix = mm;

            o.vel = glm::vec3(0.0f);

            // optionally zero angular motion
            o.rotSpeed = 0.0f;

            o.alive = false; // or set state = LANDED if you want to keep it visible
            continue;
        }

        // If we reach here, the object continues falling that frame
    }
    // remove dead (landed or collided) instances
    falling.erase(std::remove_if(falling.begin(), falling.end(),
                                 [](const Falling &f)
                                 { return !f.alive; }),
                  falling.end());

    // 判断是否在移动（水平移动）
    glm::vec2 delta(player.pos.x - player.prevPos.x,
                    player.pos.z - player.prevPos.z);

    player.isMoving = (glm::length(delta) > 0.001f);

    // 保存上一帧位置
    player.prevPos = player.pos;
}

void Game::Render(unsigned int shader3D, float dt, const glm::vec3 &cameraPos)
{
    /* =========================================================
       1. 计算太阳光矩阵（Directional Light）
       ========================================================= */
    glm::vec3 sunDir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.2f));

    float lightDist = 20.0f;
    glm::mat4 lightView = glm::lookAt(
        -sunDir * lightDist,
        glm::vec3(0.0f),
        glm::vec3(0, 1, 0));

    float orthoSize = 15.0f;
    glm::mat4 lightProj = glm::ortho(
        -orthoSize, orthoSize,
        -orthoSize, orthoSize,
        1.0f, 50.0f);

    glm::mat4 lightVP = lightProj * lightView;

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    /* =========================================================
       2. Shadow Pass（只画深度，只画真实模型）
       ========================================================= */
    if (depthFBO && shadowShader)
    {
        glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
        glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);

        glUseProgram(shadowShader);
        glUniformMatrix4fv(
            glGetUniformLocation(shadowShader, "uLightVP"),
            1, GL_FALSE, &lightVP[0][0]);

        auto setShadowModel = [&](const glm::mat4 &m)
        {
            glUniformMatrix4fv(
                glGetUniformLocation(shadowShader, "uModel"),
                1, GL_FALSE, &m[0][0]);
        };

        /* ---- floor ---- */
        {
            glm::mat4 m = floorModel.modelMatrix; // 已在初始化阶段算好
            setShadowModel(m);
            floorModel.DrawDepth();
        }

        /* ---- player ---- */
        {
            glm::mat4 m = player.modelMatrix;
            setShadowModel(m);
            playerModel.animEnable = player.isMoving;
            playerModel.DrawAnimated(m, dt, shadowShader);
            playerModel.DrawDepth();
        }

        /* ---- falling objects ---- */
        for (auto &o : falling)
        {
            glm::mat4 m = o.modelMatrix;
            setShadowModel(m);
            fallingModels[o.modelIndex].DrawDepth();
        }

        glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(prevViewport[0], prevViewport[1],
                   prevViewport[2], prevViewport[3]);
    }

    /* =========================================================
       3. Main Pass（正常渲染）
       ========================================================= */
    glUseProgram(shader3D);

    /* ---- camera & light ---- */
    glUniform3fv(
        glGetUniformLocation(shader3D, "uViewPos"),
        1, &cameraPos[0]);

    glUniform3fv(
        glGetUniformLocation(shader3D, "uLightDir"),
        1, &sunDir[0]);

    glUniform3f(
        glGetUniformLocation(shader3D, "uLightColor"),
        1.0f, 0.98f, 0.9f);

    glUniform1f(
        glGetUniformLocation(shader3D, "uLightIntensity"),
        1.2f);

    /* ---- shadow uniforms ---- */
    glUniformMatrix4fv(
        glGetUniformLocation(shader3D, "uLightVP"),
        1, GL_FALSE, &lightVP[0][0]);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glUniform1i(
        glGetUniformLocation(shader3D, "uShadowMap"),
        3);

    auto setModelAndNormal = [&](const glm::mat4 &m)
    {
        glUniformMatrix4fv(
            glGetUniformLocation(shader3D, "uModel"),
            1, GL_FALSE, &m[0][0]);

        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(m)));
        glUniformMatrix3fv(
            glGetUniformLocation(shader3D, "uNormalMat"),
            1, GL_FALSE, &normalMat[0][0]);
    };
    glEnableVertexAttribArray(1); // normal attribute
    /* ---- floor ---- */
    {
        setModelAndNormal(floorModel.modelMatrix);

        glUniform1i(glGetUniformLocation(shader3D, "uHasDiffuse"), 1);
        glUniform1i(glGetUniformLocation(shader3D, "uUseAlphaTest"), 0);
        glUniform1i(glGetUniformLocation(shader3D, "uDiffuseMap"), 0);

        glActiveTexture(GL_TEXTURE0);
        floorModel.Draw(shader3D);
    }

    /* ---- player ---- */
    {
        setModelAndNormal(player.modelMatrix);
        float horizSpeed = glm::length(glm::vec2(player.pos.x, player.pos.z)); // world units/s
        float maxSpeed = 3.0f;                                                 // tune to match your control speed
        float speedFactor = glm::clamp(horizSpeed / maxSpeed, 0.0f, 1.0f);
        glm::vec3 playerScale = glm::vec3(1.0f); // tune as needed, maybe playerModel.modelScale
        glUniform1i(glGetUniformLocation(shader3D, "uHasDiffuse"), 1);
        glUniform1i(glGetUniformLocation(shader3D, "uUseAlphaTest"), 1);
        glUniform1f(glGetUniformLocation(shader3D, "uAlphaCutoff"), 0.3f);
        glUniform1i(glGetUniformLocation(shader3D, "uDiffuseMap"), 0);
        // 告诉模型当前是否在移动
        playerModel.animEnable = player.isMoving;
        glUseProgram(shader3D);
        glActiveTexture(GL_TEXTURE0);

        // set uViewPos if used
        // glUniform3fv(glGetUniformLocation(shader3D.ID, "uViewPos"), 1, &cameraPos[0]);
        playerModel.DrawAnimated(player.modelMatrix, dt, shader3D);
    }

    /* ---- falling objects ---- */
    for (auto &o : falling)
    {
        setModelAndNormal(o.modelMatrix);

        glUniform1i(glGetUniformLocation(shader3D, "uHasDiffuse"), 1);
        glUniform1i(glGetUniformLocation(shader3D, "uUseAlphaTest"), 0);
        glUniform1i(glGetUniformLocation(shader3D, "uDiffuseMap"), 0);

        glActiveTexture(GL_TEXTURE0);
        fallingModels[o.modelIndex].Draw(shader3D);
    }

    /* ---- collectibles (colored cubes) ---- */
    if (cubeVAO)
    {
        // 使用常量法线，避免缺失顶点法线导致错误 lighting
        glDisableVertexAttribArray(1); // normal attribute
        glVertexAttrib3f(1, 0.0f, 1.0f, 0.0f);
        glDisableVertexAttribArray(2); // uv attribute, if any

        glUniform1i(glGetUniformLocation(shader3D, "uHasDiffuse"), 0);
        glUniform1i(glGetUniformLocation(shader3D, "uUseAlphaTest"), 0);

        glBindVertexArray(cubeVAO);
        for (auto &c : collectibles)
        {
            glm::mat4 m(1.0f);
            m = glm::translate(m, c.pos);
            m = glm::scale(m, glm::vec3(0.4f)); // 小立方体边长约 0.4
            setModelAndNormal(m);

            // 用 uMatDiffuse 传颜色
            glUniform3fv(glGetUniformLocation(shader3D, "uMatDiffuse"), 1, &c.color[0]);

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glBindVertexArray(0);

        // 下帧渲染其他模型会重新启用属性
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}
