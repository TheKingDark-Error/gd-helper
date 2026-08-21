#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include <vector>
#include <cmath>
#include <algorithm>

using namespace geode::prelude;

constexpr int FPS_COUNTER_TAG = 0x4845;
constexpr int FRAME_STEPPER_TAG = 0x4846;
constexpr int SPEED_HACK_TAG = 0x4847;

// ==================== CLICK SOUND ====================
class ClickSoundManager {
public:
    bool m_enabled = false;
    void updateState() {
        m_enabled = Mod::get()->getSettingValue<bool>("click-sound-enabled");
    }
    void playClick() {
        if (!m_enabled) return;
        auto engine = FMODAudioEngine::sharedEngine();
        if (engine) engine->playEffect("uiClick.ogg", 1.0f, 0.0f, 0.8f);
    }
    void playRelease() {
        if (!m_enabled) return;
        auto engine = FMODAudioEngine::sharedEngine();
        if (engine) engine->playEffect("quitSound_01.ogg", 1.0f, 0.0f, 0.6f);
    }
};

static ClickSoundManager g_clickSound;

// ==================== MACRO SYSTEM ====================
struct MacroAction {
    float time;
    bool isClick;
    int player;
};

class MacroManager {
public:
    bool m_recording = false;
    bool m_playing = false;
    std::vector<MacroAction> m_actions;
    float m_startTime = 0.0f;
    float m_playTime = 0.0f;
    size_t m_playIndex = 0;
    PlayLayer* m_layer = nullptr;

    void startRecord(PlayLayer* layer) {
        m_layer = layer;
        m_recording = true;
        m_playing = false;
        m_actions.clear();
        m_startTime = 0.0f;
    }

    void stopRecord() {
        m_recording = false;
    }

    void startPlay(PlayLayer* layer) {
        if (m_actions.empty()) return;
        m_layer = layer;
        m_playing = true;
        m_recording = false;
        m_playTime = 0.0f;
        m_playIndex = 0;
    }

    void stopPlay() {
        m_playing = false;
    }

    void recordAction(bool isClick, int player) {
        if (!m_recording) return;
        m_actions.push_back({m_startTime, isClick, player});
    }

    void update(float dt) {
        if (m_recording) {
            m_startTime += dt;
        }
        if (m_playing && m_layer) {
            m_playTime += dt;
            while (m_playIndex < m_actions.size() && m_actions[m_playIndex].time <= m_playTime) {
                auto& a = m_actions[m_playIndex];
                if (a.isClick) {
                    m_layer->handleButton(true, 1, a.player == 1);
                    g_clickSound.playClick();
                } else {
                    m_layer->handleButton(false, 1, a.player == 1);
                    g_clickSound.playRelease();
                }
                m_playIndex++;
            }
            if (m_playIndex >= m_actions.size()) {
                m_playing = false;
            }
        }
    }

    void clear() {
        m_actions.clear();
        m_recording = false;
        m_playing = false;
    }
};

static MacroManager g_macro;

// ==================== FRAME STEPPER ====================
class FrameStepperManager {
public:
    bool m_enabled = false;
    bool m_paused = false;
    PlayLayer* m_layer = nullptr;
    CCMenu* m_menu = nullptr;

    void updateState() {
        m_enabled = Mod::get()->getSettingValue<bool>("frame-stepper-enabled");
    }

    void setLayer(PlayLayer* layer) {
        m_layer = layer;
        if (m_enabled) createUI();
        else removeUI();
    }

    void removeUI() {
        if (m_menu) {
            m_menu->removeFromParentAndCleanup(true);
            m_menu = nullptr;
        }
    }

    void createUI() {
        if (!m_layer || m_menu) return;
        auto ws = CCDirector::sharedDirector()->getWinSize();
        m_menu = CCMenu::create();
        m_menu->setPosition({ws.width - 60, ws.height - 40});
        m_menu->setTag(FRAME_STEPPER_TAG);

        // Step 1 frame button (green play)
        auto playSpr = CCSprite::createWithSpriteFrameName("GJ_playBtn_001.png");
        auto playBtn = CCMenuItemSpriteExtra::create(playSpr, playSpr, this, menu_selector(FrameStepperManager::onStep));
        playBtn->setScale(0.5f);
        playBtn->setPosition({0, 0});
        m_menu->addChild(playBtn);

        // Step 5 frames button (red play)
        auto fastSpr = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
        auto fastBtn = CCMenuItemSpriteExtra::create(fastSpr, fastSpr, this, menu_selector(FrameStepperManager::onFastStep));
        fastBtn->setScale(0.5f);
        fastBtn->setPosition({0, -35});
        fastBtn->setVisible(false);
        m_menu->addChild(fastBtn);

        // Close button (X)
        auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        auto closeBtn = CCMenuItemSpriteExtra::create(closeSpr, closeSpr, this, menu_selector(FrameStepperManager::onClose));
        closeBtn->setScale(0.4f);
        closeBtn->setPosition({35, 0});
        closeBtn->setVisible(false);
        m_menu->addChild(closeBtn);

        m_layer->addChild(m_menu, 10000);
    }

    void onStep(CCObject*) {
        if (!m_layer) return;
        if (!m_paused) {
            m_paused = true;
            for (auto child : CCArrayExt<CCNode*>(m_menu->getChildren())) {
                auto item = typeinfo_cast<CCMenuItemSpriteExtra*>(child);
                if (!item) continue;
                auto pos = item->getPosition();
                if (pos.y < -20) item->setVisible(true);
                else if (pos.x > 30) item->setVisible(true);
                else item->setVisible(false);
            }
        } else {
            m_layer->stepUpdate();
        }
    }

    void onFastStep(CCObject*) {
        if (!m_layer || !m_paused) return;
        for (int i = 0; i < 5; i++) {
            m_layer->stepUpdate();
        }
    }

    void onClose(CCObject*) {
        m_paused = false;
        for (auto child : CCArrayExt<CCNode*>(m_menu->getChildren())) {
            auto item = typeinfo_cast<CCMenuItemSpriteExtra*>(child);
            if (!item) continue;
            auto pos = item->getPosition();
            if (pos.y < -20) item->setVisible(false);
            else if (pos.x > 30) item->setVisible(false);
            else item->setVisible(true);
        }
    }

    void reset() {
        m_paused = false;
        removeUI();
        m_layer = nullptr;
    }
};

static FrameStepperManager g_frameStepper;

// ==================== SPEED HACK ====================
class SpeedHackManager {
public:
    bool m_enabled = false;
    float m_speed = 1.0f;
    bool m_active = false;
    CCMenuItemSpriteExtra* m_clockBtn = nullptr;

    void updateState() {
        m_enabled = Mod::get()->getSettingValue<bool>("speed-hack-enabled");
        m_speed = Mod::get()->getSettingValue<float>("speed-hack-value");
    }

    void createUI(PlayLayer* layer) {
        if (!layer || m_clockBtn) return;
        auto ws = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({ws.width - 40, ws.height - 100});
        menu->setTag(SPEED_HACK_TAG);

        auto clockSpr = CCSprite::createWithSpriteFrameName("GJ_timeIcon_001.png");
        m_clockBtn = CCMenuItemSpriteExtra::create(clockSpr, clockSpr, this, menu_selector(SpeedHackManager::onToggle));
        m_clockBtn->setScale(0.6f);
        menu->addChild(m_clockBtn);
        layer->addChild(menu, 10000);
        updateColor();
    }

    void removeUI() {
        if (m_clockBtn && m_clockBtn->getParent()) {
            m_clockBtn->getParent()->removeFromParentAndCleanup(true);
            m_clockBtn = nullptr;
        }
    }

    void updateColor() {
        if (!m_clockBtn) return;
        auto spr = m_clockBtn->getNormalImage();
        if (spr) {
            if (m_active) spr->setColor({0, 255, 0});
            else spr->setColor({255, 0, 0});
        }
    }

    void onToggle(CCObject*) {
        m_active = !m_active;
        updateColor();
        applySpeed();
    }

    void applySpeed() {
        auto scheduler = CCDirector::sharedDirector()->getScheduler();
        if (scheduler) {
            scheduler->setTimeScale(m_active ? m_speed : 1.0f);
        }
    }

    void reset() {
        m_active = false;
        applySpeed();
        removeUI();
    }
};

static SpeedHackManager g_speedHack;

// ==================== FPS COUNTER ====================
class FPSCounter : public CCNode {
public:
    CCLabelBMFont* m_label = nullptr;

    static FPSCounter* create() {
        auto ret = new FPSCounter();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init() override {
        if (!CCNode::init()) return false;
        m_label = CCLabelBMFont::create("FPS: 0", "bigFont.fnt");
        if (!m_label) return false;
        m_label->setScale(0.4f);
        m_label->setAnchorPoint({0, 1});
        m_label->setColor({255, 255, 255});
        m_label->setOpacity(200);
        addChild(m_label);
        scheduleUpdate();
        return true;
    }

    void update(float dt) override {
        CCNode::update(dt);
        if (!m_label) return;
        auto d = CCDirector::sharedDirector();
        float spf = d->getSecondsPerFrame();
        int fps = (spf > 0.0f) ? static_cast<int>(1.0f / spf + 0.5f) : 0;
        m_label->setCString(fmt::format("FPS: {}", fps).c_str());
        auto ws = d->getWinSize();
        setPosition({10, ws.height - 10});
    }
};

// ==================== BOT ====================
struct ScannedObj {
    float dist;
    int id;
    float y;
    bool hazard = false;
    bool orb = false;
    bool pad = false;
};

class AdvancedBot {
public:
    bool m_enabled = false;
    bool m_holding = false;
    PlayLayer* m_layer = nullptr;
    float m_px = 0, m_py = 0;

    void updateState() { m_enabled = Mod::get()->getSettingValue<bool>("auto-play-enabled"); }
    void setLayer(PlayLayer* l) { m_layer = l; }

    void reset() {
        m_holding = false;
        m_layer = nullptr;
    }

    std::vector<ScannedObj> scan(float lookAheadBlocks) {
        std::vector<ScannedObj> out;
        if (!m_layer) return out;
        auto p = m_layer->m_player1;
        if (!p) return out;

        m_px = p->getPositionX();
        m_py = p->getPositionY();
        float maxDist = lookAheadBlocks * 30.0f;

        auto objs = m_layer->m_objects;
        if (!objs) return out;

        CCObject* obj;
        CCARRAY_FOREACH(objs, obj) {
            auto go = typeinfo_cast<GameObject*>(obj);
            if (!go) continue;
            float dx = go->getPositionX() - m_px;
            if (dx < 2.0f || dx > maxDist) continue;

            int oid = go->m_objectID;
            ScannedObj so;
            so.dist = dx;
            so.id = oid;
            so.y = go->getPositionY();

            if ((oid >= 8 && oid <= 11) ||
                (oid >= 36 && oid <= 37) ||
                (oid >= 85 && oid <= 87) ||
                (oid >= 183 && oid <= 184) ||
                (oid >= 186 && oid <= 187) ||
                (oid >= 198 && oid <= 199)) {
                so.hazard = true;
            }
            if (oid == 36 || oid == 39 || oid == 103 || oid == 1330 ||
                oid == 140 || oid == 141 || oid == 142 || oid == 1329 ||
                oid == 143 || oid == 1332 || oid == 1333 || oid == 1334 || oid == 1594) {
                so.orb = true;
            }
            if (oid == 50 || oid == 61 || oid == 71 || oid == 161) {
                so.pad = true;
            }

            if (so.hazard || so.orb || so.pad)
                out.push_back(so);
        }

        std::sort(out.begin(), out.end(), [](const ScannedObj& a, const ScannedObj& b) {
            return a.dist < b.dist;
        });
        return out;
    }

    bool thinkCube(const std::vector<ScannedObj>& objs) {
        float t = 2.0f * 30.0f;
        for (const auto& o : objs) {
            if (o.dist > t) continue;
            if (o.hazard && std::abs(o.y - m_py) < 45.0f) return true;
            if (o.orb && o.dist < t * 0.8f) return true;
            if (o.pad && o.dist < t * 0.9f) return true;
        }
        return false;
    }

    void update(float dt) {
        if (!m_enabled || !m_layer) return;
        auto p = m_layer->m_player1;
        if (!p) return;

        auto objs = scan(4.0f);
        bool click = thinkCube(objs);

        if (click && !m_holding) {
            m_layer->handleButton(true, 1, true);
            g_clickSound.playClick();
            m_holding = true;
        } else if (!click && m_holding) {
            m_layer->handleButton(false, 1, true);
            g_clickSound.playRelease();
            m_holding = false;
        }
    }
};

static AdvancedBot g_bot;

// ==================== PLAYLAYER ====================
class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        FPSCounter* fps = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        if (Mod::get()->getSettingValue<bool>("fps-enabled")) {
            m_fields->fps = FPSCounter::create();
            if (m_fields->fps) {
                m_fields->fps->setTag(FPS_COUNTER_TAG);
                addChild(m_fields->fps, 9999);
            }
        }

        g_bot.reset();
        g_bot.setLayer(this);
        g_bot.updateState();

        g_frameStepper.reset();
        g_frameStepper.setLayer(this);

        g_speedHack.reset();
        g_speedHack.createUI(this);

        g_clickSound.updateState();
        g_macro.clear();

        return true;
    }

    void update(float dt) {
        if (g_frameStepper.m_paused) return;
        PlayLayer::update(dt);

        g_bot.updateState();
        g_clickSound.updateState();
        g_frameStepper.updateState();
        g_speedHack.updateState();

        if (g_bot.m_enabled) g_bot.update(dt);
        g_macro.update(dt);
    }

    void onQuit() {
        g_bot.reset();
        g_frameStepper.reset();
        g_speedHack.reset();
        g_macro.stopRecord();
        g_macro.stopPlay();
        PlayLayer::onQuit();
    }

    void pushButton(int p0) {
        g_clickSound.updateState();
        if (g_clickSound.m_enabled) g_clickSound.playClick();
        if (g_macro.m_recording) g_macro.recordAction(true, p0);
        PlayLayer::pushButton(p0);
    }

    void releaseButton(int p0) {
        g_clickSound.updateState();
        if (g_clickSound.m_enabled) g_clickSound.playRelease();
        if (g_macro.m_recording) g_macro.recordAction(false, p0);
        PlayLayer::releaseButton(p0);
    }

    void levelComplete() {
        g_bot.updateState();
        if (g_bot.m_enabled) {
            FLAlertLayer::create(
                "Auto Play Active",
                "Level completed in Auto Play mode.\n"
                "Progress was <cr>NOT saved</c>!",
                "OK"
            )->show();
            return;
        }
        PlayLayer::levelComplete();
    }
};

// ==================== PAUSE LAYER ====================
class $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto ws = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({55, ws.height - 90});
        menu->setID("helper-settings-menu");

        auto makeToggle = [&](const char* text, bool on, SEL_MenuHandler cb, float y) {
            auto off = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
            auto on = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
            if (!off || !on) return;
            auto t = CCMenuItemToggler::create(off, on, this, cb);
            t->setScale(0.6f);
            t->toggle(on);
            t->setPosition({0, y});
            auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
            lbl->setScale(0.35f);
            lbl->setAnchorPoint({0, 0.5f});
            lbl->setPosition({20, y});
            menu->addChild(t);
            menu->addChild(lbl);
        };

        makeToggle("FPS", Mod::get()->getSettingValue<bool>("fps-enabled"),
                   menu_selector(MyPauseLayer::onToggleFPS), 65);
        makeToggle("Auto Play", Mod::get()->getSettingValue<bool>("auto-play-enabled"),
                   menu_selector(MyPauseLayer::onToggleBot), 35);
        makeToggle("Click Sfx", Mod::get()->getSettingValue<bool>("click-sound-enabled"),
                   menu_selector(MyPauseLayer::onToggleSound), 5);
        makeToggle("Frame Step", Mod::get()->getSettingValue<bool>("frame-stepper-enabled"),
                   menu_selector(MyPauseLayer::onToggleFrameStep), -25);
        makeToggle("Speed Hack", Mod::get()->getSettingValue<bool>("speed-hack-enabled"),
                   menu_selector(MyPauseLayer::onToggleSpeed), -55);

        auto recordLbl = CCLabelBMFont::create("[Record]", "bigFont.fnt");
        recordLbl->setScale(0.3f);
        auto recordBtn = CCMenuItemLabel::create(recordLbl, this, menu_selector(MyPauseLayer::onMacroRecord));
        recordBtn->setPosition({80, 35});
        menu->addChild(recordBtn);

        auto playLbl = CCLabelBMFont::create("[Play]", "bigFont.fnt");
        playLbl->setScale(0.3f);
        auto playBtn = CCMenuItemLabel::create(playLbl, this, menu_selector(MyPauseLayer::onMacroPlay));
        playBtn->setPosition({80, 5});
        menu->addChild(playBtn);

        auto title = CCLabelBMFont::create("Helper", "bigFont.fnt");
        if (title) {
            title->setScale(0.45f);
            title->setPosition({30, 90});
            menu->addChild(title);
        }
        addChild(menu, 100);
    }

    void onToggleFPS(CCObject* s) {
        auto t = static_cast<CCMenuItemToggler*>(s);
        bool v = !t->isOn();
        Mod::get()->setSettingValue<bool>("fps-enabled", v);
        if (auto gm = GameManager::sharedState()) {
            if (auto pl = gm->m_playLayer) {
                if (auto n = pl->getChildByTag(FPS_COUNTER_TAG)) n->setVisible(v);
            }
        }
    }

    void onToggleBot(CCObject* s) {
        auto t = static_cast<CCMenuItemToggler*>(s);
        Mod::get()->setSettingValue<bool>("auto-play-enabled", !t->isOn());
        g_bot.updateState();
    }

    void onToggleSound(CCObject* s) {
        auto t = static_cast<CCMenuItemToggler*>(s);
        Mod::get()->setSettingValue<bool>("click-sound-enabled", !t->isOn());
        g_clickSound.updateState();
    }

    void onToggleFrameStep(CCObject* s) {
        auto t = static_cast<CCMenuItemToggler*>(s);
        bool v = !t->isOn();
        Mod::get()->setSettingValue<bool>("frame-stepper-enabled", v);
        g_frameStepper.updateState();
        if (auto gm = GameManager::sharedState()) {
            if (auto pl = gm->m_playLayer) {
                if (v) g_frameStepper.setLayer(pl);
                else g_frameStepper.reset();
            }
        }
    }

    void onToggleSpeed(CCObject* s) {
        auto t = static_cast<CCMenuItemToggler*>(s);
        bool v = !t->isOn();
        Mod::get()->setSettingValue<bool>("speed-hack-enabled", v);
        g_speedHack.updateState();
    }

    void onMacroRecord(CCObject*) {
        if (auto gm = GameManager::sharedState()) {
            if (auto pl = gm->m_playLayer) {
                if (g_macro.m_recording) {
                    g_macro.stopRecord();
                    FLAlertLayer::create("Macro", "Recording stopped.", "OK")->show();
                } else {
                    g_macro.startRecord(pl);
                    FLAlertLayer::create("Macro", "Recording started!", "OK")->show();
                }
            }
        }
    }

    void onMacroPlay(CCObject*) {
        if (auto gm = GameManager::sharedState()) {
            if (auto pl = gm->m_playLayer) {
                if (g_macro.m_playing) {
                    g_macro.stopPlay();
                } else {
                    g_macro.startPlay(pl);
                }
            }
        }
    }
};
