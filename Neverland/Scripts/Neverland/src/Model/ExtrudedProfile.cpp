#include "ExtrudedProfile.hpp"

#include <cmath>

namespace ExtrudedProfile {

namespace {

    constexpr int BLOCKS_ATLAS_GRID = 7;

    Vec3 add(const Vec3 &a, const Vec3 &b) {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }

    Vec3 subtract(const Vec3 &a, const Vec3 &b) {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    Vec3 scale(const Vec3 &a, float s) {
        return {a.x * s, a.y * s, a.z * s};
    }

    Vec3 cross(const Vec3 &a, const Vec3 &b) {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }

    Vec3 normalize(const Vec3 &a) {
        const float length = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
        if (length < 1e-6f) { return {0.f, 0.f, 0.f}; }
        return scale(a, 1.f / length);
    }

    Color hexColor(uint32_t hex) {
        return Color(
            (hex >> 24) / 255.f, ((hex >> 16) & 0xFF) / 255.f, ((hex >> 8) & 0xFF) / 255.f,
            (hex & 0xFF) / 255.f
        );
    }

    // Позиция точки профиля на кольце (up мировой — профили архитектуры вертикальны).
    Vec3 ringPoint(const PathRing &ring, const ProfilePoint &point) {
        return add(add(ring.base, scale(ring.side, point.offset)), Vec3(0.f, point.height, 0.f));
    }

} // namespace

void extrude(
    const std::vector<ProfilePoint> &profile,
    const std::vector<PathRing> &rings,
    bool closed,
    uint8_t tileIndex,
    uint32_t tint,
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices
) {
    if (profile.size() < 2 || rings.size() < 2) { return; }
    const float uvSize = 1.f / BLOCKS_ATLAS_GRID - 0.001f;
    const float tileU = tileIndex % BLOCKS_ATLAS_GRID / static_cast<float>(BLOCKS_ATLAS_GRID) + 0.0005f;
    const float tileV = tileIndex / BLOCKS_ATLAS_GRID / static_cast<float>(BLOCKS_ATLAS_GRID) + 0.0005f;
    const Color color = hexColor(tint);

    // Нормировка сечения в V-тайл: по накопленной длине контура.
    std::vector<float> profileV(profile.size(), 0.f);
    float profileLength = 0.f;
    for (size_t j = 1; j < profile.size(); j++) {
        const float du = profile[j].offset - profile[j - 1].offset;
        const float dh = profile[j].height - profile[j - 1].height;
        profileLength += std::sqrt(du * du + dh * dh);
        profileV[j] = profileLength;
    }
    if (profileLength > 1e-5f) {
        for (float &v : profileV) {
            v /= profileLength;
        }
    }

    const size_t segmentCount = closed ? rings.size() : rings.size() - 1;
    for (size_t i = 0; i < segmentCount; i++) {
        const PathRing &ringA = rings[i];
        const PathRing &ringB = rings[(i + 1) % rings.size()];
        if (!ringA.chunkOwned) { continue; }
        // Продольный UV: сегменты не пересекают границы метров (кольца на границах
        // ячеек и центрах) — тайл не покидаем. Длина — по базам колец (устойчиво
        // к замыканию цикла, где along следующего кольца меньше текущего).
        const float segmentDx = ringB.base.x - ringA.base.x;
        const float segmentDz = ringB.base.z - ringA.base.z;
        const float segmentLength = std::sqrt(segmentDx * segmentDx + segmentDz * segmentDz);
        const float texU0 = ringA.along - std::floor(ringA.along + 1e-4f);
        const float texU1 = texU0 + segmentLength;
        for (size_t j = 0; j + 1 < profile.size(); j++) {
            const Vec3 p0 = ringPoint(ringA, profile[j]);
            const Vec3 p1 = ringPoint(ringB, profile[j]);
            const Vec3 p2 = ringPoint(ringB, profile[j + 1]);
            const Vec3 p3 = ringPoint(ringA, profile[j + 1]);
            // Фасетная нормаль: путь × профиль (профиль обходится так, чтобы она была наружной).
            const Vec3 normal = normalize(cross(subtract(p1, p0), subtract(p3, p0)));
            const uint32_t base = static_cast<uint32_t>(vertices.size());
            for (uint32_t index : {base, base + 1u, base + 2u, base + 2u, base + 3u, base}) {
                indices.emplace_back(index);
            }
            const float v0 = tileV + profileV[j] * uvSize;
            const float v1 = tileV + profileV[j + 1] * uvSize;
            vertices.emplace_back(Vertex(p0, {tileU + texU0 * uvSize, v0}, normal, color, 1.f));
            vertices.emplace_back(Vertex(p1, {tileU + texU1 * uvSize, v0}, normal, color, 1.f));
            vertices.emplace_back(Vertex(p2, {tileU + texU1 * uvSize, v1}, normal, color, 1.f));
            vertices.emplace_back(Vertex(p3, {tileU + texU0 * uvSize, v1}, normal, color, 1.f));
        }
    }

    if (closed) { return; }
    // Торцы открытых концов: фан от центроида сечения (контур почти звёздный — приемлемо).
    const auto emitEndCap = [&](const PathRing &ring, const Vec3 &outwardAlong, bool chunkOwned) {
        if (!chunkOwned) { return; }
        ProfilePoint centroid{0.f, 0.f};
        for (const ProfilePoint &point : profile) {
            centroid.offset += point.offset;
            centroid.height += point.height;
        }
        centroid.offset /= static_cast<float>(profile.size());
        centroid.height /= static_cast<float>(profile.size());
        const Vec3 centerPoint = ringPoint(ring, centroid);
        const Vec3 normal = normalize(outwardAlong);
        // Порядок обхода: наружная нормаль торца по outwardAlong.
        for (size_t j = 0; j + 1 < profile.size(); j++) {
            Vec3 pA = ringPoint(ring, profile[j]);
            Vec3 pB = ringPoint(ring, profile[j + 1]);
            const Vec3 e1 = subtract(pA, centerPoint);
            const Vec3 e2 = subtract(pB, centerPoint);
            const Vec3 faceNormal = cross(e1, e2);
            const bool flip =
                faceNormal.x * normal.x + faceNormal.y * normal.y + faceNormal.z * normal.z < 0.f;
            const uint32_t base = static_cast<uint32_t>(vertices.size());
            const Vec2 uvCenter(tileU + 0.5f * uvSize, tileV + 0.5f * uvSize);
            vertices.emplace_back(Vertex(centerPoint, uvCenter, normal, color, 1.f));
            vertices.emplace_back(Vertex(flip ? pB : pA, uvCenter, normal, color, 1.f));
            vertices.emplace_back(Vertex(flip ? pA : pB, uvCenter, normal, color, 1.f));
            for (uint32_t index : {base, base + 1u, base + 2u}) {
                indices.emplace_back(index);
            }
        }
    };
    // Торец начала: наружу против хода; принадлежность — первому отрезку.
    emitEndCap(
        rings.front(), normalize(subtract(rings.front().base, rings[1].base)), rings.front().chunkOwned
    );
    emitEndCap(
        rings.back(), normalize(subtract(rings.back().base, rings[rings.size() - 2].base)),
        rings[rings.size() - 2].chunkOwned
    );
}

} // namespace ExtrudedProfile
