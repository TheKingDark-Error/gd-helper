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

// ============================================================
//  GD HELPER MOD v2 - GD 2.2081 Compatible
//  Features: FPS Counter | Advanced Auto Play Bot | Click Sounds
//  Mobile-friendly build via GitHub Actions
// ============================================================

constexpr int FPS_COUNTER_TAG = 0x4845;

// ==================== CLICK SOUND MANAGER ====================
class ClickSoundManager {
public:
    bool m_enabled = false;

    void updateState() {
        m_enabled = Mod::get()->getSettingValue<bool>("click-sound-enabled");
    }

    void playClick() {
        if (!m_enabled) return;
        FMODAudioEngine::sharedEngine()->playEffect("uiClick.ogg", 1.0f, 0.0f, 0.8f);
    }

    void playRelease() {
        if (!m_enabled) return;
        FMODAudioEngine::sharedEngine()->playEffect("quitSound_01.ogg", 1.0f, 0.0f, 0.6f);
    }
};

static ClickSoundManager g_clickSound;

// ==================== BOT ====================
enum class GameMode { Cube, Ship, Wave, UFO, Robot, Spider, Swing };
enum class Speed { Half, Normal, Double, Triple, Quadruple };

struct BotConfig {
    float cubeLookAhead = 4.0f;
    float shipLookAhead = 6.0f;
    float waveLookAhead = 3.0f;
    float ufoLookAhead  = 4.0f;
    float cubeTrigger   = 2.0f;
    float shipTrigger   = 3.5f;
    float waveTrigger   = 1.5f;
    float ufoTrigger    = 2.0f;
};

struct ScannedObj {
    float dist;
    int id;
    float y;
    bool hazard = false;
    bool orb = false;
    bool pad = false;
    bool portal = false;
};

class AdvancedBot {
public:
    bool m_enabled = false;
    bool m_holding = false;
    PlayLayer* m_layer = nullptr;
    BotConfig m_cfg;
    GameMode m_mode = GameMode::Cube;
    Speed m_speed = Speed::Normal;
    bool m_gravity = false;
    bool m_mini = false;
    float m_px = 0, m_py = 0;

    void updateState() { m_enabled = Mod::get()->getSettingValue<bool>("auto-play-enabled"); }
    void setLayer(PlayLayer* l) { m_layer = l; }

    void reset() {
        m_holding = false;
        m_layer = nullptr;
        m_mode = GameMode::Cube;
        m_speed = Speed::Normal;
        m_gravity = false;
        m_mini = false;
    }

    void detectMode() {
        if (!m_layer) return;
        auto p = m_layer->m_player1;
        if (!p) return;
        if (p->m_isShip) m_mode = GameMode::Ship;
        else if (p->m_isBird) m_mode = GameMode::UFO;
        else if (p->m_isDart) m_mode = GameMode::Wave;
        else if (p->m_isRobot) m_mode = GameMode::Robot;
        else if (p->m_isSpider) m_mode = GameMode::Spider;
        else if (p->m_isSwing) m_mode = GameMode::Swing;
        else m_mode = GameMode::Cube;
        m_gravity = p->m_isUpsideDown;
        m_mini = p->m_isMini;
    }

    float speedMul() {
        switch (m_speed) {
            case Speed::Half: return 0.5f;
            case Speed::Double: return 1.5f;
            case Speed::Triple: return 2.0f;
            case Speed::Quadruple: return 2.5f;
            default: return 1.0f;
        }
    }

    std::vector<ScannedObj> scan(float lookAheadBlocks) {
        std::vector<ScannedObj> out;
        if (!m_layer) return out;
        auto p = m_layer->m_player1;
        if (!p) return out;

        m_px = p->getPositionX();
        m_py = p->getPositionY();
        float maxDist = lookAheadBlocks * 30.0f * speedMul();

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

            // Hazards
            if ((oid >= 8 && oid <= 11) ||
                (oid >= 36 && oid <= 37) ||
                (oid >= 85 && oid <= 87) ||
                (oid >= 183 && oid <= 184) ||
                (oid >= 186 && oid <= 187) ||
                (oid >= 198 && oid <= 199) ||
                oid == 89 || oid == 98 || oid == 99 || oid == 101 ||
                oid == 102 || oid == 103 || oid == 1330 || oid == 1331) {
                so.hazard = true;
            }
            // Orbs
            if (oid == 36 || oid == 39 || oid == 103 || oid == 1330 ||
                oid == 140 || oid == 141 || oid == 142 || oid == 1329 ||
                oid == 143 || oid == 1332 || oid == 1333 || oid == 1334 || oid == 1594) {
                so.orb = true;
            }
            // Pads
            if (oid == 50 || oid == 61 || oid == 71 || oid == 161) {
                so.pad = true;
            }
            // Portals (speed, gamemode, gravity, size, dual)
            if ((oid >= 12 && oid <= 15) || oid == 47 || oid == 111 ||
                oid == 660 || oid == 745 || oid == 1331 || oid == 1933 ||
                oid == 10 || oid == 11 || oid == 292 || oid == 293 ||
                oid == 200 || oid == 201 || oid == 202 || oid == 203 || oid == 1334 ||
                oid == 101 || oid == 102 || oid == 286 || oid == 287) {
                so.portal = true;
            }

            if (so.hazard || so.orb || so.pad || so.portal)
                out.push_back(so);
        }

        std::sort(out.begin(), out.end(), [](const ScannedObj& a, const ScannedObj& b) {
            return a.dist < b.dist;
        });
        return out;
    }

    void updateSpeed(const std::vector<ScannedObj>& objs) {
        for (const auto& o : objs) {
            if (!o.portal) continue;
            switch (o.id) {
                case 200: m_speed = Speed::Half; break;
                case 201: m_speed = Speed::Normal; break;
                case 202: m_speed = Speed::Double; break;
                case 203: m_speed = Speed::Triple; break;
                case 1334: m_speed = Speed::Quadruple; break;
            }
        }
    }

    bool thinkCube(const std::vector<ScannedObj>& objs) {
        float t = m_cfg.cubeTrigger * 30.0f * speedMul();
        for (const auto& o : objs) {
            if (o.dist > t) continue;
            if (o.hazard && std::abs(o.y - m_py) < 45.0f) return true;
            if (o.orb && o.dist < t * 0.8f) return true;
            if (o.pad && o.dist < t * 0.9f) return true;
        }
        return false;
    }

    bool thinkShip(const std::vector<ScannedObj>& objs) {
        float t = m_cfg.shipTrigger * 30.0f * speedMul();
        int above = 0, below = 0;
        float closestAbove = 9999, closestBelow = 9999;

        for (const auto& o : objs) {
            if (o.dist > t || (!o.hazard && !o.portal)) continue;
            float dy = o.y - m_py;
            if (dy > 25.0f) { above++; closestAbove = std::min(closestAbove, o.dist); }
            else if (dy < -25.0f) { below++; closestBelow = std::min(closestBelow, o.dist); }
        }

        if (above > 0 && below == 0) return false;
        if (below > 0 && above == 0) return true;
        if (above > 0 && below > 0) {
            return closestAbove > closestBelow;
        }
        return m_py < 150.0f;
    }

    bool thinkWave(const std::vector<ScannedObj>& objs) {
        float t = m_cfg.waveTrigger * 30.0f * speedMul();
        for (const auto& o : objs) {
            if (o.dist > t * 2.0f || !o.hazard) continue;
            float dy = o.y - m_py;
            if (std::abs(dy) < 30.0f) {
                return !m_holding; // switch direction
            }
            if (dy > 40.0f && m_holding) return true;
            if (dy < -40.0f && !m_holding) return false;
        }
        int cycle = static_cast<int>(m_px / 90.0f) % 2;
        return cycle == 0;
    }

    bool thinkUFO(const std::vector<ScannedObj>& objs) {
        float t = m_cfg.ufoTrigger * 30.0f * speedMul();
        for (const auto& o : objs) {
            if (o.dist > t) continue;
            if (o.hazard && std::abs(o.y - m_py) < 50.0f) return true;
            if (o.orb || o.pad) return true;
        }
        return false;
    }

    void update(float dt) {
        if (!m_enabled || !m_layer) return;
        auto p = m_layer->m_player1;
        if (!p) return;

        detectMode();
        float la = m_cfg.cubeLookAhead;
        switch (m_mode) {
            case GameMode::Ship: la = m_cfg.shipLookAhead; break;
            case GameMode::Wave: la = m_cfg.waveLookAhead; break;
            case GameMode::UFO: la = m_cfg.ufoLookAhead; break;
            default: break;
        }

        auto objs = scan(la);
        updateSpeed(objs);

        bool click = false;
        switch (m_mode) {
            case GameMode::Cube:
            case GameMode::Robot:
            case GameMode::Spider:
                click = thinkCube(objs); break;
            case GameMode::Ship:
                click = thinkShip(objs); break;
            case GameMode::Wave:
                click = thinkWave(objs); break;
            case GameMode::UFO:
                click = thinkUFO(objs); break;
            default:
                click = thinkCube(objs); break;
        }

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
        g_clickSound.updateState();
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        g_bot.updateState();
        g_clickSound.updateState();
        if (g_bot.m_enabled) g_bot.update(dt);
    }

    void onQuit() {
        g_bot.reset();
        PlayLayer::onQuit();
    }

    void pushButton(int p0) {
        g_clickSound.updateState();
        if (g_clickSound.m_enabled) g_clickSound.playClick();
        PlayLayer::pushButton(p0);
    }

    void releaseButton(int p0) {
        g_clickSound.updateState();
        if (g_clickSound.m_enabled) g_clickSound.playRelease();
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
                   menu_selector(MyPauseLayer::onToggleFPS), 35);
        makeToggle("Auto Play", Mod::get()->getSettingValue<bool>("auto-play-enabled"),
                   menu_selector(MyPauseLayer::onToggleBot), 5);
        makeToggle("Click Sfx", Mod::get()->getSettingValue<bool>("click-sound-enabled"),
                   menu_selector(MyPauseLayer::onToggleSound), -25);

        auto title = CCLabelBMFont::create("Helper", "bigFont.fnt");
        if (title) {
            title->setScale(0.45f);
            title->setPosition({30, 60});
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
};
