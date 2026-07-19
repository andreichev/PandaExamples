#include "Physics/VoxelCharacterController.hpp"

#include "Model/GameContext.hpp"
#include "Model/TerrainAccess.hpp"

#include <algorithm>
#include <cmath>

// Физика ходит по ВИДИМОЙ геометрии: природный рельеф — изоповерхность того же density-поля,
// что строит движковый marching cubes (трилинейный семпл узловой плотности, iso 0.5),
// рукотворные блоки — кубы BuildingCellGrid (swept AABB). Рельеф читается ОКНОМ через TerrainAPI
// (один батч-вызов на несколько кадров), семплы поля — по окну без ABI-обращений.

namespace {

constexpr float COLLISION_EPSILON = 0.001f;
constexpr float FIELD_ISO = 0.5f;
constexpr float FIELD_SCAN_STEP = 0.25f; // шаг сканирования столбца при поиске поверхности

// Окно рельефа вокруг персонажа: перечитывается при выходе капсулы из запаса.
constexpr int WINDOW_RADIUS_XZ = 7;
constexpr int WINDOW_RADIUS_Y = 11;
constexpr int WINDOW_REFRESH_MARGIN = 4; // осталось меньше запаса до края — перечитать

TerrainAccess::Window s_fieldWindow;
bool s_fieldWindowValid = false;

struct Aabb {
    glm::vec3 min;
    glm::vec3 max;
};

void ensureFieldWindow(const glm::vec3 &eyePosition) {
    const int centerX = static_cast<int>(std::floor(eyePosition.x));
    const int centerY = static_cast<int>(std::floor(eyePosition.y));
    const int centerZ = static_cast<int>(std::floor(eyePosition.z));
    if (s_fieldWindowValid &&
        centerX - WINDOW_REFRESH_MARGIN >= s_fieldWindow.minX + 1 &&
        centerX + WINDOW_REFRESH_MARGIN <= s_fieldWindow.maxX - 1 &&
        centerY - WINDOW_REFRESH_MARGIN >= s_fieldWindow.minY + 1 &&
        centerY + WINDOW_REFRESH_MARGIN <= s_fieldWindow.maxY - 1 &&
        centerZ - WINDOW_REFRESH_MARGIN >= s_fieldWindow.minZ + 1 &&
        centerZ + WINDOW_REFRESH_MARGIN <= s_fieldWindow.maxZ - 1) {
        return;
    }
    s_fieldWindow = TerrainAccess::readWindow(
        centerX - WINDOW_RADIUS_XZ, centerY - WINDOW_RADIUS_Y, centerZ - WINDOW_RADIUS_XZ,
        centerX + WINDOW_RADIUS_XZ, centerY + WINDOW_RADIUS_Y, centerZ + WINDOW_RADIUS_XZ
    );
    s_fieldWindowValid = true;
}

// Рукотворный куб (только они коллизятся как кубы; рельеф — полем).
bool isConstructedVoxel(int x, int y, int z) {
    return GameContext::s_buildingGrid != nullptr && GameContext::s_buildingGrid->isPhysicsSolidAt(x, y, z);
}

// Природный воксель для density-поля. Край участка — невидимая стена, ниже дна — твердь.
bool isNaturalSolidAt(int x, int y, int z) {
    if (!TerrainAccess::isInsideXZ(x, z)) { return y < TerrainAccess::worldMaxY(); }
    if (y < 0) { return true; }
    return s_fieldWindow.layerAt(x, y, z) != 0;
}

// Плотность узла решётки (угол воксела) — как NodeField в TerrainMeshGenerator: доля ПРИРОДНЫХ
// среди 8 смежных вокселей (постройки коллизятся кубами, в поле не участвуют).
float nodeDensity(int nx, int ny, int nz) {
    int solid = 0;
    for (int dy = -1; dy <= 0; dy++) {
        for (int dx = -1; dx <= 0; dx++) {
            for (int dz = -1; dz <= 0; dz++) {
                if (isNaturalSolidAt(nx + dx, ny + dy, nz + dz)) { solid++; }
            }
        }
    }
    return solid / 8.f;
}

// Трилинейный семпл density в мировой точке — гладкая версия поверхности marching cubes.
float sampleDensity(const glm::vec3 &point) {
    const int baseX = static_cast<int>(std::floor(point.x));
    const int baseY = static_cast<int>(std::floor(point.y));
    const int baseZ = static_cast<int>(std::floor(point.z));
    const float fx = point.x - baseX;
    const float fy = point.y - baseY;
    const float fz = point.z - baseZ;

    float corners[2][2][2];
    for (int dx = 0; dx <= 1; dx++) {
        for (int dy = 0; dy <= 1; dy++) {
            for (int dz = 0; dz <= 1; dz++) {
                corners[dx][dy][dz] = nodeDensity(baseX + dx, baseY + dy, baseZ + dz);
            }
        }
    }
    const float x00 = corners[0][0][0] + (corners[1][0][0] - corners[0][0][0]) * fx;
    const float x01 = corners[0][0][1] + (corners[1][0][1] - corners[0][0][1]) * fx;
    const float x10 = corners[0][1][0] + (corners[1][1][0] - corners[0][1][0]) * fx;
    const float x11 = corners[0][1][1] + (corners[1][1][1] - corners[0][1][1]) * fx;
    const float y0 = x00 + (x10 - x00) * fy;
    const float y1 = x01 + (x11 - x01) * fy;
    return y0 + (y1 - y0) * fz;
}

bool fieldSolidAt(const glm::vec3 &point) {
    return sampleDensity(point) > FIELD_ISO;
}

// Высота поверхности поля в столбе (x, z): наибольший переход воздух→земля в [yBottom, yTop].
// Возвращает true и y поверхности, если найден.
bool fieldSurfaceY(float x, float z, float yTop, float yBottom, float &outSurfaceY) {
    if (yTop <= yBottom) { return false; }
    float previousY = yTop;
    float previousDensity = sampleDensity(glm::vec3(x, yTop, z));
    if (previousDensity > FIELD_ISO) {
        return false; // старт уже в земле — это стена/внутренность, не опора
    }
    for (float y = yTop - FIELD_SCAN_STEP;; y -= FIELD_SCAN_STEP) {
        const float clampedY = std::max(y, yBottom);
        const float density = sampleDensity(glm::vec3(x, clampedY, z));
        if (density > FIELD_ISO) {
            // Переход между previousY (воздух) и clampedY (земля) — уточняем бисекцией.
            float airY = previousY;
            float groundY = clampedY;
            for (int i = 0; i < 6; i++) {
                const float midY = (airY + groundY) * 0.5f;
                if (sampleDensity(glm::vec3(x, midY, z)) > FIELD_ISO) { groundY = midY; }
                else {
                    airY = midY;
                }
            }
            outSurfaceY = (airY + groundY) * 0.5f;
            return true;
        }
        previousY = clampedY;
        previousDensity = density;
        if (clampedY <= yBottom) { break; }
    }
    return false;
}

// Пол поля под основанием капсулы: max по центру и 4 точкам радиуса.
bool fieldFloorUnder(
    const glm::vec3 &eyePosition, const VoxelCharacterConfig &config, float yTop, float yBottom, float &outFloorY
) {
    const float feetX = eyePosition.x;
    const float feetZ = eyePosition.z;
    const float supportRadius = config.radius * 0.7f;
    const float offsets[5][2] = {
        {0.f, 0.f},
        {supportRadius, 0.f},
        {-supportRadius, 0.f},
        {0.f, supportRadius},
        {0.f, -supportRadius},
    };
    bool found = false;
    float best = 0.f;
    for (const auto &offset : offsets) {
        float surfaceY;
        if (fieldSurfaceY(feetX + offset[0], feetZ + offset[1], yTop, yBottom, surfaceY)) {
            if (!found || surfaceY > best) { best = surfaceY; }
            found = true;
        }
    }
    if (found) { outFloorY = best; }
    return found;
}

Aabb makeBodyAabb(const glm::vec3 &eyePosition, const VoxelCharacterConfig &config) {
    return Aabb{
        glm::vec3(
            eyePosition.x - config.radius,
            eyePosition.y - config.eyeHeight,
            eyePosition.z - config.radius
        ),
        glm::vec3(
            eyePosition.x + config.radius,
            eyePosition.y - config.eyeHeight + config.height,
            eyePosition.z + config.radius
        )
    };
}

template <typename Callback>
bool forEachConstructedVoxel(const Aabb &aabb, Callback callback) {
    const int minX = static_cast<int>(std::floor(aabb.min.x + COLLISION_EPSILON));
    const int minY = static_cast<int>(std::floor(aabb.min.y + COLLISION_EPSILON));
    const int minZ = static_cast<int>(std::floor(aabb.min.z + COLLISION_EPSILON));
    const int maxX = static_cast<int>(std::floor(aabb.max.x - COLLISION_EPSILON));
    const int maxY = static_cast<int>(std::floor(aabb.max.y - COLLISION_EPSILON));
    const int maxZ = static_cast<int>(std::floor(aabb.max.z - COLLISION_EPSILON));

    bool found = false;
    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                if (!isConstructedVoxel(x, y, z)) { continue; }
                found = true;
                callback(x, y, z);
            }
        }
    }
    return found;
}

float sanitizePositive(float value, float fallback) {
    return value > 0.0f ? value : fallback;
}

} // namespace

void VoxelCharacterController::invalidateFieldCache() {
    s_fieldWindowValid = false;
}

void VoxelCharacterController::move(
    glm::vec3 &eyePosition,
    VoxelCharacterState &state,
    const VoxelCharacterConfig &config,
    const VoxelCharacterInput &input,
    float deltaTime
) {
    if (!TerrainAccess::isReady()) { return; }
    ensureFieldWindow(eyePosition);

    const float maxFrameTime = sanitizePositive(config.maxFrameTime, 0.1f);
    const float maxSubstep = sanitizePositive(config.maxSubstep, 0.02f);
    const float clampedDeltaTime = std::clamp(deltaTime, 0.0f, maxFrameTime);
    const int substeps =
        std::max(1, static_cast<int>(std::ceil(clampedDeltaTime / maxSubstep)));
    const float substepDeltaTime = clampedDeltaTime / static_cast<float>(substeps);

    if (input.flyMode) {
        state.grounded = false;
        state.coyoteTimer = 0.0f;
        state.verticalVelocity = 0.0f;
        for (int step = 0; step < substeps; step++) {
            eyePosition += input.horizontalDirection * input.horizontalSpeed * substepDeltaTime;
            eyePosition.y += input.flyVerticalInput * input.horizontalSpeed * substepDeltaTime;
        }
        return;
    }

    bool jumpPending = input.jumpPressed;
    for (int step = 0; step < substeps; step++) {
        pushOutOfField(eyePosition, state, config);
        const glm::vec3 horizontalTranslation =
            input.horizontalDirection * input.horizontalSpeed * substepDeltaTime;
        moveHorizontalWithStep(eyePosition, state, config, horizontalTranslation);

        if (state.grounded) { state.coyoteTimer = std::max(config.coyoteTime, 0.0f); }
        else {
            state.coyoteTimer = std::max(0.0f, state.coyoteTimer - substepDeltaTime);
        }

        if (jumpPending && state.coyoteTimer > 0.0f) {
            state.verticalVelocity = config.jumpSpeed;
            state.grounded = false;
            state.coyoteTimer = 0.0f;
            jumpPending = false;
        }

        if (state.grounded && state.verticalVelocity < 0.0f) { state.verticalVelocity = 0.0f; }
        state.verticalVelocity -= config.gravity * substepDeltaTime;
        state.verticalVelocity = std::max(state.verticalVelocity, -config.maxFallSpeed);

        moveVertical(eyePosition, state, config, state.verticalVelocity * substepDeltaTime);

        if (!state.grounded && state.verticalVelocity <= 0.0f &&
            snapToGround(eyePosition, config, config.groundSnapDistance)) {
            state.grounded = true;
            state.coyoteTimer = std::max(config.coyoteTime, 0.0f);
            state.verticalVelocity = 0.0f;
        }
    }
}

// Ноги оказались внутри поля (насыпали кистью под собой) — мягко поднять на поверхность.
void VoxelCharacterController::pushOutOfField(
    glm::vec3 &eyePosition, VoxelCharacterState &state, const VoxelCharacterConfig &config
) {
    const float feetY = eyePosition.y - config.eyeHeight;
    if (!fieldSolidAt(glm::vec3(eyePosition.x, feetY + 0.05f, eyePosition.z))) { return; }

    constexpr float MAX_PUSH_UP = 3.f;
    for (float y = feetY + FIELD_SCAN_STEP; y <= feetY + MAX_PUSH_UP; y += FIELD_SCAN_STEP) {
        if (sampleDensity(glm::vec3(eyePosition.x, y, eyePosition.z)) <= FIELD_ISO) {
            // Первый воздух над ногами: уточняем границу бисекцией и встаём на неё.
            float groundY = y - FIELD_SCAN_STEP;
            float airY = y;
            for (int i = 0; i < 6; i++) {
                const float midY = (airY + groundY) * 0.5f;
                if (sampleDensity(glm::vec3(eyePosition.x, midY, eyePosition.z)) > FIELD_ISO) {
                    groundY = midY;
                } else {
                    airY = midY;
                }
            }
            eyePosition.y = (airY + groundY) * 0.5f + config.eyeHeight;
            state.grounded = true;
            if (state.verticalVelocity < 0.0f) { state.verticalVelocity = 0.0f; }
            return;
        }
    }
    // Глубоко в тверди (плохой спавн/сейв, обвал кисти): локальный скан не спас —
    // телепорт на поверхность столбца, иначе падение в толще не закончится никогда.
    float surfaceHeight;
    Vec3 surfaceNormal;
    if (TerrainAccess::sampleSurface(eyePosition.x, eyePosition.z, surfaceHeight, surfaceNormal)) {
        eyePosition.y = surfaceHeight + config.eyeHeight + 0.05f;
        state.grounded = true;
        if (state.verticalVelocity < 0.0f) { state.verticalVelocity = 0.0f; }
        VoxelCharacterController::invalidateFieldCache(); // окно после телепорта устарело
    }
}

// Вертикаль: кубовый свип + пол/потолок density-поля.
void VoxelCharacterController::moveVertical(
    glm::vec3 &eyePosition,
    VoxelCharacterState &state,
    const VoxelCharacterConfig &config,
    float translation
) {
    const float feetBefore = eyePosition.y - config.eyeHeight;

    bool hitGround = false;
    bool hitCeiling = false;
    moveAxis(eyePosition, config, 1, translation, hitGround, hitCeiling);

    if (translation <= 0.0f) {
        // Падение/опора: пол поля между прежним положением ног и новым.
        const float feetAfter = eyePosition.y - config.eyeHeight;
        float fieldFloor;
        if (fieldFloorUnder(eyePosition, config, feetBefore + 0.5f, feetAfter - 0.05f, fieldFloor) &&
            fieldFloor > feetAfter) {
            eyePosition.y = fieldFloor + config.eyeHeight;
            hitGround = true;
        }
    } else {
        // Подъём: потолок поля у макушки (свод пещеры/навес рельефа).
        const glm::vec3 head(
            eyePosition.x, eyePosition.y - config.eyeHeight + config.height, eyePosition.z
        );
        if (fieldSolidAt(head)) {
            eyePosition.y = feetBefore + config.eyeHeight; // откат подъёма
            hitCeiling = true;
        }
    }

    if (hitGround) {
        state.grounded = true;
        state.coyoteTimer = std::max(config.coyoteTime, 0.0f);
        if (state.verticalVelocity < 0.0f) { state.verticalVelocity = 0.0f; }
    } else {
        state.grounded = false;
    }
    if (hitCeiling && state.verticalVelocity > 0.0f) { state.verticalVelocity = 0.0f; }
}

bool VoxelCharacterController::moveHorizontalWithStep(
    glm::vec3 &eyePosition,
    VoxelCharacterState &state,
    const VoxelCharacterConfig &config,
    const glm::vec3 &translation
) {
    if (glm::length(translation) < 0.0001f) { return false; }

    const glm::vec3 originalPosition = eyePosition;
    bool hitNegative = false;
    bool hitPositive = false;
    bool collided = false;
    collided |= moveAxis(eyePosition, config, 0, translation.x, hitNegative, hitPositive);
    collided |= moveAxis(eyePosition, config, 2, translation.z, hitNegative, hitPositive);

    // Гард поля: слишком крутой подъём поверхности = стена — откат горизонтали.
    // Проходимый подъём за подшаг: stepHeight на земле, небольшой зазор в воздухе.
    const float allowedRise = state.grounded ? config.stepHeight : 0.25f;
    const float feetY = eyePosition.y - config.eyeHeight;
    const glm::vec3 riseProbe(eyePosition.x, feetY + allowedRise, eyePosition.z);
    if (fieldSolidAt(riseProbe)) {
        eyePosition.x = originalPosition.x;
        eyePosition.z = originalPosition.z;
        return true;
    }
    // Плавный подъём по склону: ноги ниже поверхности поля → встать на неё (в пределах allowedRise).
    float fieldFloor;
    if (fieldFloorUnder(eyePosition, config, feetY + allowedRise, feetY, fieldFloor) &&
        fieldFloor > feetY + COLLISION_EPSILON) {
        eyePosition.y = fieldFloor + config.eyeHeight;
        if (state.grounded) { state.verticalVelocity = 0.0f; }
    }

    if (!collided || !state.grounded) { return collided; }

    // Step-механика рукотворных кубов (подъём на блок высотой до stepHeight).
    glm::vec3 steppedPosition = originalPosition;
    bool hitGround = false;
    bool hitCeiling = false;
    if (moveAxis(steppedPosition, config, 1, config.stepHeight, hitGround, hitCeiling)) {
        return collided;
    }

    bool stepCollided = false;
    stepCollided |= moveAxis(steppedPosition, config, 0, translation.x, hitNegative, hitPositive);
    stepCollided |= moveAxis(steppedPosition, config, 2, translation.z, hitNegative, hitPositive);
    if (stepCollided) { return collided; }

    hitGround = false;
    hitCeiling = false;
    moveAxis(steppedPosition, config, 1, -config.stepHeight, hitGround, hitCeiling);
    if (!hitGround) { return collided; }

    eyePosition = steppedPosition;
    state.grounded = true;
    state.verticalVelocity = 0.0f;
    return true;
}

bool VoxelCharacterController::moveAxis(
    glm::vec3 &eyePosition,
    const VoxelCharacterConfig &config,
    int axis,
    float translation,
    bool &hitNegative,
    bool &hitPositive
) {
    hitNegative = false;
    hitPositive = false;
    if (std::abs(translation) < 0.000001f) { return false; }

    eyePosition[axis] += translation;
    const bool collided =
        forEachConstructedVoxel(makeBodyAabb(eyePosition, config), [&](int x, int y, int z) {
            if (axis == 0) {
                if (translation > 0.0f) {
                    eyePosition.x =
                        std::min(eyePosition.x, static_cast<float>(x) - config.radius - COLLISION_EPSILON);
                    hitPositive = true;
                } else {
                    eyePosition.x = std::max(
                        eyePosition.x,
                        static_cast<float>(x + 1) + config.radius + COLLISION_EPSILON
                    );
                    hitNegative = true;
                }
            } else if (axis == 1) {
                if (translation > 0.0f) {
                    eyePosition.y = std::min(
                        eyePosition.y,
                        static_cast<float>(y) - config.height + config.eyeHeight - COLLISION_EPSILON
                    );
                    hitPositive = true;
                } else {
                    eyePosition.y = std::max(
                        eyePosition.y,
                        static_cast<float>(y + 1) + config.eyeHeight + COLLISION_EPSILON
                    );
                    hitNegative = true;
                }
            } else {
                if (translation > 0.0f) {
                    eyePosition.z =
                        std::min(eyePosition.z, static_cast<float>(z) - config.radius - COLLISION_EPSILON);
                    hitPositive = true;
                } else {
                    eyePosition.z = std::max(
                        eyePosition.z,
                        static_cast<float>(z + 1) + config.radius + COLLISION_EPSILON
                    );
                    hitNegative = true;
                }
            }
        });
    return collided;
}

bool VoxelCharacterController::snapToGround(
    glm::vec3 &eyePosition, const VoxelCharacterConfig &config, float maxDistance
) {
    if (maxDistance <= 0.0f) { return false; }

    const float feetY = eyePosition.y - config.eyeHeight;
    float fieldFloor;
    if (fieldFloorUnder(eyePosition, config, feetY + COLLISION_EPSILON, feetY - maxDistance, fieldFloor)) {
        eyePosition.y = fieldFloor + config.eyeHeight;
        return true;
    }

    const glm::vec3 originalPosition = eyePosition;
    bool hitGround = false;
    bool hitCeiling = false;
    if (moveAxis(eyePosition, config, 1, -maxDistance, hitGround, hitCeiling) && hitGround) {
        return true;
    }

    eyePosition = originalPosition;
    return false;
}
