#include "src/stormhold/variant.hpp"

#include <algorithm>

#include "src/common/game/game_canvas.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/registered_application.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/game/util.hpp"
#include "src/stormhold/cus_image.hpp"
#include "src/stormhold/dungeon.hpp"
#include "src/stormhold/npc_script.hpp"
#include "src/stormhold/profile.hpp"

namespace stormhold {

namespace {

const std::vector<std::vector<int32_t>> kDungeonGroups = {
    {2, 3, 4, 11, 12, 13, 20, 21, 22, 29, 30, 31},
    {5, 6, 7},
    {23, 24, 25},
    {14, 15, 16},
    {8, 9, 10},
    {32, 33, 34},
    {26, 27, 28},
    {17, 18, 19},
    {35, 36, 37}};

}

Variant::Variant(Game &game) : game_(game) {
    this->dungeonGen_.buildCampGrid();
}

game::SplashArt Variant::loadSplashArt() {
    game::SplashArt art;
    art.carrierLogo = Image::createImage(game_.platformContext_, std::string("/mformaLogo.png"));
    art.publisherLogo = Image::createImage(game_.platformContext_, std::string("/vir2lLogo.png"));
    art.top = this->createImage(std::string("/splashtop.png"));
    art.bottom = this->createImage(std::string("/splashbot.png"));
    return art;
}

void Variant::loadTables() {
    this->loadGeometry();
    Dungeon::loadNames(game_.platformContext_);
    NpcSystem::loadDialogue(game_.platformContext_);
    game_.splashUI_->progressPercent_ = 5;
    this->loadHelpStrings();
    this->loadHelpTitles();
}

void Variant::loadArt() {
    GameCanvas::floorImage_ = this->createImage(std::string("floor3.png"));
    GameCanvas::wallRightImage_ = this->createImage(std::string("newwallsnok.png"));
    game_.checkDestroyed();
    GameCanvas::bagSprites_ = SharedArray<render::Sprite *>(3);
    GameCanvas::bagSprites_[0] = CusImage::load(game_.platformContext_, std::string("baglarge.cus"));
    GameCanvas::bagSprites_[1] = CusImage::load(game_.platformContext_, std::string("bagmid.cus"));
    GameCanvas::bagSprites_[2] = CusImage::load(game_.platformContext_, std::string("bagsmall.cus"));
    GameCanvas::crystalSprites_ = SharedArray<render::Sprite *>(3);
    GameCanvas::crystalSprites_[0] = CusImage::load(game_.platformContext_, std::string("crystalnear.cus"));
    GameCanvas::crystalSprites_[1] = CusImage::load(game_.platformContext_, std::string("crystalmid.cus"));
    GameCanvas::crystalSprites_[2] = CusImage::load(game_.platformContext_, std::string("crystalfar.cus"));
    game_.checkDestroyed();
    GameCanvas::effectImages_ = SharedArray<Image *>(3);
    int32_t n2 = 0;
    while (n2 < 3) {
        GameCanvas::effectImages_[n2] = nullptr;
        ++n2;
    }
    GameCanvas::effectImages_[0] = this->createImage(std::string("blood1.png"));
    GameCanvas::effectImages_[1] = this->createImage(std::string("monsterspell.png"));
    GameCanvas::effectImages_[2] = this->createImage(std::string("selfspell.png"));
    GameCanvas::chestSprites_ = SharedArray<render::Sprite *>(3);
    GameCanvas::chestSprites_[0] = CusImage::load(game_.platformContext_, std::string("chestnearclosed.cus"));
    GameCanvas::chestSprites_[1] = CusImage::load(game_.platformContext_, std::string("chestmidclosed.cus"));
    GameCanvas::chestSprites_[2] = CusImage::load(game_.platformContext_, std::string("chestfarclosed.cus"));
    game_.checkDestroyed();
    GameCanvas::hudIcons_ = SharedArray<Image *>(6);
    GameCanvas::hudIcons_[0] = this->createImage(std::string("icon_attack.png"));
    GameCanvas::hudIcons_[1] = this->createImage(std::string("icon_cast.png"));
    GameCanvas::hudIcons_[2] = this->createImage(std::string("icon_change.png"));
    GameCanvas::hudIcons_[3] = this->createImage(std::string("icon_option.png"));
    GameCanvas::hudIcons_[4] = this->createImage(std::string("icon_action.png"));
    GameCanvas::hudIcons_[5] = this->createImage(std::string("icon_camp.png"));
    game_.checkDestroyed();
}

render::Sprite *Variant::loadMonsterSprite(const std::string &name) {
    return CusImage::load(game_.platformContext_, name);
}

void Variant::allocateScreens() {
    npcHelloUI_ = game_.makeOwnedUIWidget(4, uistate::SCREEN_NPC_GREETING);
    npcHelloUI_->setupTextBox(std::string("NPC name here"), std::string("NPC text here"));
    rumorsUI_ = game_.makeOwnedUIWidget(4, uistate::SCREEN_NPC_GREETING_ALTERNATE);
    rumorsUI_->setupTextBox(std::string("Rumors"), std::string("Rumors text here"));
    game_.NPCChoicesUI_ = SharedArray<UIWidget *>(6);
    SharedArray<std::string> stringArray1;
    int32_t n3 = 0;
    while (n3 < 4) {
        game_.NPCChoicesUI_[n3] = game_.makeOwnedUIWidget(5, 9 + n3);
        stringArray1 = SharedArray<std::string>{std::string("Train"), std::string("Give"), std::string("Befriend"),
                                     std::string("Threaten"), std::string("Kill")};
        game_.NPCChoicesUI_[n3]->setupForm(std::string("Name"), std::string("Aid: <TAG>"), stringArray1);
        game_.NPCChoicesUI_[n3]->backTarget_ = game_.gameCanvas_;
        ++n3;
    }
    game_.NPCChoicesUI_[4] = game_.makeOwnedUIWidget(5, screens::NPC_CHOICES_4);
    stringArray1 = SharedArray<std::string>{std::string("Give Item"), std::string("Take Crystal")};
    game_.NPCChoicesUI_[4]->setupForm(std::string("Beneca"), std::string("Aid: <TAG>"), stringArray1);
    game_.NPCChoicesUI_[4]->backTarget_ = game_.gameCanvas_;
    game_.NPCChoicesUI_[5] = game_.makeOwnedUIWidget(5, screens::NPC_CHOICES_5);
    SharedArray<std::string> stringArray5{std::string("Rumors"), std::string("Give Crystal"), std::string("Enchant"),
                                std::string("Bless"),  std::string("Cure"),         std::string("Warp"),
                                std::string("Recovery")};
    game_.NPCChoicesUI_[5]->setupForm(std::string("Helga"), std::string("Aid: <TAG>"), stringArray5);
    game_.NPCChoicesUI_[5]->backTarget_ = game_.gameCanvas_;
    game_.GenericInfoUI_ = game_.makeOwnedUIWidget(4, uistate::SCREEN_NPC_GIVE_RESPONSE);
    game_.GenericInfoUI_->setupTextBox(std::string("Oracle"), std::string("NPC text here"));
    game_.noSavedGameUI_->a(UIWidget::cmdOk_);
    game_.noSavedGameUI_->a((CommandListener *)&game_);
}

void Variant::buildDungeons() {
    game_.dungeons_.clear();
    game_.splashUI_->progressPercent_ = 62;
    game_.dungeons_.emplace<Dungeon>(0, game_.dungeons_, game_.worldState_, 1, dungeonGeometry_[0],
                                     dungeonGen_.campWidth_, dungeonGen_.campHeight_,
                                     dungeonGen_.campGrid_);
    int32_t n1 = 1;
    while (n1 < 37) {
        Dungeon *dungeon = game_.dungeons_.emplace<Dungeon>(
            (std::size_t)n1, game_.dungeons_, game_.worldState_, (int8_t)(n1 + 1),
            dungeonGeometry_[n1]);
        dungeonGen_.generate(dungeon);
        ++game_.splashUI_->progressPercent_;
        ++n1;
    }
}

void Variant::createNewGame() {
    platform::writeLogLine("Game: creating a new game");
    game_.createGameUI_->progressPercent_ = 0;
    int32_t n1 = this->gameAdvancementLevel(0);
    int32_t n2 = 0;
    while (n2 <= n1) {
        this->openAndRepopulateDungeons(n2);
        ++n2;
    }
    game_.setCurrentDisplay(game_.newGameUI_);
}

void Variant::resumeGame() {
    int32_t n1 = this->gameAdvancementLevel((int32_t)game_.character_->giftPoints_);
    this->openDungeonGroup(n1);
}

const char *Variant::screenName(int32_t screenId) const {
    switch (screenId) {
        case screens::NPC_CHOICES_0:
        case screens::NPC_CHOICES_1:
        case screens::NPC_CHOICES_2:
        case screens::NPC_CHOICES_3:
        case screens::NPC_CHOICES_4:
        case screens::NPC_CHOICES_5: return "npc choices";
        case screens::NPC_KILL_RESPONSE: return "npc kill response";
        case screens::NPC_ENCHANT_WHAT: return "npc enchant what";
        case screens::NPC_ENCHANT_RESPONSE: return "npc enchant response";
        case screens::RETURN_TO_GAME: return "return to game";
        case screens::RETURN_TO_GAME_PAUSED: return "return to game (paused)";
        case screens::BACK_MESSAGE: return "back message";
        case screens::NPC_TAKE_WHAT: return "npc take what";
        case screens::NPC_TAKE_RESPONSE: return "npc take response";
        case screens::NPC_BLESS_RESPONSE: return "npc bless response";
        default: return nullptr;
    }
}

void Variant::openAndRepopulateDungeons(int32_t group) {
    // Called on every gift-point change, so it is usually a no-op.  Only an
    // actual opening is worth a line; logging each call would repeat the same
    // message through a whole playthrough.
    int32_t opened = 0;
    const std::vector<int32_t> &ids = kDungeonGroups[(std::size_t)group];
    for (std::size_t i = 0; i < ids.size(); ++i) {
        Dungeon *dungeon = game_.dungeons_.as<Dungeon>((std::size_t)(ids[i] - 1));
        if (!dungeon->populated_) {
            dungeon->populated_ = true;
            dungeon->populate();
            ++opened;
        }
        if (game_.createGameUI_ != nullptr) {
            game_.createGameUI_->progressPercent_ =
                std::min<int>(100, (int)(100 * (i + 1) / ids.size()));
            game_.createGameUI_->requestRepaint();
            game_.createGameUI_->flushRepaints();
        }
    }
    if (opened > 0) {
        platform::writeLogLine("World: opened " + std::to_string(opened) +
                               " dungeon(s) at advancement level " + std::to_string(group));
    }
}

void Variant::openDungeonGroup(int32_t group) {
    std::size_t total = 0;
    for (int32_t g = 0; g <= group; ++g) total += kDungeonGroups[(std::size_t)g].size();

    std::size_t done = 0;
    for (int32_t g = 0; g <= group; ++g) {
        for (int32_t id : kDungeonGroups[(std::size_t)g]) {
            Dungeon *dungeon = game_.dungeons_.as<Dungeon>((std::size_t)(id - 1));
            dungeon->populated_ = true;
            dungeon->rebuildOccupancy();
            game_.loadGameUI_->progressPercent_ = std::min<int>(100, (int)(100 * ++done / total));
        }
    }
}

int32_t Variant::gameAdvancementLevel(int32_t n) {
    if (n < 9) {
        return 0;
    }
    if (n < 13) {
        return 1;
    }
    if (n < 17) {
        return 2;
    }
    if (n < 23) {
        return 3;
    }
    if (n < 28) {
        return 4;
    }
    if (n < 34) {
        return 5;
    }
    if (n < 40) {
        return 6;
    }
    if (n < 48) {
        return 7;
    }
    return 8;
}

UIWidget *Variant::infoWidget(int32_t screenId) {
    return game_.makeOwnedUIWidget(4, screenId);
}

UIWidget *Variant::infoBox(int32_t screenId, const std::string &title, const std::string &body,
                           game::DisplayTarget back, game::DisplayTarget next) {
    UIWidget *h2 = game_.makeOwnedUIWidget(4, screenId);
    h2->setupTextBox(title, body);
    h2->backTarget_ = back;
    h2->nextTarget_ = next;
    return h2;
}

void Variant::refreshNpcAid(int32_t n) {
    std::string string1 = game_.NPCChoicesUI_[n]->tagTemplate_;
    std::string string2 = game_.NPCChoicesUI_[n]->bodyText();
    (void)string2;
    int16_t s1 = 0;
    if (NpcSystem::isChampion(n)) {
        s1 = game_.worldState_.npcs.aidPoints[n];
    } else if (n == 4) {
        s1 = game_.worldState_.npcs.scrapCount;
    } else if (n == 5) {
        s1 = game_.worldState_.npcs.gemCount;
    }
    std::string string3 = GameUtil::replace(string1, std::string("<TAG>"), (int32_t)s1);
    game_.NPCChoicesUI_[n]->setBodyText(string3);
}

std::string Variant::introductionText(int32_t page) {
    (void)page;
    return NpcSystem::dialogue_[7][3];
}

void Variant::showEndOfGame() {
    game_.endOfGameUI_ = this->newEndOfGameUI();
    game_.setCurrentDisplay(game_.endOfGameUI_);
}

void Variant::showGameOver() {
    game_.endOfGameUI_ = this->newGameOverUI();
    game_.setCurrentDisplay(game_.endOfGameUI_);
}

void Variant::openNpcScreen(GameCanvas &canvas, int32_t n) {
    // Breadcrumbs for crash reports: this records who the player talked to and
    // what art was resident, so a log recovered after a crash says which NPC
    // and which sprite slot was involved instead of just an address.
    {
        int32_t residentSprites = 0;
        for (int32_t i = 0; i < GameCanvas::npcSprites_.length(); ++i) {
            if (GameCanvas::npcSprites_[i] != nullptr) ++residentSprites;
        }
        // The npc and dungeon are already on the "started talking" line; what
        // this adds is sprite residency, which is what a missing-portrait
        // report needs.
        platform::writeLogLine(
            "NPC: opening npc screen with " + std::to_string(residentSprites) + "/" +
            std::to_string(GameCanvas::npcSprites_.length()) + " sprites resident, " +
            std::to_string(game_.NPCChoicesUI_.length()) + " choice screens");
    }
    Player &player = *canvas.player_;
    worldstate::NpcState &npcs = player.world_->worldState().npcs;
    std::optional<std::string> string1 = NpcSystem::interact(&player, n, 1, 0);
    // Varus has no choices menu -- there are only six of those, one per
    // townsfolk NPC, and he is NPC 6.  He just says his piece and hands the
    // player back to the map, the same way the camp handoff does.
    if (n >= game_.NPCChoicesUI_.length()) {
        if (string1) {
            wardenSpeaksUI_ = this->newWardenSpeaksUI(*string1);
            game_.setCurrentDisplay(wardenSpeaksUI_);
            canvas.repaintEnabled_ = false;
        }
        return;
    }
    if (string1) {
        npcHelloUI_->setTitle(NpcSystem::npcNames_[n]);
        npcHelloUI_->setBodyText(*string1);
        npcHelloUI_->nextTarget_ = game_.NPCChoicesUI_[n];
        npcHelloUI_->contextIndex_ = n;
        UIWidget *h2 = npcHelloUI_->nextTarget_.widget();
        std::string string2 = h2->tagTemplate_;
        std::string string3 = h2->bodyText();
        int16_t s1 = 0;
        if (NpcSystem::isChampion(n)) {
            s1 = npcs.aidPoints[n];
        } else if (n == 4) {
            s1 = npcs.scrapCount;
        } else if (n == 5) {
            s1 = npcs.gemCount;
        }
        string3 = GameUtil::replace(string2, std::string("<TAG>"), (int32_t)s1);
        h2->setBodyText(string3);
        game_.setCurrentDisplay(npcHelloUI_);
        canvas.repaintEnabled_ = false;
    } else if (n == 4) {
        UIWidget *h3 = game_.NPCChoicesUI_[4];
        std::string string4 = h3->tagTemplate_;
        std::string string5 = h3->bodyText();
        int16_t s2 = 0;
        s2 = npcs.scrapCount;
        string5 = GameUtil::replace(string4, std::string("<TAG>"), (int32_t)s2);
        h3->setBodyText(string5);
        game_.setCurrentDisplay(h3);
        canvas.repaintEnabled_ = false;
    } else if (n == 5) {
        UIWidget *h4 = game_.NPCChoicesUI_[5];
        std::string string6 = h4->tagTemplate_;
        std::string string7 = h4->bodyText();
        int16_t s3 = 0;
        s3 = npcs.gemCount;
        string7 = GameUtil::replace(string6, std::string("<TAG>"), (int32_t)s3);
        h4->setBodyText(string7);
        game_.setCurrentDisplay(h4);
        canvas.repaintEnabled_ = false;
    }
}

UIWidget *Variant::newWardenSpeaksUI(const std::string &string) {
    UIWidget *h2 = game_.makeOwnedUIWidget(4, screens::RETURN_TO_GAME_PAUSED);
    h2->setupTextBox(std::string("Varus"), string);
    return h2;
}

UIWidget *Variant::newEndOfGameUI() {
    std::string string1 = NpcSystem::dialogue_[7][4];
    UIWidget *h2 = game_.makeOwnedUIWidget(4, uistate::SCREEN_END_OF_GAME);
    h2->setupTextBox(std::string("Victory!"), string1);
    h2->nextTarget_ = this->newGameOverUI();
    return h2;
}

UIWidget *Variant::newGameOverUI() {
    std::string string1 = NpcSystem::dialogue_[7][5];
    UIWidget *h2 = game_.makeOwnedUIWidget(4, uistate::SCREEN_GAME_OVER);
    h2->setupTextBox(std::string("Game Over"), string1);
    h2->nextTarget_ = game_.mainMenuUI_;
    return h2;
}

Image *Variant::createImage(const std::string &string) {
    if (string.rfind("/", 0) != 0) {
        return Image::createImage(game_.platformContext_, std::string("/") + string);
    }
    return Image::createImage(game_.platformContext_, string);
}

void Variant::loadGeometry() {
    BinaryReader *dataInputStream = GameUtil::openResource(game_.platformContext_, std::string("/geomin.dat"));
    dungeonGeometry_ = makeSharedArray2D<int8_t>(37, 6);
    int32_t n1 = 0;
    while (n1 < 37) {
        int32_t n2 = 0;
        while (n2 < 6) {
            dungeonGeometry_[n1][n2] = dataInputStream->readByte();
            ++n2;
        }
        ++n1;
    }
    delete dataInputStream;
}

void Variant::loadHelpTitles() {
    SharedArray<std::string> &helpTitles = game_.helpTitles_;
    helpTitles[0] = NpcSystem::dialogue_[7][6];
    helpTitles[1] = NpcSystem::dialogue_[7][8];
    helpTitles[2] = NpcSystem::dialogue_[7][11];
    helpTitles[3] = NpcSystem::dialogue_[7][13];
    helpTitles[4] = NpcSystem::dialogue_[7][19];
    helpTitles[5] = NpcSystem::dialogue_[7][21];
    helpTitles[6] = NpcSystem::dialogue_[7][24];
    helpTitles[7] = NpcSystem::dialogue_[7][29];
    helpTitles[8] = NpcSystem::dialogue_[7][31];
    helpTitles[9] = NpcSystem::dialogue_[7][34];
    helpTitles[10] = NpcSystem::dialogue_[7][37];
    helpTitles[11] = NpcSystem::dialogue_[7][39];
}

void Variant::loadHelpStrings() {
    SharedArray<std::string> &helpStrings = game_.helpStrings_;
    helpStrings[0] = NpcSystem::dialogue_[7][7];
    helpStrings[1] = NpcSystem::dialogue_[7][9] + NpcSystem::dialogue_[7][10];
    helpStrings[2] = NpcSystem::dialogue_[7][12];
    helpStrings[3] = NpcSystem::dialogue_[7][14] + NpcSystem::dialogue_[7][15] +
                     NpcSystem::dialogue_[7][16] + NpcSystem::dialogue_[7][17] +
                     NpcSystem::dialogue_[7][18];
    helpStrings[4] = NpcSystem::dialogue_[7][20];
    helpStrings[5] = NpcSystem::dialogue_[7][22] + NpcSystem::dialogue_[7][23];
    helpStrings[6] = NpcSystem::dialogue_[7][25] + NpcSystem::dialogue_[7][26] +
                     NpcSystem::dialogue_[7][27] + NpcSystem::dialogue_[7][28];
    helpStrings[7] = NpcSystem::dialogue_[7][30];
    helpStrings[8] = NpcSystem::dialogue_[7][32] + NpcSystem::dialogue_[7][33];
    helpStrings[9] = NpcSystem::dialogue_[7][35] + NpcSystem::dialogue_[7][36];
    helpStrings[10] = NpcSystem::dialogue_[7][38];
    helpStrings[11] = NpcSystem::dialogue_[7][40];
}

SharedArray<SharedArray<std::string>> itemEffectText;

static void init_item_effect_text() {
    itemEffectText = SharedArray<SharedArray<std::string>>(13);
    const char *rows[13][2] = {{"Warp to camp", ""},    {"Cures ailment", ""},
                               {"Restores Health", ""}, {"Restores Magicka", ""},
                               {"", ""},                {"Grants level", "experience"},
                               {"Health & Magicka", ""}, {"Increase harm", ""},
                               {"Increase armor", ""},  {"Safe camping", ""},
                               {"Kill monster", ""},    {"Kill monster", ""},
                               {"Kill monster", ""}};
    for (int32_t r = 0; r < 13; ++r) {
        itemEffectText[r] = SharedArray<std::string>(2);
        itemEffectText[r][0] = std::string(rows[r][0]);
        itemEffectText[r][1] = std::string(rows[r][1]);
    }
}

void stormhold_init_statics(platform::PlatformContext *context) {
    // Another session may have loaded Dawnstar's shared tables and UI statics.
    // Reinitialize on every boot, just as Dawnstar does.
    (void)context;
    RegisteredApplication::initializeStatics();
    CusImage::initializeStatics();
    init_item_effect_text();
    Dungeon::initializeStatics();
    Player::initializeStatics();
    NpcSystem::initializeStatics();
    GameCanvas::initializeStatics(profile());
    UIWidget::initializeStatics();
}

}
