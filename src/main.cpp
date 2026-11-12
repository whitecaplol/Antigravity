#include "context.hpp"

void collectRemainingDescendants(CCNode* root, std::vector<CCNode*>* visited) {
    if (!root || isAnyOf<GJListLayer*, PlayLayer*, LevelCell*>(root)) return;

    if (isAnyOf<CCLayer*>(root) && (!isRelaxedAnyOf<CCLayer*>(root->getParent()) || root->getParent()->getChildrenCount() == 1)) {
        visited->push_back(root);
    }

    for (auto child : root->getChildrenExt()) {
        collectRemainingDescendants(child, visited);
    }
}

void collectChildrenForContexts(CCNode* root, std::vector<CCNode*>* visited) {
    if (!root || root->getChildByType<LoadingLayer*>()) return;
    visited->append_range(root->getChildrenExt());
    for (auto child : root->getChildrenExt()) {
        if (isAnyOf<LevelEditorLayer*>(child)) continue;
        collectRemainingDescendants(child, visited);
    }
}

// Wow
// #include <Geode/modify/CCTextInputNode.hpp>
// class $modify(CCTextInputNode) {
//     bool ccTouchBegan(CCTouch* touch, CCEvent* event) {
//         touch->setTouchInfo(CCTextInputNode::getID(), float x, float y)
//
//         auto point = convertTouchToNodeSpace(touch);
//         auto result = 0 <= point.x && point.x <= getContentWidth() && 0 <= point.y && point.y <= getContentHeight();
//         log::debug("{}", result);
//
//         auto trueResult = CCTextInputNode::ccTouchBegan(touch, event);
//         log::debug("{}", trueResult);
//         return trueResult;
//     }
// };

// Because CCDirectors do not support fields
std::unordered_map<CCNode*, AntigravityContext*> activeLayers;
bool resetQueued = false;
bool pokeQueued = false;
bool pauseAntigravity = false;

#include <Geode/modify/CCDirector.hpp>
class $modify(CCDirector) {
    void drawScene() {
        if (pauseAntigravity && !resetQueued) {
            CCDirector::drawScene();
            return;
        }

        auto scene = getRunningScene();
        if (isRelaxedAnyOf<CCTransitionScene*>(scene)) {
            CCDirector::drawScene();
            return;
        }

        auto mod = Mod::get();
        auto winSize = getWinSize();

        std::vector<CCNode*> contextNodes;
        collectChildrenForContexts(scene, &contextNodes);

        std::unordered_set<CCNode*> current(contextNodes.begin(), contextNodes.end());

        std::erase_if(activeLayers, [&current](const auto& pair) {
            if (current.contains(pair.first)) return false;
            delete activeLayers.at(pair.first);
            return true;
        });

        auto currentGravityX = mod->getSettingValue<float>("gravity-x");
        auto currentGravityY = mod->getSettingValue<float>("gravity-y");
        auto glueFonts = mod->getSettingValue<bool>("glue-fonts");
        auto dt = fmin(getDeltaTime(), 1.0f / 30);

        if (mod->getSettingValue<bool>("enabled") && !resetQueued) {
            for (auto child : contextNodes) {
                if (isAnyOf<PlayLayer*>(child)) continue;
                if (!mod->getSettingValue<bool>("enable-in-editor") && isAnyOf<LevelEditorLayer*>(child)) continue;
                if (activeLayers.contains(child)) {
                    auto context = activeLayers[child];
                    context->setGravity(currentGravityX, currentGravityY);
                    context->setGlueFonts(glueFonts);
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
}
