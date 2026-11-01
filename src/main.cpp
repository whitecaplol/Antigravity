#include "context.hpp"

// Because CCDirectors do not support fields
std::unordered_map<CCNode*, AntigravityContext*> activeLayers;

#include <Geode/modify/CCDirector.hpp>
class $modify(CCDirector) {
    void drawScene() {
        CCDirector::drawScene();

        auto mod = Mod::get();
        if (!mod->getSettingValue<bool>("enabled")) return;

        auto winSize = getWinSize();

        auto children = getRunningScene()->getChildrenExt();
        std::unordered_set<CCNode*> current(children.begin(), children.end());

        std::erase_if(activeLayers, [&current](const auto& pair) {
            if (current.contains(pair.first)) {
                return false;
            }
            delete activeLayers.at(pair.first);
            return true;
        });

        for (auto child : children) {
            if (!mod->getSettingValue<bool>("enable-everywhere") && is_any_of<PlayLayer*, LevelEditorLayer*>(child)) continue;
            if (activeLayers.contains(child)) {
                activeLayers[child]->step(getDeltaTime());
            } else {
                auto agContainer = AntigravityContext::create(
                    child, winSize,
                    mod->getSettingValue<float>("gravity-x"),
                    mod->getSettingValue<float>("gravity-y")
                );
                activeLayers.insert_or_assign(child, agContainer);
            }
        }
    }
};
