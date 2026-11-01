#include "context.hpp"

// Because CCDirectors do not support fields
std::unordered_map<CCNode*, AntigravityContext*> activeLayers;

#include <Geode/modify/CCDirector.hpp>
class $modify(CCDirector) {
    void drawScene() {
        auto mod = Mod::get();
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

        if (mod->getSettingValue<bool>("enabled")) {
            for (auto child : children) {
                if (isAnyOf<PlayLayer*>(child)) continue;
                if (!mod->getSettingValue<bool>("enable-in-editor") && isAnyOf<LevelEditorLayer*>(child)) continue;
                if (activeLayers.contains(child)) {
                    auto success = activeLayers[child]->step(getDeltaTime());
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
            for (const auto& [node, context] : activeLayers) {
                context->reset();
            }
        }

        CCDirector::drawScene();
    }
};
