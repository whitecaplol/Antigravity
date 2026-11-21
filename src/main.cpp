#include "context.hpp"

void collectRemainingDescendants(CCNode* root, std::vector<CCNode*>* visited) {
    if (!root || isAnyOf<GJListLayer*, PlayLayer*, LevelCell*>(root) || isRelaxedAnyOf<EditorWindowPopup*>(root)) return;

    if (isAnyOf<CCLayer*>(root) && (!isRelaxedAnyOf<CCLayer*>(root->getParent()) || root->getParent()->getChildrenCount() == 1)) {
        visited->push_back(root);
    }

    for (auto child : root->getChildrenExt()) {
        collectRemainingDescendants(child, visited);
    }
}

void collectChildrenForContexts(CCNode* root, std::vector<CCNode*>* visited) {
    if (!root || root->getChildByType<LoadingLayer*>()) return;
    for (auto child : root->getChildrenExt()) {
        if (isRelaxedAnyOf<EditorWindowPopup*>(child)) continue;
        visited->push_back(child);
        if (!isAnyOf<LevelEditorLayer*>(child)) {
            collectRemainingDescendants(child, visited);
        }
    }
}

// This is cursed (pt 1)
#include <Geode/modify/CCTextInputNode.hpp>
class $modify(CCTextInputNode) {
    bool ccTouchBegan(CCTouch* touch, CCEvent* event) {
        auto size = getContentSize();
        auto anchorPointOffset = isIgnoreAnchorPointForPosition() ? CCPointZero : m_textField->getAnchorPoint();
        auto point = convertTouchToNodeSpace(touch) + ((m_textField->getAnchorPoint() - anchorPointOffset) * size);
        auto result = 0 <= point.x && point.x <= size.width && 0 <= point.y && point.y <= size.height;
        if (result) {
            onClickTrackNode(true);
            updateCursorPosition(touch->getLocation(), {{0, 0}, size});
        } else {
            onClickTrackNode(false);
        }

        return result;
    }
};

// This is cursed (pt 2)
class TouchLayer : public CCLayer {
private:
    b2Vec2 initialPosition = b2Vec2_zero;
    b2Vec2 position = b2Vec2_zero;
    WeakRef<CCDirector> director = nullptr;
    bool isActive = false;
    bool firstQuerySinceNewActive = false;
    bool shouldDisableOnTouchEnd = false;
protected:
    bool init(const CCSize& winSize, CCDirector* director) {
        if (!CCLayer::init()) return false;

        this->director = director;
        setTouchMode(kCCTouchesOneByOne);
        setTouchPriority(-(1 << 10));
        setTouchEnabled(true);
        setContentSize(winSize);
        setZOrder(1 << 15);
        return true;
    }
public:
    static TouchLayer* create(const CCSize& winSize, CCDirector* director) {
        auto ret = new TouchLayer();
        if (ret->init(winSize, director)) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }

    void registerWithTouchDispatcher() override {
        if (!director.valid()) return;
        director.lock()->sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, getTouchPriority(), false);
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        firstQuerySinceNewActive = true;
        shouldDisableOnTouchEnd = false;
        isActive = true;
        position = b2Vec2{touch->getLocation().x, touch->getLocation().y};
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        position = b2Vec2{touch->getLocation().x, touch->getLocation().y};
        if (!firstQuerySinceNewActive && b2Length(initialPosition - position) > 2.0f) {
            shouldDisableOnTouchEnd = true;
        }
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        position = b2Vec2_zero;
        initialPosition = b2Vec2_zero;
        isActive = false;
        if (shouldDisableOnTouchEnd && director.valid()) {
            CCSet set; // I love touchesCancelled
            set.addObject(touch);
            director.lock()->getTouchDispatcher()->touchesCancelled(&set, event);
        };
        shouldDisableOnTouchEnd = false;
    }

    void ccTouchCancelled(CCTouch* touch, CCEvent* event) override {
        position = b2Vec2_zero;
        initialPosition = b2Vec2_zero;
        isActive = false;
        shouldDisableOnTouchEnd = false;
    }

    bool isFirstQuerySinceNewActive() { // "New All" ahh naming
        return firstQuerySinceNewActive;
    }

    bool isTouchActive() {
        return isActive;
    }

    b2Vec2 getTouch() {
        if (firstQuerySinceNewActive) {
            initialPosition = position;
        }
        firstQuerySinceNewActive = false;
        return position;
    }
};

// Because CCDirectors do not support fields
std::unordered_map<CCNode*, AntigravityContext*> activeLayers;
TouchLayer* touchLayer = nullptr;
bool resetQueued = false;
bool pokeQueued = false;
bool pauseAntigravity = false;
bool doILoveGDCologne = false;

#include <Geode/modify/CCDirector.hpp>
class $modify(CCDirector) {
    void drawScene() {
        auto winSize = getWinSize();
        auto mod = Mod::get();

        if (!touchLayer && mod->getSettingValue<bool>("enabled")) {
            touchLayer = TouchLayer::create(winSize, this);
            touchLayer->retain();
        }

        auto scene = getRunningScene();

        if (isRelaxedAnyOf<CCTransitionScene*>(scene)) {
            CCDirector::drawScene();
            touchLayer->setTouchEnabled(false);
            touchLayer->setParent(nullptr);
            return;
        }

        if (touchLayer->getParent() != scene) {
            touchLayer->onExit();
            touchLayer->setParent(scene);
            touchLayer->onEnter();
        }

        if (pauseAntigravity && !resetQueued) {
            CCDirector::drawScene();
            return;
        }

        std::vector<CCNode*> contextNodes;
        collectChildrenForContexts(scene, &contextNodes);

        std::unordered_set<CCNode*> current(contextNodes.begin(), contextNodes.end());

        std::erase_if(activeLayers, [&current](const auto& pair) {
            if (current.contains(pair.first)) return false;
            delete activeLayers.at(pair.first);
            return true;
        });

        auto canDrag = mod->getSettingValue<bool>("enable-drag");
        auto currentGravityX = mod->getSettingValue<float>("gravity-x");
        auto currentGravityY = mod->getSettingValue<float>("gravity-y");
        auto glueFonts = mod->getSettingValue<bool>("glue-fonts");
        auto dt = fmin(getDeltaTime(), 1.0f / 30);
        auto retoggleDragging = true;

        if (mod->getSettingValue<bool>("enabled") && !resetQueued) {
            for (auto child : contextNodes) {
                if (auto playLayer = typeinfo_cast<PlayLayer*>(child)) {
                    retoggleDragging = false;
                    touchLayer->setTouchEnabled(!(playLayer->isGameplayActive() && !playLayer->m_isPaused));
                    continue;
                }
                if (!mod->getSettingValue<bool>("enable-in-editor") && isRelaxedAnyOf<LevelEditorLayer*>(child)) continue;
                if (activeLayers.contains(child)) {
                    auto context = activeLayers[child];
                    context->setGravity(currentGravityX, currentGravityY);
                    context->setGlueFonts(glueFonts);
                    if (canDrag && child == contextNodes.back()) {
                        if (touchLayer->isTouchActive()) {
                            if (touchLayer->isFirstQuerySinceNewActive()) {
                                context->beginDrag(touchLayer->getTouch());
                            } else {
                                context->moveDrag(touchLayer->getTouch());
                            }
                        } else {
                            context->endDrag();
                        }
                    } else {
                        context->endDrag();
                    }
                    if (doILoveGDCologne) {
                        auto bumpscosity = Loader::get()->getLoadedMod("colon.bumpscosity")->getSavedValue<float>("bumpscosity", 0); // i.e. Restitution, but that's boring
                        context->updateBumpscosity(bumpscosity);
                    }
                    auto success = context->step(dt, &current, pokeQueued);
                    if (!success) {
                        delete activeLayers.at(child);
                        activeLayers.erase(child);
                    }
                } else {
                    auto agContainer = AntigravityContext::create(child, winSize, currentGravityX, currentGravityY, glueFonts);
                    activeLayers.insert_or_assign(child, agContainer);
                }
            }
        } else {
            resetQueued = false;
            for (const auto& [node, context] : activeLayers) {
                context->reset();
            }
        }

        if (retoggleDragging && touchLayer) {
            touchLayer->setTouchEnabled(true);
        }

        pokeQueued = false;
        CCDirector::drawScene();
    }
};

$on_game(Loaded) {
    listenForKeybindSettingPresses("reset-antigravity", [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
        if (down && !repeat) {
            resetQueued = true;
        }
    });

    listenForKeybindSettingPresses("pause-antigravity", [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
        if (down && !repeat) {
            pauseAntigravity = !pauseAntigravity;
        }
    });

    listenForKeybindSettingPresses("poke-antigravity", [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
        if (down && !repeat) {
            pokeQueued = true;
        }
    });

    if (Loader::get()->getLoadedMod("colon.bumpscosity")) {
        doILoveGDCologne = true;
    }
}
