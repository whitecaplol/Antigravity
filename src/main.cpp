#include "Geode/cocos/base_nodes/CCNode.h"
#include <Geode/Geode.hpp>
#include <vector>

using namespace geode::prelude;

template <typename... Types>
bool is_any_of(cocos2d::CCNode* node) {
    return (typeinfo_cast<Types>(node) || ...);
}

void collectDescendants(CCNode* node, std::vector<CCNode*>& visited, CCSize& winSize) {
    bool is_correct_type = is_any_of<CCMenuItemSpriteExtra*, CCLabelBMFont*,
        CCSprite*, CCMenuItemToggler*, Slider*>(node);
    bool is_small_enough = node->getScaledContentHeight() < winSize.height &&
        node->getScaledContentWidth() < winSize.width;
    if (is_correct_type && is_small_enough) {
        visited.push_back(node);
        return;
    }

    for (CCNode* child : node->getChildrenExt()) {
        collectDescendants(child, visited, winSize);
    }
}

bool isStandaloneLayer(CCNode* layer) {
    while (layer != nullptr) {
        layer = layer->getParent();
        if (is_any_of<CCLayer*>(layer)) return false;
    }
    return true;
}

#include <Geode/modify/CCLayer.hpp>
class $modify(AntigravityLayer, CCLayer) {
    bool init() {
        if (!CCLayer::init()) return false;

        auto weakThis = WeakRef<CCLayer>(this);

        Loader::get()->queueInMainThread([weakThis]() {
            auto safeThis = weakThis.lock();
            if (!safeThis || !isStandaloneLayer(safeThis)) return;
            log::info("Found layer {}!", safeThis->getID());

            CCSize winSize = CCDirector::sharedDirector()->getWinSize();

            std::vector<CCNode*> descendants = {};
            collectDescendants(safeThis, descendants, winSize);

            if (descendants.empty()) return;
            log::info("Found {} nodes!", descendants.size());
        });

        return true;
    }
};
