#include "context.hpp"

void collectAllDescendantsLike(const char* type, CCNode* root, std::vector<CCNode*>* visited) {
    if (!root) return;

    if (std::strcmp(type, getRuntimeTypeName(root)) == 0) {
        visited->push_back(root);
    }

    for (auto child : root->getChildrenExt()) {
        collectAllDescendantsLike(type, child, visited);
    }
}

void collectChildrenForContexts(const char* type, CCNode* root, std::vector<CCNode*>* visited) {
    if (!root) return;
    visited->append_range(root->getChildrenExt());
    for (auto child : root->getChildrenExt()) {
        collectAllDescendantsLike(type, child, visited);
    }
}

// Because CCDirectors do not support fields
std::unordered_map<CCNode*, AntigravityContext*> activeLayers;
CCLayer* layerTypeDummy = nullptr;

#include <Geode/modify/CCDirector.hpp>
class $modify(CCDirector) {
    void drawScene() {
        if (!layerTypeDummy) {
            layerTypeDummy = CCLayer::create();
            CC_SAFE_RETAIN(layerTypeDummy);
        }

        auto mod = Mod::get();
        auto winSize = getWinSize();
        auto scene = getRunningScene();

        std::vector<CCNode*> contextNodes;
        collectChildrenForContexts(getRuntimeTypeName(layerTypeDummy), scene, &contextNodes);

        std::unordered_set<CCNode*> current(contextNodes.begin(), contextNodes.end());

        std::erase_if(activeLayers, [&current](const auto& pair) {
            if (current.contains(pair.first)) {
                return false;
            }
            delete activeLayers.at(pair.first);
            return true;
        });

        if (mod->getSettingValue<bool>("enabled")) {
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
            for (const auto& [node, context] : activeLayers) {
                context->reset();
            }
        }

        CCDirector::drawScene();
    }
};
