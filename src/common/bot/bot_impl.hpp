#ifndef COMMON_BOT_BOT_IMPL_HPP
#define COMMON_BOT_BOT_IMPL_HPP

namespace bot {

template <typename DungeonT>
void Bot<DungeonT>::tick() {
    *clock_ += tickMs_;
    if (game_->gameCanvas_ != nullptr) {
        game_->gameCanvas_->repaintEnabled_ = false;
        game_->gameCanvas_->tick();
        showFrame();
    }
    clearScreens();

    capMonsters();
}

template <typename DungeonT>
void Bot<DungeonT>::capMonsters() {
    Player *p = player();
    if (p == nullptr) return;
    const std::size_t dungeonIndex = (std::size_t)(p->dungeonId_ - 1);
    if (!game_->worldState().monsters.hasTable(dungeonIndex)) return;
    if ((int32_t)game_->worldState().monsters.at(dungeonIndex).size() <= kMonsterCeiling) {
        return;
    }

    const worldstate::MonsterList records = game_->worldState().monsters.at(dungeonIndex);
    std::vector<std::pair<int32_t, std::pair<int32_t, int32_t>>> byDistance;
    byDistance.reserve(records.size());
    for (const worldstate::MonsterRecord &record : records) {
        const int32_t x = record[4];
        const int32_t y = record[5];
        byDistance.push_back({wrappingAbs(x - p->gridX_) + wrappingAbs(y - p->gridY_), {x, y}});
    }
    std::sort(byDistance.begin(), byDistance.end(),
              [](const auto &a, const auto &b) { return a.first > b.first; });

    int32_t over = (int32_t)records.size() - kMonsterCeiling;
    for (const auto &entry : byDistance) {
        if (over <= 0) break;
        game_->removeMonsterAt(p->dungeonId_, entry.second.first, entry.second.second);
        --over;
    }
}

template <typename DungeonT>
void Bot<DungeonT>::showFrame() {
    if (frameHook_ != nullptr) frameHook_(*game_);
}

template <typename DungeonT>
int32_t Bot<DungeonT>::clearScreens(int32_t guard) {
    int32_t cleared = 0;
    for (int32_t n = 0; n < guard; ++n) {
        UIWidget *screen = game_->currentUI_;
        if (screen == nullptr) break;
        const int32_t id = screen->screenId_;

        if (id == uistate::SCREEN_LEVEL_UP) {
            if (screen->rowCount() <= 0) break;
            screen->setSelectedIndex(0);
            game_->commandAction(UIWidget::cmdSelect_, nullptr);
            if (game_->currentUI_ == nullptr ||
                game_->currentUI_->screenId_ != uistate::SCREEN_LEVEL_UP) {
                ++levelUpsTaken_;
            }
            ++cleared;
            showFrame();
            continue;
        }

        game_->commandAction(UIWidget::cmdOk_, nullptr);
        ++cleared;
        showFrame();
        if (game_->currentUI_ == screen) break;
    }
    if (cleared > 0 && game_->gameCanvas_ != nullptr) game_->gameCanvas_->resume();
    return cleared;
}

template <typename DungeonT>
void Bot<DungeonT>::heal() {
    Player *p = player();
    if (p == nullptr) return;
    p->vitals_[2] = p->vitals_[3];
    p->vitals_[6] = p->vitals_[7];

    p->ailments_ = 0;
}

template <typename DungeonT>
bool Bot<DungeonT>::walkable(int32_t x, int32_t y) const {
    DungeonT *d = this->dungeon();
    if (x < 0 || y < 0 || x >= d->width_ || y >= d->height_) return false;
    const int8_t tile = d->tileAt(x, y);

    if ((tile & smallhelpers::WALL) != 0) return false;

    if (avoidWarps_ && (tile & 8) != 0) return false;
    return true;
}

template <typename DungeonT>
bool Bot<DungeonT>::stepTowards(int32_t facing) {
    Player *p = player();
    heal();
    p->facing_ = (int8_t)facing;
    p->refreshSurroundings();

    if (stayInDungeon_ != 0) {
        DungeonT *d = this->dungeon();
        int32_t nextX = p->gridX_;
        int32_t nextY = p->gridY_;
        for (const Step &s : kSteps) {
            if (s.facing != facing) continue;
            nextX += s.dx;
            nextY += s.dy;
            break;
        }
        if (nextX < 0 || nextY < 0 || nextX >= d->width_ || nextY >= d->height_) {
            return false;
        }
    }

    if (avoidWarps_) {
        DungeonT *d = this->dungeon();
        int32_t wx = p->gridX_, wy = p->gridY_;
        for (const Step &s : kSteps) {
            if (s.facing != facing) continue;
            wx += s.dx;
            wy += s.dy;
            break;
        }
        if (wx >= 0 && wy >= 0 && wx < d->width_ && wy < d->height_ &&
            (d->tileAt(wx, wy) & 8) != 0) {
            return false;
        }
    }

    fightAhead();
    heal();

    makeRoom();

    const bool moved = p->move(1, false);
    tick();
    return moved;
}

template <typename DungeonT>
bool Bot<DungeonT>::walkTo(int32_t x, int32_t y) {
    Player *p = player();
    if (p->gridX_ == x && p->gridY_ == y) return true;

    for (int guard = 0; guard < 400; ++guard) {
        if (p->gridX_ == x && p->gridY_ == y) return true;

        const int32_t startX = p->gridX_;
        const int32_t startY = p->gridY_;
        std::map<std::pair<int32_t, int32_t>, std::pair<int32_t, int32_t>> from;
        std::deque<std::pair<int32_t, int32_t>> queue;
        queue.emplace_back(startX, startY);
        from[{startX, startY}] = {startX, startY};
        bool found = false;
        while (!queue.empty() && !found) {
            const std::pair<int32_t, int32_t> at = queue.front();
            queue.pop_front();
            for (const Step &s : kSteps) {
                const std::pair<int32_t, int32_t> next{at.first + s.dx, at.second + s.dy};
                if (from.count(next) != 0) continue;
                if (!walkable(next.first, next.second)) continue;
                from[next] = at;
                if (next.first == x && next.second == y) {
                    found = true;
                    break;
                }
                queue.push_back(next);
            }
        }
        if (!found) return false;

        const std::pair<int32_t, int32_t> start{startX, startY};
        std::pair<int32_t, int32_t> at{x, y};
        for (int hops = 0; hops <= (int)from.size(); ++hops) {
            const auto prev = from.find(at);
            if (prev == from.end()) return false;
            if (prev->second == start || prev->second == at) break;
            at = prev->second;
        }
        if (at == start) return false;

        int32_t facing = 0;
        for (const Step &s : kSteps) {
            if (startX + s.dx == at.first && startY + s.dy == at.second) {
                facing = s.facing;
                break;
            }
        }
        if (facing == 0) return false;
        if (!stepTowards(facing)) {
            return false;
        }
    }
    return p->gridX_ == x && p->gridY_ == y;
}

template <typename DungeonT>
bool Bot<DungeonT>::faceSummoner() {
    Player *p = player();
    const std::size_t dungeonIndex = (std::size_t)(p->dungeonId_ - 1);
    if (!game_->worldState().monsters.hasTable(dungeonIndex)) return false;

    const worldstate::MonsterList records = game_->worldState().monsters.at(dungeonIndex);
    Monster decoded;
    for (const worldstate::MonsterRecord &record : records) {
        Monster *m = Monster::fromRecord(&decoded, record, p->dungeon());
        if (m == nullptr || m->hp_ <= 0) continue;
        if (m->stat(kSummonerStatColumn) != kSummonerEffect) continue;
        for (const Step &s : kSteps) {
            if (p->gridX_ + s.dx != m->gridX_ || p->gridY_ + s.dy != m->gridY_) continue;
            p->facing_ = (int8_t)s.facing;
            p->refreshSurroundings();
            return true;
        }
    }
    return false;
}

template <typename DungeonT>
int32_t Bot<DungeonT>::monstersHere() const {
    const std::size_t dungeonIndex = (std::size_t)(player()->dungeonId_ - 1);
    if (!game_->worldState().monsters.hasTable(dungeonIndex)) return 0;
    return (int32_t)game_->worldState().monsters.at(dungeonIndex).size();
}

template <typename DungeonT>
bool Bot<DungeonT>::fightAhead(int32_t maxSwings) {
    Player *p = player();
    Monster storage;
    for (int32_t swing = 0; swing < maxSwings; ++swing) {
        heal();

        faceSummoner();

        Monster *target = p->monsterAhead(&storage);
        if (target == nullptr) return swing > 0;
        if (target->hp_ <= 0) return true;
        p->attackMonster(target);
        target->store();
        tick();
        if (target->hp_ <= 0) {
            const int32_t deadX = target->gridX_;
            const int32_t deadY = target->gridY_;
            game_->gameCanvas_->combatMonster_ = target;
            tick();
            game_->removeMonsterAt(p->dungeonId_, deadX, deadY);
            p->refreshSurroundings();
            return true;
        }
    }
    return false;
}

template <typename DungeonT>
bool Bot<DungeonT>::takeChestAhead() {
    Player *p = player();
    SharedArray<int8_t> chest = p->chestAhead();
    if (chest.isNull() || chest.length() == 0) return false;
    makeRoom();
    p->takeChest(chest);
    tick();
    return true;
}

template <typename DungeonT>
int32_t Bot<DungeonT>::equipBest() {
    Player *p = player();
    int32_t swapped = 0;
    for (int32_t slot = 0; slot < p->itemCount_; ++slot) {
        const int32_t id = wrappingAbs(p->inventory_[slot]);
        if (id <= 0) continue;
        if (!Items::isWeaponOrArmour(id)) continue;
        if (p->isEquipped(slot)) continue;
        const int32_t category = Items::stat(Items::CATEGORY, id);
        const int32_t rating = Items::stat(Items::RATING, id);
        const int32_t worn = p->findEquipped(category);
        const int32_t wornRating =
            worn == -1 ? -1 : Items::stat(Items::RATING, wrappingAbs(p->inventory_[worn]));
        if (rating <= wornRating) continue;
        if (p->equip(slot, true)) ++swapped;
    }
    return swapped;
}

template <typename DungeonT>
int32_t Bot<DungeonT>::makeRoom() {
    Player *p = player();
    if (p->hasRoom()) return -1;

    int32_t worst = -1;
    int32_t worstValue = 0x7FFFFFFF;
    int32_t spare = -1;
    for (int32_t slot = 0; slot < p->itemCount_; ++slot) {
        const int32_t id = wrappingAbs(p->inventory_[slot]);
        if (id <= 0) continue;
        if (p->isEquipped(slot)) continue;
        if (Items::stat(Items::CATEGORY, id) == 11) {
            if (spare < 0) spare = slot;
            continue;
        }
        const int32_t value = Items::stat(Items::SELL_PRICE, id);
        if (value < worstValue) {
            worstValue = value;
            worst = slot;
        }
    }
    if (worst < 0) worst = spare;
    if (worst < 0) return -1;
    p->removeItem(worst);
    return worst;
}

template <typename DungeonT>
bool Bot<DungeonT>::crossEdge(int32_t edge, int32_t facing, int32_t fromDungeon) {
    Player *p = player();
    DungeonT *d = this->dungeon();
    stayInDungeon_ = 0;

    std::vector<std::pair<int32_t, int32_t>> approaches;
    if (d->id_ != 1 && (d->exitDir1_ == facing || d->exitDir2_ == facing)) {
        switch (facing) {
            case 1: approaches.push_back({17, 0}); break;
            case 3: approaches.push_back({17, d->height_ - 1}); break;
            case 4: approaches.push_back({0, 17}); break;
            default: approaches.push_back({d->width_ - 1, 17}); break;
        }
    }

    std::vector<std::pair<int32_t, int32_t>> rim;
    if (edge == 0 || edge == 2) {
        const int32_t y = edge == 0 ? 0 : d->height_ - 1;
        for (int32_t x = 0; x < d->width_; ++x) {
            if (walkable(x, y)) rim.emplace_back(x, y);
        }
    } else {
        const int32_t x = edge == 3 ? 0 : d->width_ - 1;
        for (int32_t y = 0; y < d->height_; ++y) {
            if (walkable(x, y)) rim.emplace_back(x, y);
        }
    }
    std::sort(rim.begin(), rim.end(), [&](const auto &a, const auto &b) {
        const int32_t da = wrappingAbs(a.first - p->gridX_) + wrappingAbs(a.second - p->gridY_);
        const int32_t db = wrappingAbs(b.first - p->gridX_) + wrappingAbs(b.second - p->gridY_);
        return da < db;
    });
    approaches.insert(approaches.end(), rim.begin(), rim.end());

    int32_t reached = 0;
    for (const auto &at : approaches) {
        if (p->dungeonId_ != fromDungeon) return true;
        if (!walkTo(at.first, at.second)) continue;

        ++reached;
        for (int guard = 0; guard < 40 && p->dungeonId_ == fromDungeon; ++guard) {
            if (!stepTowards(facing)) break;

        }
        if (p->dungeonId_ != fromDungeon) return true;
    }
    return false;
}

template <typename DungeonT>
SweepResult Bot<DungeonT>::sweepCurrentDungeon() {
    Player *p = player();
    DungeonT *d = this->dungeon();
    SweepResult out;
    out.dungeonId = p->dungeonId_;
    out.entered = true;
    stayInDungeon_ = out.dungeonId;

    std::set<std::pair<int32_t, int32_t>> targets;
    for (int32_t y = 0; y < d->height_; ++y) {
        for (int32_t x = 0; x < d->width_; ++x) {
            if (!walkable(x, y)) continue;
            if ((d->tileAt(x, y) & smallhelpers::CHAMPION) != 0) continue;
            targets.emplace(x, y);
        }
    }

    for (const std::pair<int32_t, int32_t> &target : targets) {
        if (p->dungeonId_ != out.dungeonId) break;
        if (!walkTo(target.first, target.second)) continue;
        ++out.tilesWalked;

        for (const Step &s : kSteps) {
            if (p->dungeonId_ != out.dungeonId) break;
            heal();
            p->facing_ = (int8_t)s.facing;
            p->refreshSurroundings();
            Monster storage;
            if (p->monsterAhead(&storage) != nullptr) {
                ++out.swings;
                if (fightAhead()) ++out.monstersKilled;
            }
            if (takeChestAhead()) ++out.chestsOpened;
        }
        equipBest();
    }

    stayInDungeon_ = 0;
    return out;
}

template <typename DungeonT>
std::vector<SweepResult> Bot<DungeonT>::sweepAllDungeons() {
    std::vector<SweepResult> out;
    std::set<int32_t> done;
    std::set<std::pair<int32_t, int32_t>> deadEnds;
    int32_t lastOpenCount = populatedDungeons();

    for (int32_t guard = 0;
         guard < 8 * (int32_t)worldstate::DungeonRegistry::kDungeonCount &&
         done.size() < worldstate::DungeonRegistry::kDungeonCount;
         ++guard) {
        Player *p = player();
        const int32_t here = p->dungeonId_;
        if (done.count(here) == 0) {
            out.push_back(sweepCurrentDungeon());
            done.insert(here);
        }

        DungeonT *d = this->dungeon();
        std::vector<int32_t> route = routeToUnswept(here, done, deadEnds);
        if (route.empty()) {
            const bool grew = populatedDungeons() > lastOpenCount;
            lastOpenCount = populatedDungeons();
            if (!deadEnds.empty() || grew) {
                deadEnds.clear();
                route = routeToUnswept(here, done, deadEnds);
            }
            if (route.empty()) break;
        }

        for (const int32_t next : route) {
            const int32_t at = player()->dungeonId_;
            DungeonT *from = this->dungeon();
            int32_t edge = -1;
            for (int32_t e = 0; e < 4; ++e) {
                if (from->geometry_[e] == next) {
                    edge = e;
                    break;
                }
            }
            if (edge < 0) {
                deadEnds.insert({at, next});
                break;
            }
            const int32_t facing = edge == 0 ? 1 : edge == 1 ? 2 : edge == 2 ? 3 : 4;
            if (!crossEdge(edge, facing, at)) {
                deadEnds.insert({at, next});
                break;
            }
            if (player()->dungeonId_ != next) {
                break;
            }
        }
    }
    return out;
}

template <typename DungeonT>
int32_t Bot<DungeonT>::populatedDungeons() const {
    int32_t open = 0;
    for (int32_t id = 1; id <= (int32_t)worldstate::DungeonRegistry::kDungeonCount; ++id) {
        if (game_->dungeonAt(id)->populated_) ++open;
    }
    return open;
}

template <typename DungeonT>
std::vector<int32_t> Bot<DungeonT>::routeToUnswept(int32_t from, const std::set<int32_t> &done,
                                         const std::set<std::pair<int32_t, int32_t>> &deadEnds) {
    std::map<int32_t, int32_t> cameFrom;
    std::deque<int32_t> queue{from};
    cameFrom[from] = 0;
    int32_t goal = 0;
    while (!queue.empty() && goal == 0) {
        const int32_t at = queue.front();
        queue.pop_front();
        DungeonCore *d = game_->dungeonAt(at);
        for (int32_t e = 0; e < 4; ++e) {
            const int32_t next = d->geometry_[e];
            if (next <= 0 || cameFrom.count(next) != 0) continue;
            if (deadEnds.count({at, next}) != 0) continue;
            if (!game_->dungeonAt(next)->populated_) continue;

            DungeonCore *from = game_->dungeonAt(at);
            if (at != 1) {
                const int32_t facing = e == 0 ? 1 : e == 1 ? 2 : e == 2 ? 3 : 4;
                if (from->exitDir1_ != facing && from->exitDir2_ != facing) continue;
            }
            cameFrom[next] = at;
            if (done.count(next) == 0) {
                goal = next;
                break;
            }
            queue.push_back(next);
        }
    }
    if (goal == 0) return {};

    std::vector<int32_t> route;
    for (int32_t at = goal; at != from; at = cameFrom[at]) route.push_back(at);
    std::reverse(route.begin(), route.end());
    return route;
}

}

#endif
