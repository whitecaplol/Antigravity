#include "context.hpp"

template <typename T>
void collectAllDescendantsLike(CCNode* root, std::vector<CCNode*>* visited) {
    if (!root || isAnyOf<GJListLayer*>(root)) return;

    if (isAnyOf<T>(root)) {
        visited->push_back(root);
    }

    for (auto child : root->getChildrenExt()) {
        collectAllDescendantsLike<T>(child, visited);
    }
}

void collectChildrenForContexts(CCNode* root, std::vector<CCNode*>* visited) {
    if (!root) return;
    visited->append_range(root->getChildrenExt());
    for (auto child : root->getChildrenExt()) {
        collectAllDescendantsLike<CCLayer*>(child, visited);
    }
}

// Because CCDirectors do not support fields
std::unordered_map<CCNode*, AntigravityContext*> activeLayers;
bool resetQueued = false;
bool pauseAntigravity = false;

#include <Geode/modify/CCDirector.hpp>
class $modify(CCDirector) {
    void drawScene() {
        if (pauseAntigravity && !resetQueued) {
            CCDirector::drawScene();
            return;
        }

        auto mod = Mod::get();
        auto winSize = getWinSize();
        auto scene = getRunningScene();

        std::vector<CCNode*> contextNodes;
        collectChildrenForContexts(scene, &contextNodes);

        std::unordered_set<CCNode*> current(contextNodes.begin(), contextNodes.end());

        std::erase_if(activeLayers, [&current](const auto& pair) {
            if (current.contains(pair.first)) return false;
            delete activeLayers.at(pair.first);
            return true;
        });

        if (mod->getSettingValue<bool>("enabled") && !resetQueued) {
            for (auto child : contextNodes) {
                if (isAnyOf<PlayLayer*>(child)) continue;
                if (!mod->getSettingValue<bool>("enable-in-editor") && isAnyOf<LevelEditorLayer*>(child)) continue;
                if (activeLayers.contains(child)) {
                    auto success = activeLayers[child]->step(getDeltaTime(), &current);
                    if (!success) {
                        delete activeLayers.at(child);
                        activeLayers.erase(child);
                    }
                } else {
                    auto agContainer = AntigravityContext::create(
                        child, winSize,
                        mod->getSettingValue<float>("gravity-x"),
                        mod->getSettingValue<float>("gravity-y")
                    );
                    activeLayers.insert_or_assign(child, agContainer);
                }
            }
        } else {
            resetQueued = false;
            for (const auto& [node, context] : activeLayers) {
                context->reset();
            }
        }

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
}
