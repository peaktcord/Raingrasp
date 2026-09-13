#include "src/common/render/map_atlas.hpp"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <map>

namespace mapatlas {

namespace {

const uint32_t kWall = 0x2A2A2Au;
const uint32_t kFloor = 0xDCDCDCu;
const uint32_t kPortal = 0xCC00FFu;
const uint32_t kMonster = 0xE02020u;
const uint32_t kChest = 0x3060FFu;
const uint32_t kItem = 0x30C060u;
const uint32_t kNpc = 0xFFD000u;
const uint32_t kBackground = 0x101014u;
const uint32_t kOutline = 0x4A4A5Au;

const int32_t kHeader = 16;
const int32_t kLegend = 16;
const int32_t kMargin = 4;

uint32_t markerColor(MarkerKind kind) {
    switch (kind) {
        case CHEST: return kChest;
        case ITEM: return kItem;
        default: return kMonster;
    }
}

uint32_t squareColor(int8_t square) {
    if ((square & 0x20) != 0) return kNpc;
    if ((square & 8) != 0) return kPortal;
    return (square & 1) != 0 ? kWall : kFloor;
}

void drawCells(render::SoftGraphics *graphics, const Dungeon &dungeon, int32_t originX,
               int32_t originY, int32_t cell) {
    for (int32_t x = 0; x < dungeon.width; ++x) {
        for (int32_t y = 0; y < dungeon.height; ++y) {
            graphics->setColor((int32_t)squareColor(dungeon.at(x, y)));
            graphics->fillRect(originX + x * cell, originY + y * cell, cell, cell);
        }
    }
    int32_t inset = cell >= 6 ? 1 : 0;
    for (size_t n = 0; n < dungeon.markers.size(); ++n) {
        const Marker &marker = dungeon.markers[n];
        if (marker.x < 0 || marker.x >= dungeon.width || marker.y < 0 ||
            marker.y >= dungeon.height) {
            continue;
        }
        graphics->setColor((int32_t)markerColor(marker.kind));
        graphics->fillRect(originX + marker.x * cell + inset, originY + marker.y * cell + inset,
                           cell - 2 * inset, cell - 2 * inset);
    }
}

void legendEntry(render::SoftGraphics *graphics, int32_t *x, int32_t y, uint32_t color,
                 const char *label) {
    graphics->setColor((int32_t)color);
    graphics->fillRect(*x, y + 3, 7, 7);
    graphics->setColor(0xC8C8C8);
    graphics->drawString(std::string(label), *x + 10, y, render::LEFT | render::TOP);
    *x += 10 + (int32_t)std::string(label).size() * 6 + 8;
}

void drawLegend(render::SoftGraphics *graphics, int32_t x, int32_t y, bool withNpc) {
    graphics->setFont(Font::getFont(0, 1, 8));
    legendEntry(graphics, &x, y, kMonster, "monster");
    legendEntry(graphics, &x, y, kChest, "chest");
    legendEntry(graphics, &x, y, kItem, "item");
    if (withNpc) legendEntry(graphics, &x, y, kNpc, "npc");
    legendEntry(graphics, &x, y, kPortal, "portal");
}

bool hasNpc(const Dungeon &dungeon) {
    for (int32_t x = 0; x < dungeon.width; ++x) {
        for (int32_t y = 0; y < dungeon.height; ++y) {
            if ((dungeon.at(x, y) & 0x20) != 0) return true;
        }
    }
    return false;
}

struct Placement {
    int32_t x = 0;
    int32_t y = 0;
    bool placed = false;
};

// Every dungeon occupies one square slot of the same stride, so a walk along the link graph
// lands on a consistent lattice no matter which path reaches a given dungeon. An undersized
// dungeon (the camp) is centred inside its slot at draw time rather than displacing its
// neighbours, which is what used to make the ring around the camp overlap itself.
int32_t slotStride(const std::vector<Dungeon> &dungeons) {
    int32_t stride = 1;
    for (size_t n = 0; n < dungeons.size(); ++n) {
        stride = std::max(stride, std::max(dungeons[n].width, dungeons[n].height));
    }
    return stride;
}

void neighbourOrigin(int32_t stride, Edge edge, const Placement &here, Placement *there) {
    there->x = here.x;
    there->y = here.y;
    switch (edge) {
        case EAST: there->x = here.x + stride; break;
        case WEST: there->x = here.x - stride; break;
        case SOUTH: there->y = here.y + stride; break;
        case NORTH: there->y = here.y - stride; break;
    }
    there->placed = true;
}

}

bool writeMap(const Dungeon &dungeon, const std::string &path, int32_t cell) {
    int32_t width = std::max(dungeon.width * cell + 2 * kMargin, 330);
    int32_t height = kHeader + dungeon.height * cell + kLegend + 2 * kMargin;
    render::Surface surface(width, height);
    render::SoftGraphics graphics(&surface);
    graphics.setColor((int32_t)kBackground);
    graphics.fillRect(0, 0, width, height);

    graphics.setFont(Font::getFont(0, 1, 8));
    graphics.setColor(0xFFFFFF);
    std::string title = std::to_string((int)dungeon.id) + ". " + dungeon.name + "  " +
                        std::to_string((int)dungeon.width) + "x" +
                        std::to_string((int)dungeon.height);
    graphics.drawString(std::string(title), kMargin, kMargin, render::LEFT | render::TOP);

    drawCells(&graphics, dungeon, kMargin, kHeader + kMargin, cell);
    drawLegend(&graphics, kMargin, kHeader + kMargin + dungeon.height * cell + 3,
               hasNpc(dungeon));
    return render::writePng(surface, path);
}

bool writeAtlas(const std::vector<Dungeon> &dungeons, const std::string &path, int32_t cell,
                std::vector<std::string> *conflicts) {
    if (dungeons.empty()) return false;
    static const char *kEdgeName[4] = {"north", "east", "south", "west"};

    std::map<int32_t, const Dungeon *> byId;
    for (size_t n = 0; n < dungeons.size(); ++n) {
        byId[dungeons[n].id] = &dungeons[n];
    }
    std::map<int32_t, Placement> placed;
    int32_t stride = slotStride(dungeons);

    std::deque<int32_t> queue;
    int32_t seed = byId.count(1) != 0 ? 1 : dungeons[0].id;
    placed[seed] = Placement{0, 0, true};
    queue.push_back(seed);
    while (!queue.empty()) {
        int32_t id = queue.front();
        queue.pop_front();
        const Dungeon &from = *byId[id];
        Placement here = placed[id];
        for (int32_t edge = 0; edge < 4; ++edge) {
            int32_t target = from.links[edge];
            if (target <= 0 || byId.count(target) == 0) continue;
            Placement there;
            neighbourOrigin(stride, (Edge)edge, here, &there);
            std::map<int32_t, Placement>::iterator existing = placed.find(target);
            if (existing == placed.end()) {
                placed[target] = there;
                queue.push_back(target);
            } else if (existing->second.x != there.x || existing->second.y != there.y) {
                if (conflicts != nullptr) {
                    conflicts->push_back(
                        std::to_string((int)id) + " " + kEdgeName[edge] + " -> " +
                        std::to_string((int)target) + ": wants (" + std::to_string((int)there.x) +
                        ", " + std::to_string((int)there.y) + "), placed at (" +
                        std::to_string((int)existing->second.x) + ", " +
                        std::to_string((int)existing->second.y) + ")");
                }
            }
        }
    }

    int32_t minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool first = true;
    for (size_t n = 0; n < dungeons.size(); ++n) {
        std::map<int32_t, Placement>::iterator it = placed.find(dungeons[n].id);
        if (it == placed.end()) continue;
        int32_t x0 = it->second.x;
        int32_t y0 = it->second.y;
        int32_t x1 = x0 + stride;
        int32_t y1 = y0 + stride;
        if (first) {
            minX = x0; minY = y0; maxX = x1; maxY = y1;
            first = false;
        } else {
            minX = std::min(minX, x0);
            minY = std::min(minY, y0);
            maxX = std::max(maxX, x1);
            maxY = std::max(maxY, y1);
        }
    }
    int32_t strayX = minX;
    int32_t strayY = maxY + 2;
    for (size_t n = 0; n < dungeons.size(); ++n) {
        if (placed.count(dungeons[n].id) != 0) continue;
        placed[dungeons[n].id] = Placement{strayX, strayY, true};
        strayX += stride + 2;
        maxX = std::max(maxX, strayX);
        maxY = std::max(maxY, strayY + stride);
    }

    int32_t width = (maxX - minX) * cell + 2 * kMargin;
    int32_t height = kHeader + (maxY - minY) * cell + kLegend + 2 * kMargin;
    render::Surface surface(width, height);
    render::SoftGraphics graphics(&surface);
    graphics.setColor((int32_t)kBackground);
    graphics.fillRect(0, 0, width, height);

    // Terrain first, then every outline and label, so that a border or an id digit can never
    // land on top of a neighbour's floor even if two dungeons do end up sharing ground.
    bool anyNpc = false;
    std::vector<Placement> origins(dungeons.size());
    for (size_t n = 0; n < dungeons.size(); ++n) {
        const Dungeon &dungeon = dungeons[n];
        Placement spot = placed[dungeon.id];
        origins[n].x = kMargin + (spot.x - minX) * cell + (stride - dungeon.width) / 2 * cell;
        origins[n].y = kHeader + kMargin + (spot.y - minY) * cell +
                       (stride - dungeon.height) / 2 * cell;
        drawCells(&graphics, dungeon, origins[n].x, origins[n].y, cell);
        anyNpc = anyNpc || hasNpc(dungeon);
    }
    graphics.setFont(Font::getFont(0, 1, 8));
    for (size_t n = 0; n < dungeons.size(); ++n) {
        const Dungeon &dungeon = dungeons[n];
        graphics.setColor((int32_t)kOutline);
        graphics.drawRect(origins[n].x, origins[n].y, dungeon.width * cell - 1,
                          dungeon.height * cell - 1);
        graphics.setColor(0x000000);
        graphics.drawString(std::string(std::to_string((int)dungeon.id)), origins[n].x + 3,
                            origins[n].y + 2, render::LEFT | render::TOP);
    }

    graphics.setFont(Font::getFont(0, 1, 8));
    graphics.setColor(0xFFFFFF);
    std::string title = std::to_string((int)dungeons.size()) +
                        " dungeons, joined along the edges their geometry table links";
    graphics.drawString(std::string(title), kMargin, kMargin, render::LEFT | render::TOP);
    drawLegend(&graphics, kMargin, height - kLegend, anyNpc);
    return render::writePng(surface, path);
}

}
