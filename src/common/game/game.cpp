#include "src/common/game/game.hpp"

#include "src/common/game/game_canvas.hpp"
#include "src/common/game/items.hpp"
#include "src/common/game/monster.hpp"
#include "src/common/game/player.hpp"
#include "src/common/game/portoptions_menu.hpp"
#include "src/common/game/save_codec.hpp"
#include "src/common/game/savegame.hpp"
#include "src/common/game/spells.hpp"
#include "src/common/game/ui_widget.hpp"
#include "src/common/game/uistate.hpp"
#include "src/common/game/util.hpp"

Game::Game(const game::Profile &profile, platform::PlatformContext *platformContext)
    : platformContext_(platformContext), profile_(&profile) {
    uiCanvasStorage_ = std::make_unique<game::WidgetCanvas>(platformContext_);
    uiCanvas_ = uiCanvasStorage_.get();
    worldState_.npcs.initialize();
    rng_ = std::make_unique<GameRandom>(platformContext_->nowMillis());
    worldState_.platformContext = platformContext_;
    worldState_.random = rng_.get();
    worldState_.chests.initializeTables((std::size_t)profile.firstRecordTable);
    worldState_.monsters.initializeTables((std::size_t)profile.firstRecordTable,
                                          profile.monsterKey);
    this->appName_ = "The Elder Scrolls";
    this->display_ = nullptr;
    applicationState_ = 1;
    variant_ = profile.newVariant(*this);
}

Game::~Game() = default;

GameCanvas *Game::createGameCanvas() {
    gameCanvasStorage_ = std::make_unique<GameCanvas>(this);
    gameCanvas_ = gameCanvasStorage_.get();
    return gameCanvas_;
}

UIWidget *Game::makeOwnedUIWidget(int32_t layout, int32_t screenId) {
    auto widget = std::make_unique<UIWidget>(this, layout, screenId);
    UIWidget *result = widget.get();
    ownedUiWidgets_.push_back(std::move(widget));
    return result;
}

void Game::startRegisteredApplication() {
    if (this->display_ == nullptr) {
        this->display_ = &this->displayStorage_;
        this->createGameCanvas();
        this->currentItemIndex_ = -1;
        this->currentSpellIndex_ = -1;
        this->imgloadRunning_ = false;
        this->killThread_ = false;
        this->loadingDungeonId_ = 0;
        this->imgsLoaded_ = false;
        this->createErrorForm();
        this->initSplash();
        applicationState_ = 2;
    } else {
        this->gameCanvas_->repaint();
        this->display_->setCurrent(this->gameCanvas_);
    }
}

void Game::initSplash() {
    try {
        splashArt_ = variant_->loadSplashArt();
        this->splashUI_ = makeOwnedUIWidget(2, uistate::SCREEN_SPLASH);
        this->splashUI_->bindCanvas();
        this->splashUI_->nextTarget_ = this->errorForm_;
        this->showSplash_ = true;
        showCarrierLogo_ = false;
        this->setCurrentDisplay(this->splashUI_);
        this->startHelperJob(2);
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("ERROR: failed to initialise the splash screen: ") +
                               exception.what());
        this->display_->setCurrent(this->errorForm_);
    }
}

void Game::startHelperJob(int32_t job) {
    helperThreadState_ = job;
    if (helperJobRunner_ != nullptr) {
        helperJobRunner_(this, job);
    }
}

void Game::runHelperJob(int32_t job) {
    if (job == 1) {
        platform::writeLogLine("ERROR: reached unsupported job id: 1");
    } else if (job == 2) {
        this->runAppload();
    } else if (job == 4) {
        variant_->createNewGame();
    } else if (job == 5) {
        if (this->saveGameState()) {
            this->setCurrentDisplay(this->gameCanvas_);
        } else {
            this->setCurrentDisplay(variant_->infoBox(
                uistate::SCREEN_EXIT_ALTERNATE, std::string("Save Error"),
                std::string("There was an error in saving your character record. Your previous "
                       "character record is still saved. Try turning your phone off then on "
                       "again to clear the memory.")));
        }
    } else if (job == 6) {
        if (this->loadGameState()) {
            variant_->resumeGame();
            this->loadingDungeonId_ = this->character_->dungeonId_;
            this->imgloadRunning_ = true;
            reloadGame_ = true;
            reloadGame_ = false;
            this->imgloadRunning_ = false;
            loadGameUI_->progressPercent_ = 100;
            loadGameUI_->requestRepaint();
            loadGameUI_->flushRepaints();
            if (profile_->loadRefreshesSurroundings) {
                this->character_->refreshSurroundings();
            }
            this->gameCanvas_->player_ = this->character_;
            this->gameCanvas_->stateChanged_ = true;
            this->gameCanvas_->startLoop();
            this->setCurrentDisplay(this->gameCanvas_);
        } else {
            this->setCurrentDisplay(this->noSavedGameUI_);
        }
    } else {
        this->runImageLoader();
        this->setCurrentDisplay(this->gameCanvas_);
    }
}

void Game::runAppload() {
    try {
        this->showSplash_ = true;
        showCarrierLogo_ = false;
        this->showSplash_ = false;
        this->splashUI_->progressPercent_ = 0;
        this->allocateGame();
        this->allocateAllUIs();
        variant_->buildDungeons();
        this->splashUI_->progressPercent_ = 100;
    } catch (std::exception &throwable) {
        platform::writeLogLine(std::string("ERROR: failed to start the application: ") +
                               throwable.what());
        this->setCurrentDisplay(this->errorForm_);
    }
}

void Game::allocateGame() {
    Player::ensureDataLoaded(platformContext_);
    variant_->loadTables();
    this->loadMonsterFilenames();
    this->splashUI_->progressPercent_ = 10;
    Items::loadAll(platformContext_);
    Spell::load(platformContext_);
    worldState_.monsterUid = 0;
    Monster::loadTypes(platformContext_);
    this->splashUI_->progressPercent_ = 15;
}

void Game::loadMonsterFilenames() {
    BinaryReader *dataInputStream =
        GameUtil::openDatFile(platformContext_, std::string("monsterfilenamesin.dat"));
    monsterFilenames_ = SharedArray<SharedArray<std::string>>(5);
    int32_t n1 = 0;
    while (n1 < 5) {
        monsterFilenames_[n1] = SharedArray<std::string>(7);
        int32_t n2 = 0;
        while (n2 < 7) {
            monsterFilenames_[n1][n2] = dataInputStream->readUTF();
            ++n2;
        }
        ++n1;
    }
    delete dataInputStream;
}

void Game::allocateAllUIs() {
    this->splashUI_->progressPercent_ = 20;
    this->checkDestroyed();
    GameCanvas::npcSprites_ = SharedArray<render::Sprite *>(profile_->npcSpriteSlots);
    int32_t n1 = 0;
    while (n1 < profile_->npcSpriteSlots) {
        GameCanvas::npcSprites_[n1] = nullptr;
        ++n1;
    }
    variant_->loadArt();
    this->loadingDungeonId_ = 1;
    this->imgloadRunning_ = true;
    this->loadDungeonUI_ = makeOwnedUIWidget(11, uistate::SCREEN_PROGRESS_LOAD_DUNGEON);
    this->loadDungeonUI_->bindCanvasAgain();
    this->loadCampMonsters();
    this->checkDestroyed();
    {
        creditsString_ = std::string("Game Design: Anthony Gill and Greg Gorden\n"
                                "Art: Mark Jones\n"
                                "Programming: ") +
                         profile_->creditsProgramming +
                         "\nTechnical Director: Andrew Friedman\n"
                         "(C) 2003 Vir2L Studos, a ZeniMax Media company. The Elder Scrolls and Vir2L are "
                         "registered trademarks of ZeniMax Media Inc. All rights reserved.\n";
    }
    this->mainMenuUI_ = makeOwnedUIWidget(3, uistate::SCREEN_MAIN_MENU);
    SharedArray<std::string> stringArray1{"New Game", "Continue Game", "Display & Controls",
                                "Help", "Credits", "Game Select", "Exit"};
    this->mainMenuUI_->setupList(std::string("Main Menu"), stringArray1, false);
    this->newGameUI_ = makeOwnedUIWidget(5, uistate::SCREEN_CLASS_SELECT);
    SharedArray<std::string> stringArray3 = Player::classNames_;
    this->newGameUI_->setupForm(std::string("New Game"), std::string("Select a Class:"), stringArray3);
    this->splashUI_->progressPercent_ = 35;
    this->characterMainUI_ = makeOwnedUIWidget(6, uistate::SCREEN_CHARACTER_SHEET);
    SharedArray<std::string> stringArray4{"See Class Info", "Create Character"};
    this->characterMainUI_->setupFormWithSubtitle(std::string("Character"), std::string("You selected:"),
                                                  std::string(""), stringArray4);
    this->OptionsUI_ = makeOwnedUIWidget(3, uistate::SCREEN_OPTIONS);
    SharedArray<std::string> stringArray8(profile_->optionsRowCount);
    for (int32_t row = 0; row < profile_->optionsRowCount; ++row) {
        stringArray8[row] = std::string(menuaction::optionsLabel(profile_->optionsRows[row]));
    }
    this->OptionsUI_->setupList(std::string("Options"), stringArray8, false);
    this->OptionsUI_->a(UIWidget::cmdBack_);
    this->helpUI_ = this->newHelpUI(nullptr);
    this->splashUI_->progressPercent_ = 42;
    this->charNameTextFormStorage_ = std::make_unique<Form>(std::string("Enter name"));
    this->charNameTextForm_ = this->charNameTextFormStorage_.get();
    this->charNamePromptStorage_ = std::make_unique<StringItem>(
        std::string(), std::string("Enter a name for your character"));
    this->charNameTextForm_->append(this->charNamePromptStorage_.get());
    this->charNameFieldStorage_ =
        std::make_unique<TextField>(std::string(), std::string(), 10, 0);
    this->charNameTextForm_->append(this->charNameFieldStorage_.get());
    this->charNameTextForm_->addCommand(UIWidget::cmdOk_);
    if (profile_->nameFormCancels) {
        this->charNameTextForm_->addCommand(UIWidget::cmdCancel_);
    }
    this->charNameTextForm_->setCommandListener(this);
    this->noSavedGameUI_ = makeOwnedUIWidget(4, uistate::SCREEN_NO_SAVED_GAME);
    this->noSavedGameUI_->setupTextBox(
        std::string("Unavailable"),
        std::string("No game is available for loading. Press OK to return to main menu."));
    this->noSavedGameUI_->backTarget_ = this->mainMenuUI_;
    this->splashUI_->nextTarget_ = this->mainMenuUI_;
    this->newGameUI_->backTarget_ = this->mainMenuUI_;
    this->characterMainUI_->backTarget_ = this->newGameUI_;
    variant_->allocateScreens();
    this->splashUI_->progressPercent_ = 55;
}

UIWidget *Game::newInventoryUI() {
    UIWidget *inventoryScreen = makeOwnedUIWidget(5, uistate::SCREEN_INVENTORY);
    SharedArray<std::string> stringArray1(this->character_->itemCount_);
    int32_t n1 = 0;
    while (n1 < this->character_->itemCount_) {
        int8_t by1 = this->character_->inventory_[n1];
        stringArray1[n1] = by1 < 0 ? "E: " + Items::nameOf(wrappingAbs(by1)) : Items::nameOf(by1);
        ++n1;
    }
    std::string string1 = profile_->inventoryShowsGold
                         ? GameUtil::replace(std::string("Your gold: <TAG>"), std::string("<TAG>"),
                                             this->character_->gold_)
                         : std::string("Items:");
    inventoryScreen->setupForm(std::string("Inventory"), string1, stringArray1);
    inventoryScreen->backTarget_ = this->OptionsUI_;
    return inventoryScreen;
}

UIWidget *Game::newPortOptionsUI(UIWidget *back) {
    UIWidget *widget = makeOwnedUIWidget(3, uistate::SCREEN_PORT_OPTIONS);
    widget->setupList(std::string("Display & Controls"),
                      portoptions::labels(platformContext_->portOptions()), false);
    widget->setHorizontalValueRows(portoptions::HANDSET_DEFAULTS);
    widget->a(UIWidget::cmdBack_);
    widget->backTarget_ = back;
    return widget;
}

void Game::refreshPortOptionsUI() {
    if (portOptionsUI_ != nullptr) {
        portOptionsUI_->setRowLabels(portoptions::labels(platformContext_->portOptions()));
    }
}

UIWidget *Game::newConfirmQuitUI(UIWidget *back) {
    UIWidget *g3 = makeOwnedUIWidget(5, uistate::SCREEN_CONFIRM_QUIT);
    SharedArray<std::string> stringArray1{"Yes", "No"};
    g3->setupForm(std::string("Quit?"), std::string("Are you sure?"), stringArray1);
    if (!profile_->quitFormKeepsCancel) {
        g3->b(UIWidget::cmdCancel_);
    }
    g3->backTarget_ = back;
    return g3;
}

UIWidget *Game::newHelpUI(UIWidget *back) {
    UIWidget *h3 = makeOwnedUIWidget(3, uistate::SCREEN_HELP);
    h3->setupList(std::string("Help"), helpTitles_, true);
    h3->backTarget_ = back;
    return h3;
}

void Game::pauseApplication() {
    this->gameCanvas_->pauseForUi();
    if (applicationState_ != 4) {
        applicationState_ = 3;
    }
}

void Game::destroyApplication(bool) {
    applicationState_ = 4;
}

bool Game::loadGameState() {
    bool bl1 = true;
    SaveRecords *saveRecords = nullptr;
    loadGameUI_->progressPercent_ = 0;
    std::optional<std::string> string1 = this->lastGoodSaveName();
    try {
        if (!string1) {
            throw std::runtime_error("No valid record store!");
        }
        platform::writeLogLine("Save: loading from slot \"" + *string1 + "\"");
        saveRecords = SaveRecords::open(platformContext_, *string1, false);
        // A complete save is the character, 109 master-list records, and the
        // other-state record. Older desktop builds appended generations while
        // still loading record 1, so recover the newest complete one.
        constexpr int32_t kRecordsPerSave = 111;
        const int32_t recordCount = saveRecords->recordCount();
        const int32_t generationStart =
            recordCount >= kRecordsPerSave
                ? ((recordCount / kRecordsPerSave) - 1) * kRecordsPerSave + 1
                : 1;
        SharedArray<int8_t> byArray1 = saveRecords->get(generationStart);
        if (!byArray1.isNull()) {
            platform::writeLogLine("Save: read character record, " +
                                   std::to_string(byArray1.length()) + " bytes");
        }
        this->characterStorage_.reset(Player::fromBytes(this->profile(), byArray1, true));
        this->character_ = this->characterStorage_.get();
        this->character_->world_ = this;
        platform::writeLogLine(
            "Loaded game position: dungeon=" + std::to_string((int)this->character_->dungeonId_) +
            " x=" + std::to_string((int)this->character_->gridX_) +
            " y=" + std::to_string((int)this->character_->gridY_) +
            " facing=" + std::to_string((int)this->character_->facing_));
        loadGameUI_->progressPercent_ = 20;
        loadGameUI_->requestRepaint();
        loadGameUI_->flushRepaints();
        int32_t n2 = this->readMasterListRecords(saveRecords, generationStart + 1);
        platform::writeLogLine("Save: read the master lists");
        byArray1 = saveRecords->get(n2);
        profile_->saveCodec->readOtherState(*this, byArray1);
        variant_->afterLoad();
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("ERROR: failed to load the saved game: ") +
                               exception.what());
        bl1 = false;
        return bl1;
    }
    try {
        saveRecords->close();
    } catch (const std::exception &exception) {
        // The records are already read, so this does not fail the load; it is
        // logged because it still points at a storage problem.
        platform::writeLogLine(std::string("Save: failed to close the save store after "
                                           "loading: ") + exception.what());
    }
    return bl1;
}

bool Game::saveGameState() {
    bool bl1 = true;
    SaveRecords *saveRecords = nullptr;
    saveGameUI_->progressPercent_ = 0;
    std::string string1 = this->unusedSaveName();
    try {
        platform::writeLogLine(
            "Saving game position: dungeon=" + std::to_string((int)this->character_->dungeonId_) +
            " x=" + std::to_string((int)this->character_->gridX_) +
            " y=" + std::to_string((int)this->character_->gridY_) +
            " facing=" + std::to_string((int)this->character_->facing_));
        saveRecords = SaveRecords::replace(platformContext_, string1);
        SharedArray<int8_t> byArray1 = this->character_->toBytes(true);
        saveGameUI_->progressPercent_ = 20;
        saveGameUI_->requestRepaint();
        saveGameUI_->flushRepaints();
        saveRecords->add(byArray1, 0, byArray1.length());
        this->writeMasterListsToSave(saveRecords);
        byArray1 = profile_->saveCodec->writeOtherState(*this);
        saveRecords->add(byArray1, 0, byArray1.length());
        saveRecords->close();
        saveRecords = nullptr;
        this->cleanupSaveRecords();
        saveGameUI_->progressPercent_ = 100;
        saveGameUI_->requestRepaint();
        saveGameUI_->flushRepaints();
        if (saveRecords == nullptr) return bl1;
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("ERROR: failed to save the game: ") + exception.what());
        bl1 = false;
        return bl1;
    } catch (...) {
        platform::writeLogLine("ERROR: failed to save the game (unknown exception)");
        bl1 = false;
        return bl1;
    }
    try {
        saveRecords->close();
        return bl1;
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("Save: failed to close the save store after "
                                           "saving: ") + exception.what());
    }
    return bl1;
}

void Game::createErrorForm() {
    this->errorFormStorage_ = std::make_unique<Form>(std::string("Error"));
    this->errorForm_ = this->errorFormStorage_.get();
    this->errItemStorage_ =
        std::make_unique<StringItem>(std::string("Error"), std::string("Cannot load game"));
    this->errItem_ = this->errItemStorage_.get();
    this->errorForm_->append(this->errItem_);
    this->errorForm_->addCommand(UIWidget::cmdOk_);
    this->errorForm_->setCommandListener(this);
}

void Game::displayError(const std::string &text) {
    Form *form = new Form(std::string("Error"));
    StringItem *stringItem = new StringItem(std::string("Error"), text);
    form->append(stringItem);
    form->addCommand(UIWidget::cmdOk_);
    form->setCommandListener(this);
    this->displayStorage_.setCurrent(form);
}

void Game::writeMasterListsToSave(SaveRecords *saveRecords) {
    int32_t n1;
    int32_t n2;
    int32_t n3 = 1;
    while (n3 < 37) {
        const worldstate::MonsterList &monsters =
            worldState_.monsters.at((std::size_t)n3);
        n2 = (int32_t)monsters.size();
        n1 = 4 + n2 * 28;
        BinaryWriter *outputStream = new BinaryWriter(n1);
        outputStream->writeInt(n2);
        Monster monster;
        for (const SharedArray<int8_t> &record : monsters) {
            Monster::fromRecord(&monster, record);
            monster.writeTo(outputStream);
        }
        SharedArray<int8_t> encoded = outputStream->toByteArray();
        saveRecords->add(encoded, 0, encoded.length());
        delete outputStream;
        saveGameUI_->progressPercent_ = 20 + 30 * (n3 + 1) / 37;
        saveGameUI_->requestRepaint();
        saveGameUI_->flushRepaints();
        ++n3;
    }
    n2 = 1;
    while (n2 < 37) {
        const worldstate::ChestList &chests = worldState_.chests.at((std::size_t)n2);
        int32_t n4 = (int32_t)chests.size();
        n1 = 4 + n4 * 8;
        BinaryWriter *dataOutputStream = new BinaryWriter(n1);
        dataOutputStream->writeInt(n4);
        for (const SharedArray<int8_t> &chest : chests) {
            Game::writeBytesToBinaryWriter(dataOutputStream, chest, 8);
        }
        SharedArray<int8_t> encoded = dataOutputStream->toByteArray();
        saveRecords->add(encoded, 0, encoded.length());
        delete dataOutputStream;
        saveGameUI_->progressPercent_ = 50 + 30 * (n2 + 1) / 37;
        saveGameUI_->requestRepaint();
        saveGameUI_->flushRepaints();
        ++n2;
    }
    int32_t n5 = 0;
    while (n5 < 37) {
        const worldstate::DroppedItemList &items =
            worldState_.droppedItems.at((std::size_t)n5);
        int32_t n6 = (int32_t)items.size();
        n1 = 4 + n6 * 7;
        BinaryWriter *dataOutputStream = new BinaryWriter(n1);
        dataOutputStream->writeInt(n6);
        for (const SharedArray<int8_t> &byArray1 : items) {
            Game::writeBytesToBinaryWriter(dataOutputStream, byArray1, 7);
        }
        SharedArray<int8_t> encoded = dataOutputStream->toByteArray();
        saveRecords->add(encoded, 0, encoded.length());
        delete dataOutputStream;
        saveGameUI_->progressPercent_ = 80 + 19 * (n5 + 1) / 37;
        saveGameUI_->requestRepaint();
        saveGameUI_->flushRepaints();
        ++n5;
    }
}

int32_t Game::readMasterListRecords(SaveRecords *saveRecords, int32_t n) {
    int32_t n1;
    int32_t n2 = n;
    int32_t n3 = 1;
    while (n3 < 37) {
        SharedArray<int8_t> byArray1 = saveRecords->get(n2++);
        BinaryReader *dataInputStream = new BinaryReader(byArray1);
        worldState_.monsters.at((std::size_t)n3).clear();
        int32_t n4 = dataInputStream->readInt();
        n1 = 0;
        while (n1 < n4) {
            Monster *d2 = Monster::readFrom(dataInputStream);
            worldState_.monsters.put((std::size_t)n3, d2->toRecord());
            ++n1;
        }
        delete dataInputStream;
        loadGameUI_->progressPercent_ = 20 + 30 * (n3 + 1) / 37;
        loadGameUI_->requestRepaint();
        loadGameUI_->flushRepaints();
        ++n3;
    }
    int32_t n5 = 1;
    while (n5 < 37) {
        SharedArray<int8_t> record = saveRecords->get(n2++);
        BinaryReader *dataInputStream = new BinaryReader(record);
        worldstate::ChestList &chests = worldState_.chests.at((std::size_t)n5);
        chests.clear();
        n1 = dataInputStream->readInt();
        int32_t n6 = 0;
        while (n6 < n1) {
            SharedArray<int8_t> chest = Game::readBytesFromBinaryReader(dataInputStream, 8);
            worldState_.chests.put((std::size_t)n5, chest);
            ++n6;
        }
        delete dataInputStream;
        loadGameUI_->progressPercent_ = 50 + 30 * (n5 + 1) / 37;
        loadGameUI_->requestRepaint();
        loadGameUI_->flushRepaints();
        ++n5;
    }
    int32_t n7 = 0;
    while (n7 < 37) {
        SharedArray<int8_t> byArray2 = saveRecords->get(n2++);
        BinaryReader *dataInputStream = new BinaryReader(byArray2);
        worldstate::DroppedItemList &items =
            worldState_.droppedItems.at((std::size_t)n7);
        items.clear();
        int32_t n8 = dataInputStream->readInt();
        int32_t n9 = 0;
        while (n9 < n8) {
            SharedArray<int8_t> dropped = Game::readBytesFromBinaryReader(dataInputStream, 7);
            items.push_back(dropped);
            ++n9;
        }
        delete dataInputStream;
        loadGameUI_->progressPercent_ = 80 + 19 * (n7 + 1) / 37;
        loadGameUI_->requestRepaint();
        loadGameUI_->flushRepaints();
        ++n7;
    }
    return n2;
}

SharedArray<int8_t> Game::readBytesFromBinaryReader(BinaryReader *dataInputStream, int32_t n) {
    return savegame::readBytes(dataInputStream, n);
}

void Game::writeBytesToBinaryWriter(BinaryWriter *dataOutputStream,
                                          const SharedArray<int8_t> &byArray, int32_t n) {
    savegame::writeBytes(dataOutputStream, byArray, n);
}

void Game::loadCampMonsters() {
    SharedArray<int8_t> byArray1(5);
    platform::writeLogLine("Art: loading warden images");
    byArray1[0] = 1;
    byArray1[1] = 1;
    byArray1[2] = 1;
    byArray1[3] = 1;
    byArray1[4] = 1;
    this->loadingDungeonId_ = 1;
    this->runImageLoaderFor(byArray1);
}

void Game::runImageLoader() {
    this->imgsLoaded_ = false;
    if (reloadGame_) {
        loadGameUI_->progressPercent_ = 80;
    } else {
        this->loadDungeonUI_->progressPercent_ = 0;
    }
    SharedArray<int8_t> byArray1(5);
    int32_t n1 = 0;
    while (n1 < 5) {
        byArray1[n1] = 0;
        ++n1;
    }
    if (this->loadingDungeonId_ == 1) {
        this->unloadAllMonsterImages();
        this->loadCampMonsters();
    } else {
        const std::size_t dungeonIndex = (std::size_t)(this->loadingDungeonId_ - 1);
        if (!worldState_.monsters.hasTable(dungeonIndex)) {
            return;
        }
        Monster monster;
        for (const SharedArray<int8_t> &byArray2 : worldState_.monsters.at(dungeonIndex)) {
            Monster *d2 = Monster::fromRecord(&monster, byArray2);
            if (d2->type_ >= 1 && d2->type_ <= 5) {
                byArray1[0] = (int8_t)(byArray1[0] + 1);
                continue;
            }
            if (d2->type_ >= 6 && d2->type_ <= 10) {
                byArray1[1] = (int8_t)(byArray1[1] + 1);
                continue;
            }
            if (d2->type_ >= 11 && d2->type_ <= 25) {
                byArray1[2] = (int8_t)(byArray1[2] + 1);
                continue;
            }
            if (d2->type_ >= 26 && d2->type_ <= 40) {
                byArray1[3] = (int8_t)(byArray1[3] + 1);
                continue;
            }
            byArray1[4] = (int8_t)(byArray1[4] + 1);
        }
        this->unloadAllMonsterImages();
        this->runImageLoaderFor(byArray1);
    }
    this->gameCanvas_->announceDungeon_ = true;
}

void Game::runImageLoaderFor(const SharedArray<int8_t> &byArray) {
    this->imgsLoaded_ = false;
    try {
        int32_t n1 = 0;
        while (n1 < 5) {
            if (byArray[n1] > 0) {
                int32_t n2 = profile_->monsterImageBands[n1][0];
                int32_t n3 = profile_->monsterImageBands[n1][1];
                int32_t n4 = 0;
                while (n4 < n3) {
                    int32_t n5 = n2 + n4;
                    bool excluded = false;
                    for (int32_t e = 0; e < profile_->excludedMonsterImageCount; ++e) {
                        if (profile_->excludedMonsterImages[e] == n5) excluded = true;
                    }
                    if (!excluded) {
                        GameCanvas::npcSprites_[n5] =
                            variant_->loadMonsterSprite(monsterFilenames_[n1][n4]);
                    }
                    if (!this->imgloadRunning_) {
                        return;
                    }
                    if (this->killThread_) {
                        this->killThread_ = false;
                        return;
                    }
                    ++n4;
                }
            }
            if (reloadGame_) {
                loadGameUI_->progressPercent_ = 80 + (n1 + 1) * 20 / 5;
                loadGameUI_->requestRepaint();
                loadGameUI_->flushRepaints();
            } else {
                this->loadDungeonUI_->progressPercent_ = (n1 + 1) * 100 / 5;
                this->loadDungeonUI_->requestRepaint();
                this->loadDungeonUI_->flushRepaints();
            }
            ++n1;
        }
        this->imgsLoaded_ = true;
        this->imgloadRunning_ = false;
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("ERROR: failed to load images: ") + exception.what());
        this->display_->setCurrent(this->errorForm_);
    } catch (...) {
        platform::writeLogLine("ERROR: unhandled exception in the image loader");
        this->display_->setCurrent(this->errorForm_);
    }
}

void Game::unloadAllMonsterImages() {
    int32_t n1 = GameCanvas::npcSprites_.length();
    int32_t n2 = 0;
    while (n2 < n1) {
        if (GameCanvas::npcSprites_[n2] != nullptr) {
            GameCanvas::npcSprites_[n2] = nullptr;
        }
        ++n2;
    }
}

void Game::removeMonsterAt(int32_t dungeonId, int32_t x, int32_t y) {
    SharedArray<int8_t> byArray1 =
        worldState_.monsters.removeAt((std::size_t)(dungeonId - 1), x, y);
    DungeonCore *i2 = dungeons_[(std::size_t)(dungeonId - 1)];
    if (!byArray1.isNull()) {
        i2->tiles_[x][y] = GameUtil::clearFlag((int8_t)2, i2->tiles_[x][y]);
    }
}

void Game::refreshAhead() {
    if (this->gameCanvas_ != nullptr) {
        this->gameCanvas_->refreshChestAhead();
        this->gameCanvas_->refreshNpcAhead();
    }
}

Monster *Game::combatTarget() {
    return this->gameCanvas_->combatMonster_;
}

void Game::showLevelUp() {
    this->LevelUpUI_ = this->newLevelUpUI(1);
    this->setCurrentDisplay(this->LevelUpUI_);
}

void Game::showEndOfGame() {
    variant_->showEndOfGame();
}

void Game::showOptions() {
    this->setCurrentDisplay(this->OptionsUI_);
}

UIWidget *Game::newInventoryItemUI(int32_t n) {
    UIWidget *itemScreen = makeOwnedUIWidget(5, uistate::SCREEN_INVENTORY_ITEM);
    std::string string1 = this->character_->describeItem(n);
    const bool equippable = this->character_->isEquippable(n);
    const auto actions = commandflow::itemRows(
        equippable, equippable && this->character_->isEquipped(n),
        this->character_->canLearn(n), this->character_->isUsable(n));
    itemScreen->itemActions_ = actions;
    SharedArray<std::string> stringArray1(actions.count);
    for (int32_t row = 0; row < actions.count; ++row) {
        stringArray1[row] = std::string(actions.rows[row].label);
    }
    itemScreen->setupForm(std::string("Item"), string1, stringArray1);
    itemScreen->wrapBody_ = true;
    itemScreen->backTarget_ = this->InventoryUI_;
    return itemScreen;
}

UIWidget *Game::newSkillsListUI() {
    UIWidget *skillsScreen = makeOwnedUIWidget(5, uistate::SCREEN_SKILLS_LIST);
    std::vector<std::string> skills = this->character_->skillList();
    int32_t n1 = (int32_t)skills.size();
    SharedArray<std::string> stringArray1(n1);
    int32_t n2 = 0;
    while (n2 < n1) {
        stringArray1[n2] = skills[(size_t)n2];
        ++n2;
    }
    skillsScreen->setupForm(std::string("Skills"), std::string("Your Skills:"), stringArray1);
    skillsScreen->backTarget_ = this->OptionsUI_;
    return skillsScreen;
}

UIWidget *Game::newSpellsListUI() {
    UIWidget *spellsScreen = makeOwnedUIWidget(5, uistate::SCREEN_SPELLS_LIST);
    std::vector<std::string> spells = this->character_->spellList();
    int32_t n1 = (int32_t)spells.size();
    SharedArray<std::string> stringArray1(n1);
    int32_t n2 = 0;
    while (n2 < n1) {
        stringArray1[n2] = spells[(size_t)n2];
        ++n2;
    }
    spellsScreen->setupForm(std::string("Spells"), std::string("Your Spells:"), stringArray1);
    spellsScreen->backTarget_ = this->OptionsUI_;
    return spellsScreen;
}

UIWidget *Game::newSpellInfoUI(int32_t n) {
    UIWidget *spellInfoScreen = makeOwnedUIWidget(5, uistate::SCREEN_SPELL_INFO);
    int32_t n1 = this->character_->spellAt(n);
    std::string string1 = this->character_->describeSpell(n1);
    SharedArray<std::string> stringArray1{"Ready Spell"};
    spellInfoScreen->setupForm(std::string("Spell Info"), string1, stringArray1);
    spellInfoScreen->wrapBody_ = true;
    spellInfoScreen->backTarget_ = this->SpellsListUI_;
    return spellInfoScreen;
}

UIWidget *Game::newLevelUpUI(int32_t n) {
    UIWidget *levelUpScreen = makeOwnedUIWidget(5, uistate::SCREEN_LEVEL_UP);
    SharedArray<std::string> stringArray1 = this->character_->levelUpChoices();
    const char *lead = profile_->levelUpPromptBreaks ? "Select an attribute to \nincrease "
                                                     : "Select an attribute to increase ";
    std::string string1;
    if (n == 1) {
        levelUpScreen->contextIndex_ = 0;
        string1 = std::string(lead) + "3 points:";
    } else if (n == 2) {
        string1 = std::string(lead) + "2 points:";
        levelUpScreen->contextIndex_ = 1;
    } else if (n == 3) {
        levelUpScreen->contextIndex_ = 2;
        string1 = std::string(lead) + "1 point:";
    }
    levelUpScreen->setupForm(std::string("Level Up"), string1, stringArray1);
    levelUpScreen->b(UIWidget::cmdCancel_);
    levelUpScreen->wrapBody_ = true;
    levelUpScreen->backTarget_ = levelUpScreen;
    return levelUpScreen;
}

void Game::showExitScreen() {
    UIWidget *exitUI = variant_->infoWidget(uistate::SCREEN_EXIT);
    exitUI->setupExitScreen();
    this->setCurrentDisplay(exitUI);
}

// uistate::describeScreen names the shared screens; the variant names its own.
std::string Game::describeScreenId(int32_t screenId) const {
    if (variant_ != nullptr) {
        if (const char *name = variant_->screenName(screenId)) {
            return std::string(name) + " (" + std::to_string(screenId) + ")";
        }
    }
    return uistate::describeScreen(screenId);
}

void Game::setCurrentDisplay(game::DisplayTarget target) {
    UIWidget *widget = target.widget();
    Displayable *displayable = target.displayable();
    // Every screen swap funnels through here, so this one line gives a bug
    // report the sequence of screens the player actually walked through.
    if (widget != nullptr) {
        if (currentUI_ == nullptr) {
            platform::writeLogLine("Screen: entering " + this->describeScreenId(widget->screenId_));
        } else if (currentUI_ != widget && currentUI_->screenId_ != widget->screenId_) {
            // A screen that re-shows itself (the level-up screen does this once
            // per point spent) is a different widget with the same id; logging
            // that repeat would drown the transitions that matter.
            platform::writeLogLine("Screen: " + this->describeScreenId(currentUI_->screenId_) +
                                   " -> " + this->describeScreenId(widget->screenId_));
        }
    } else if (displayable == this->gameCanvas_ && currentUI_ != nullptr) {
        platform::writeLogLine("Screen: leaving " + this->describeScreenId(currentUI_->screenId_) +
                               " for the world view");
    }
    try {
        if (currentUI_ != nullptr) {
            if (widget != nullptr) {
                if (currentUI_ != widget) {
                    currentUI_->onHide();
                }
            } else {
                currentUI_->onHide();
            }
        }
    } catch (const std::exception &exception) {
        platform::writeLogLine(std::string("ERROR: failed to hide the previous screen: ") +
                               exception.what());
    } catch (...) {
        platform::writeLogLine("ERROR: unhandled exception hiding the previous screen");
    }
    if (this->gameCanvas_ != nullptr) {
        this->gameCanvas_->pauseForUi();
    }
    if (this->display_ == nullptr) {
        this->display_ = &this->displayStorage_;
    }
    if (widget != nullptr) {
        currentUI_ = widget;
        this->uiCanvas_->widget_ = currentUI_;
        this->display_->setCurrent(this->uiCanvas_);
        currentUI_->onShow();
        currentUI_->requestRepaint();
        currentUI_->flushRepaints();
    } else if (displayable == this->gameCanvas_) {
        try {
            currentUI_ = nullptr;
            if (this->display_->getCurrent() == this->gameCanvas_) {
                this->gameCanvas_->showNotify();
                return;
            }
            this->display_->setCurrent(this->gameCanvas_);
        } catch (const std::exception &exception) {
            platform::writeLogLine(std::string("ERROR: failed to switch to the world view: ") +
                                   exception.what());
        } catch (...) {
            platform::writeLogLine("ERROR: unhandled exception switching to the world view");
        }
    } else if (displayable != nullptr) {
        currentUI_ = nullptr;
        this->display_->setCurrent(displayable);
    }
}

std::string Game::unusedSaveName() { return savegame::slotName(); }

std::optional<std::string> Game::lastGoodSaveName() {
    SharedArray<std::string> stringArray1 = SaveRecords::list(platformContext_);
    int32_t n1 = stringArray1.isNull() ? 0 : stringArray1.length();
    std::string name = savegame::slotName();
    for (int32_t n2 = 0; n2 < n1; ++n2) {
        if (name == stringArray1[n2]) {
            return name;
        }
    }
    return std::nullopt;
}

void Game::cleanupSaveRecords() {}

void Game::checkDestroyed() {
    if (applicationState_ == 4) {
        this->exit();
    }
}
