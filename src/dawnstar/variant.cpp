#include "src/dawnstar/variant.hpp"

#include <stdexcept>

#include "src/common/game/game_canvas.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/registered_application.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/game/util.hpp"
#include "src/dawnstar/dungeon.hpp"
#include "src/dawnstar/dungeon_gen.hpp"
#include "src/dawnstar/extension.hpp"
#include "src/dawnstar/npc_script.hpp"
#include "src/dawnstar/profile.hpp"

namespace dawnstar {

game::SplashArt Variant::loadSplashArt() {
    game::SplashArt art;
    art.top = Image::createImage(game_.platformContext_, std::string("/splashtop.png"));
    art.bottom = Image::createImage(game_.platformContext_, std::string("/splashbot.png"));
    return art;
}

void Variant::loadTables() {
    this->createImageFromFile();
    game_.splashUI_->progressPercent_ = 5;
    game_.splashArt_.carrierLogo = this->createImage(std::string("mformaLogo.png"));
    game_.splashArt_.publisherLogo = this->createImage(std::string("vir2lLogo.png"));
    this->loadHelpStrings();
}

void Variant::loadArt() {
    GameCanvas::floorImage_ = this->createImage(std::string("floor3.png"));
    GameCanvas::floorIceImage_ = this->createImage(std::string("floorIce.png"));
    GameCanvas::wallRightImage_ = this->createImage(std::string("wallsr.png"));
    GameCanvas::wallInnerImage_ = this->createImage(std::string("wallsi.png"));
    GameCanvas::gateImage_ = this->createImage(std::string("gate.png"));
    game_.checkDestroyed();
    GameCanvas::bagSprites_ = SharedArray<render::Sprite *>(3);
    GameCanvas::bagSprites_[0] = new render::ImageSprite(this->createImage(std::string("baglarge.png")));
    GameCanvas::bagSprites_[1] = new render::ImageSprite(this->createImage(std::string("bagmid.png")));
    GameCanvas::bagSprites_[2] = new render::ImageSprite(this->createImage(std::string("bagsmall.png")));
    game_.checkDestroyed();
    GameCanvas::chestSprites_ = SharedArray<render::Sprite *>(3);
    GameCanvas::chestSprites_[0] = new render::ImageSprite(this->createImage(std::string("chestnearclosed.png")));
    GameCanvas::chestSprites_[1] = new render::ImageSprite(this->createImage(std::string("chestmidclosed.png")));
    GameCanvas::chestSprites_[2] = new render::ImageSprite(this->createImage(std::string("chestfarclosed.png")));
    game_.checkDestroyed();
    GameCanvas::hudIconAtlas_ = this->createImage(std::string("icons.png"));
    GameCanvas::hudPanelImage_ = this->createImage(std::string("panel.png"));
    game_.checkDestroyed();
}

render::Sprite *Variant::loadMonsterSprite(const std::string &name) {
    return new render::ImageSprite(this->createImage(name));
}

void Variant::allocateScreens() {
    game_.GenericInfoUI_ = game_.makeOwnedUIWidget(4, screens::REGISTRATION_EXIT);
    NPCQuestionWhatUI_ = game_.makeOwnedUIWidget(5, screens::NPC_QUESTION_WHAT);
    SharedArray<std::string> stringArray2{"North wall defense", "East wall defense",
                                "Arguing with governor", "Imperial aid", "Ice tribes",
                                "Gates before attack"};
    NPCQuestionWhatUI_->setupForm(std::string(""), std::string("Ask about what?"), stringArray2);
    game_.NPCChoicesUI_ = SharedArray<UIWidget *>(9);
    SharedArray<std::string> stringArray5{"Buy", "Sell"};
    int32_t n2 = 0;
    while (n2 < 4) {
        game_.NPCChoicesUI_[n2] = game_.makeOwnedUIWidget(5, 9 + n2);
        game_.NPCChoicesUI_[n2]->setupForm(NpcSystem::npcNames_[n2], std::string("Your gold: <TAG>"), stringArray5);
        game_.NPCChoicesUI_[n2]->backTarget_ = game_.gameCanvas_;
        ++n2;
    }
    game_.NPCChoicesUI_[4] = game_.makeOwnedUIWidget(5, screens::NPC_CHOICES_4);
    SharedArray<std::string> stringArray6{"Rumors", "Cure", "Warp", "Recovery"};
    game_.NPCChoicesUI_[4]->setupForm(std::string("Eustacia"), std::string("Welcome"), stringArray6);
    game_.NPCChoicesUI_[4]->backTarget_ = game_.gameCanvas_;
    SharedArray<std::string> stringArray7{"Train", "Give", "Befriend", "Threaten", "Ask a question",
                                "Warp"};
    int32_t n3 = 5;
    while (n3 < 9) {
        game_.NPCChoicesUI_[n3] = game_.makeOwnedUIWidget(5, 9 + n3);
        game_.NPCChoicesUI_[n3]->setupForm(NpcSystem::npcNames_[n3], std::string("Aid: <TAG>"), stringArray7);
        game_.NPCChoicesUI_[n3]->backTarget_ = game_.gameCanvas_;
        ++n3;
    }
    ClueUI_ = game_.makeOwnedUIWidget(5, screens::CLUE_LIST);
    SharedArray<std::string> stringArray9{"Alhavara", "Beatrice", "Chung", "Delacroix", "Rumors"};
    ClueUI_->setupForm(std::string("Clue Log"), std::string(""), stringArray9);
    ClueUI_->backTarget_ = game_.OptionsUI_;
}

void Variant::buildDungeons() {
    this->imageIndex_.clear();
    game_.dungeons_.clear();
    game_.splashUI_->progressPercent_ = 60;
    DungeonGen dungeonGen(game_.dungeons_, game_.splashUI_, game_.worldState_);
}

void Variant::createNewGame() {
    game_.createGameUI_->progressPercent_ = 5;
    game_.mainMenuUI_ = nullptr;
    game_.newGameUI_ = nullptr;
    game_.characterMainUI_ = nullptr;
    game_.charNameFieldStorage_.reset();
    game_.charNamePromptStorage_.reset();
    game_.charNameTextFormStorage_.reset();
    game_.charNameTextForm_ = nullptr;
    game_.splashUI_ = nullptr;
    game_.createGameUI_->progressPercent_ = 10;
    NpcSystem::loadDialogue(game_.platformContext_);
    game_.createGameUI_->progressPercent_ = 100;
    game_.setCurrentDisplay(game_.GenericInfoUI_);
}

void Variant::resumeGame() {
    int32_t n1 = this->gameAdvancementLevel(game_.character_->giftPoints_);
    this->openAndRepopulateDungeons(n1);
}

const char *Variant::screenName(int32_t screenId) const {
    switch (screenId) {
        case screens::NPC_CHOICES_0:
        case screens::NPC_CHOICES_1:
        case screens::NPC_CHOICES_2:
        case screens::NPC_CHOICES_3:
        case screens::NPC_CHOICES_4:
        case screens::NPC_CHOICES_5:
        case screens::NPC_CHOICES_6:
        case screens::NPC_CHOICES_7:
        case screens::NPC_CHOICES_8: return "npc choices";
        case screens::NPC_QUESTION_RESPONSE: return "npc question response";
        case screens::NPC_QUESTION_WHAT: return "npc question what";
        case screens::NPC_QUESTION_WHOM: return "npc question whom";
        case screens::NPC_TRAVEL: return "npc travel";
        case screens::NPC_BUY_WHAT: return "npc buy what";
        case screens::NPC_BUY_RESPONSE: return "npc buy response";
        case screens::NPC_SELL_WHAT: return "npc sell what";
        case screens::NPC_SELL_RESPONSE: return "npc sell response";
        case screens::NPC_CONFIRM_SALE: return "npc confirm sale";
        case screens::CLUE_LIST: return "clue list";
        case screens::CLUE_INFO: return "clue info";
        case screens::REVEAL_CONFIRM: return "reveal confirm";
        case screens::REVEAL_WHOM: return "reveal whom";
        case screens::REVEAL_RESULT: return "reveal result";
        case screens::REVEAL_INTRO: return "reveal intro";
        case screens::CAMP_CONFIRM: return "camp confirm";
        case screens::INTRODUCTION_SECOND: return "introduction (page 2)";
        case screens::REGISTRATION_EXIT: return "registration exit";
        default: return nullptr;
    }
}

void Variant::openAndRepopulateDungeons(int32_t n) {
    (void)n;
    int32_t n1 = 0;
    int32_t n2 = 0;
    while (n2 < 37) {
        int32_t n3 = n2;
        Dungeon *dungeon = game_.dungeons_.as<Dungeon>((std::size_t)n3);
        dungeon->populated_ = true;
        dungeon->rebuildOccupancy();
        game_.loadGameUI_->progressPercent_ = 100 * ++n1 / 37;
        if (game_.loadGameUI_->progressPercent_ > 100) {
            game_.loadGameUI_->progressPercent_ = 100;
        }
        ++n2;
    }
}

int32_t Variant::gameAdvancementLevel(int32_t n) {
    if (n < 17) {
        return 0;
    }
    if (n < 29) {
        return 1;
    }
    if (n < 38) {
        return 2;
    }
    if (n < 49) {
        return 3;
    }
    if (n < 62) {
        return 4;
    }
    return 5;
}

void Variant::afterLoad() {
    if (game_.mainMenuUI_ != nullptr) {
        game_.mainMenuUI_ = nullptr;
        game_.newGameUI_ = nullptr;
        game_.characterMainUI_ = nullptr;
        game_.charNameFieldStorage_.reset();
        game_.charNamePromptStorage_.reset();
        game_.charNameTextFormStorage_.reset();
        game_.charNameTextForm_ = nullptr;
        game_.splashUI_ = nullptr;
        NpcSystem::loadDialogue(game_.platformContext_);
    }
}

UIWidget *Variant::infoWidget(int32_t screenId) {
    game_.GenericInfoUI_->setScreenId(screenId);
    return game_.GenericInfoUI_;
}

UIWidget *Variant::infoBox(int32_t screenId, const std::string &title, const std::string &body,
                           game::DisplayTarget back, game::DisplayTarget next) {
    (void)back;
    (void)next;
    game_.GenericInfoUI_->setScreenId(screenId);
    game_.GenericInfoUI_->setupTextBox(title, body);
    return game_.GenericInfoUI_;
}

void Variant::refreshNpcAid(int32_t n) {
    if (n == 4) {
        return;
    }
    std::string string1 = game_.NPCChoicesUI_[n]->tagTemplate_;
    std::string string2 = game_.NPCChoicesUI_[n]->bodyText();
    int32_t n1 = 0;
    if (NpcSystem::isChampion(n)) {
        n1 = game_.worldState_.npcs.aidPoints[n - 5];
    } else if (NpcSystem::isPeddler(n)) {
        n1 = game_.character_->gold_;
    }
    string2 = GameUtil::replace(string1, std::string("<TAG>"), n1);
    game_.NPCChoicesUI_[n]->setBodyText(string2);
}

std::string Variant::introductionText(int32_t page) {
    if (page == 0) {
        return NpcSystem::dialogue_[9][3];
    }
    return NpcSystem::dialogue_[9][4] + NpcSystem::dialogue_[9][5];
}

void Variant::showEndOfGame() {
    game_.endOfGameUI_ = this->newEndOfGameUI();
    game_.setCurrentDisplay(game_.endOfGameUI_);
}

void Variant::showGameOver() {
    game_.endOfGameUI_ = this->newGameOverUI();
    game_.setCurrentDisplay(game_.endOfGameUI_);
}

bool Variant::performMenuAction(menuaction::Action action) {
    switch (action) {
    case menuaction::CLUE_LOG: {
        ClueUI_->backTarget_ = game_.currentUI_;
        game_.setCurrentDisplay(ClueUI_);
        return true;
    }
    case menuaction::REVEAL_TRAITOR: {
        if (ext(game_.character_).traitorRevealed_) {
            // Already solved: show the verdict again as a reminder and go back to
            // the game. REVEAL_RESULT leaves a running countdown alone, so this
            // cannot re-trigger the accusation, the Star of Frost, or the reset.
            std::string text = NpcSystem::dialogue_[9][68] + "\n" + NpcSystem::dialogue_[9][69] +
                               "\n" + NpcSystem::dialogue_[9][70];
            game_.GenericInfoUI_->setScreenId(screens::REVEAL_RESULT);
            game_.GenericInfoUI_->setupTextBox(std::string("Reveal Traitor"), text);
            game_.setCurrentDisplay(game_.GenericInfoUI_);
            return true;
        }
        game_.GenericInfoUI_->setScreenId(screens::REVEAL_INTRO);
        game_.GenericInfoUI_->setupTextBox(std::string("Reveal Traitor"), NpcSystem::dialogue_[9][66]);
        game_.setCurrentDisplay(game_.GenericInfoUI_);
        return true;
    }
    default: return false;
    }
}

void Variant::openNpcScreen(GameCanvas &canvas, int32_t n) {
    std::optional<std::string> string1 = NpcSystem::interact(canvas.player_, n, 1, 0);
    if (string1) {
        game_.GenericInfoUI_->setScreenId(8);
        game_.GenericInfoUI_->setupTextBox(NpcSystem::npcNames_[n], *string1);
        game_.GenericInfoUI_->contextIndex_ = n;
        this->refreshNpcAid(n);
        game_.setCurrentDisplay(game_.GenericInfoUI_);
        canvas.repaintEnabled_ = false;
    } else if (n == 4) {
        game_.setCurrentDisplay(game_.NPCChoicesUI_[4]);
        canvas.repaintEnabled_ = false;
    }
}

void Variant::newClueLogUI(int32_t n) {
    std::string string1;
    std::string out;
    if (n == 4) {
        for (int32_t n1 = 0; n1 < 6; ++n1) {
            if (ext(game_.character_).questFlags_[90 + n1]) {
                out += NpcSystem::dialogue_[9][5 + NpcSystem::kTraitorRumors[ext(game_.character_).traitorId_][n1]];
                out += "\n";
            }
        }
        string1 = "Rumors";
    } else {
        int32_t n2 = 18 * n;
        bool bl1 = n == ext(game_.character_).traitorId_;
        for (int32_t n4 = 0; n4 < 6; ++n4) {
            int32_t n3 = 0;
            for (int32_t n5 = 0; n5 < 3; ++n5) {
                if (ext(game_.character_).questFlags_[n2 + n4 * 3 + n5]) {
                    if (n5 >= n) {
                        n3 = 1;
                    }
                    if (bl1 && ext(game_.character_).questFlags_[72 + n4 * 3 + n5]) {
                        out += NpcSystem::dialogue_[9][5 + NpcSystem::kClueFalse[n4 * 4 + n5 + n3]];
                    } else {
                        out += NpcSystem::dialogue_[9][5 + NpcSystem::kClueTrue[n4 * 4 + n5 + n3]];
                    }
                    out += "\n";
                }
            }
        }
        string1 = NpcSystem::npcNames_[5 + n];
    }
    if (out.empty()) {
        out = "You have no information yet.";
    }
    game_.GenericInfoUI_->setScreenId(screens::CLUE_INFO);
    game_.GenericInfoUI_->setupTextBox(string1, std::string(out));
}

UIWidget *Variant::newRevealUI() {
    UIWidget *revealScreen = game_.makeOwnedUIWidget(5, screens::REVEAL_CONFIRM);
    SharedArray<std::string> stringArray1{"Yes", "No"};
    revealScreen->setupForm(std::string("Reveal Traitor"), NpcSystem::dialogue_[9][67], stringArray1);
    revealScreen->b(UIWidget::cmdCancel_);
    revealScreen->backTarget_ = game_.OptionsUI_;
    return revealScreen;
}

UIWidget *Variant::newRevealWhomUI() {
    UIWidget *revealWhomScreen = game_.makeOwnedUIWidget(5, screens::REVEAL_WHOM);
    SharedArray<std::string> stringArray1{"Alhavara", "Beatrice", "Chung", "Delacroix"};
    revealWhomScreen->setupForm(std::string("Reveal Traitor"), std::string("Who is the Traitor?"), stringArray1);
    revealWhomScreen->backTarget_ = game_.OptionsUI_;
    return revealWhomScreen;
}

UIWidget *Variant::newEndOfGameUI() {
    std::string string1 = NpcSystem::dialogue_[9][74] + "\n" + NpcSystem::dialogue_[9][75] + "\n" + NpcSystem::dialogue_[9][76];
    UIWidget *endOfGameScreen = game_.makeOwnedUIWidget(4, uistate::SCREEN_END_OF_GAME);
    endOfGameScreen->setupTextBox(std::string("Victory!"), string1);
    return endOfGameScreen;
}

UIWidget *Variant::newGameOverUI() {
    std::string string1 = NpcSystem::dialogue_[9][73];
    UIWidget *gameOverScreen = game_.makeOwnedUIWidget(4, uistate::SCREEN_GAME_OVER);
    string1 = GameUtil::replace(string1, std::string("<TAG>"), NpcSystem::npcNames_[5 + ext(game_.character_).traitorId_]);
    gameOverScreen->setupTextBox(std::string("Game Over"), string1);
    return gameOverScreen;
}

void Variant::createImageFromFile() {
    struct ImageEntry {
        std::string name;
        int32_t length;
    };
    std::unordered_map<int32_t, ImageEntry> entries;
    int32_t n1 = -1;
    int32_t n2 = -1;
    int32_t n3 = -1;
    int32_t n4 = 0;
    BinaryReader *dataInputStream = nullptr;
    Image *image = nullptr;
    try {
        dataInputStream = GameUtil::openResource(game_.platformContext_, std::string("/imgfiles.lmp"));
        while (n3 == -1 || n4 < n3) {
            std::string string1 = "";
            int32_t n5 = dataInputStream->read();
            ++n4;
            while (n5 != 45) {
                if (n5 == -1) {
                    throw std::runtime_error(
                        "imgfiles.lmp: end of archive while reading a name at byte " +
                        std::to_string(n4));
                }
                string1 = string1 + (char)n5;
                n5 = dataInputStream->read();
                ++n4;
            }
            if (string1.length() <= 1) continue;
            int32_t off0 = dataInputStream->read() & 0xFF;
            int32_t off1 = dataInputStream->read() & 0xFF;
            int32_t off2 = dataInputStream->read() & 0xFF;
            int32_t off3 = dataInputStream->read() & 0xFF;
            n2 = (off0 << 24) + (off1 << 16) + (off2 << 8) + off3;
            int32_t len0 = dataInputStream->read() & 0xFF;
            int32_t len1 = dataInputStream->read() & 0xFF;
            n1 = (len0 << 8) + len1;
            n4 += 6;
            entries[n2] = ImageEntry{string1, n1};
            if (n3 >= 0) continue;
            n3 = n2;
        }
        int32_t n6 = (int32_t)entries.size();
        int32_t scanLimit = n3 + n4 + 0x100000;
        while (n6 > 0) {
            if (n4 > scanLimit) {
                throw std::runtime_error("imgfiles.lmp: " + std::to_string(n6) +
                                         " image(s) unaccounted for; scanned past byte " +
                                         std::to_string(n4));
            }
            auto entry = entries.find(n4);
            if (entry == entries.end()) {
                platform::writeLogLine("ERROR: image not found");
                ++n4;
                continue;
            }
            std::string string3 = entry->second.name;
            n1 = entry->second.length;
            entries.erase(entry);
            SharedArray<int8_t> byArray1(n1);
            dataInputStream->read(byArray1);
            image = Image::createImage(game_.platformContext_, byArray1, 0, n1);
            this->imageIndex_[string3] = image;
            n4 += n1;
            --n6;
        }
        delete dataInputStream;
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("ERROR: failed to create image from file: ") + exception.what());
    }
}

Image *Variant::createImage(const std::string &stringIn) {
    std::string string1 = stringIn;
    if (string1.rfind("/", 0) == 0) {
        string1 = string1.substr(1);
    }
    auto found = this->imageIndex_.find(string1);
    Image *image = found == this->imageIndex_.end() ? nullptr : found->second;
    if (image == nullptr) {
        platform::writeLogLine("ERROR: image not found: " + string1);
    }
    return image;
}

void Variant::loadHelpStrings() {
    SharedArray<std::string> stringArray1(35);
    try {
        BinaryReader *object1 = GameUtil::openDatFile(game_.platformContext_, std::string("helptext.dat"));
        int32_t n1 = object1->readInt();
        if (n1 != 35) {
            platform::writeLogLine("ERROR: unexpected number of help messages: " + std::to_string(n1));
        }
        stringArray1 = SharedArray<std::string>(n1);
        int32_t n2 = 0;
        while (n2 < n1) {
            stringArray1[n2] = object1->readUTF();
            ++n2;
        }
        delete object1;
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("ERROR: failed to load help text: ") +
                               exception.what());
    }
    SharedArray<std::string> &helpTitles = game_.helpTitles_;
    SharedArray<std::string> &helpStrings = game_.helpStrings_;
    helpTitles[0] = stringArray1[0];
    helpTitles[1] = stringArray1[2];
    helpTitles[2] = stringArray1[5];
    helpTitles[3] = stringArray1[7];
    helpTitles[4] = stringArray1[13];
    helpTitles[5] = stringArray1[15];
    helpTitles[6] = stringArray1[18];
    helpTitles[7] = stringArray1[23];
    helpTitles[8] = stringArray1[25];
    helpTitles[9] = stringArray1[28];
    helpTitles[10] = stringArray1[31];
    helpTitles[11] = stringArray1[33];
    helpStrings[0] = stringArray1[1];
    helpStrings[1] = stringArray1[3] + stringArray1[4];
    helpStrings[2] = stringArray1[6];
    helpStrings[3] = stringArray1[8] + stringArray1[9] + stringArray1[10] + stringArray1[11] + stringArray1[12];
    helpStrings[4] = stringArray1[14];
    helpStrings[5] = stringArray1[16] + stringArray1[17];
    helpStrings[6] = stringArray1[19] + stringArray1[20] + stringArray1[21] + stringArray1[22];
    helpStrings[7] = stringArray1[24];
    helpStrings[8] = stringArray1[26] + stringArray1[27];
    helpStrings[9] = stringArray1[29] + stringArray1[30];
    helpStrings[10] = stringArray1[32];
    helpStrings[11] = stringArray1[34];
}

int32_t Variant::nextInt(int32_t n) {
    return wrappingAbs(game_.worldState_.random->nextInt() % n);
}

SharedArray<std::string> itemEffectText;

static void init_item_effect_text() {
    itemEffectText = SharedArray<std::string>{"Warp to camp", "Cures ailment", "Restores Health",
                                    "Restores Magicka", " ", "Grants level experience",
                                    "Health & Magicka", "Increase harm", "Increase armor",
                                    "Safe camping", "Kills weak monster",
                                    "Kills normal monster", "Kills strong monster"};
}

void dawnstar_init_statics(platform::PlatformContext *context) {
    (void)context;
    RegisteredApplication::initializeStatics();
    init_item_effect_text();
    Dungeon::initializeStatics();
    NpcSystem::initializeStatics();
    UIWidget::initializeStatics();
    GameCanvas::initializeStatics(profile());
    Player::initializeStatics();
}

}
