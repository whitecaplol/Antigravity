#include <vector>
#include <unordered_map>

#include <Geode/Geode.hpp>
#include <box2d/box2d.h>

using namespace geode::prelude;

template <typename... Types>
bool is_any_of(cocos2d::CCNode* node) {
    return (typeinfo_cast<Types>(node) || ...);
}

void collectDescendants(CCNode* node, std::vector<WeakRef<CCNode>>* visited, CCSize& winSize) {
    if (!node) return;

    bool isCorrectType = is_any_of<CCMenuItemSpriteExtra*, CCLabelBMFont*,
        CCSprite*, CCMenuItemToggler*, Slider*, CCScale9Sprite*>(node);
    bool isSmallEnough = node->getScaledContentHeight() < winSize.height &&
        node->getScaledContentWidth() < winSize.width;
    // bool isOpaque = false;
    // if (auto opacityNode = typeinfo_cast<CCNodeRGBA*>(node)) {
    //     isOpaque = opacityNode->getOpacity() > 0;
    // }

    if (isCorrectType && isSmallEnough) { //&& isOpaque) {
        visited->push_back(WeakRef(node));
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

void centralizeAnchorPoint(CCNode* node) {
    auto oldAnchor = node->getAnchorPoint();
    node->setPosition(node->getPosition() + ccp((0.5 - oldAnchor.x) * node->getContentWidth(), (0.5 - oldAnchor.y) * node->getContentHeight()));
    node->setAnchorPoint({0.5, 0.5});
}

#include <Geode/modify/CCLayer.hpp>
class $modify(AntigravityLayer, CCLayer) {
    struct Fields {
        std::vector<WeakRef<CCNode>>* nodes = new std::vector<WeakRef<CCNode>>{};
        std::unordered_map<WeakRef<CCNode>, b2Body*>* maps = new std::unordered_map<WeakRef<CCNode>, b2Body*>();
        b2World* world = new b2World({0.0f, -9.8f});

        ~Fields() {
            delete nodes;
            delete maps;
            delete world;
        }
    };

    bool init() {
        if (!CCLayer::init()) return false;

        auto weakThis = WeakRef<CCLayer>(this);

        Loader::get()->queueInMainThread([weakThis]() {
            auto safeThis = weakThis.lock();
            if (!safeThis || !isStandaloneLayer(safeThis)) return;

            log::info("Found layer {}!", safeThis->getID());

            auto agLayer = static_cast<AntigravityLayer*>(safeThis.data());
            auto winSize = CCDirector::sharedDirector()->getWinSize();

            collectDescendants(safeThis, agLayer->m_fields->nodes, winSize);

            if (agLayer->m_fields->nodes->empty()) return;
            log::info("Found {} nodes!", agLayer->m_fields->nodes->size());

            agLayer->setUpPhysics(winSize);
            agLayer->schedule(schedule_selector(AntigravityLayer::step));
        });

        return true;
    }

    void setUpPhysics(CCSize& winSize) {
        b2BodyDef borderBodyDef;
        borderBodyDef.type = b2_staticBody;
        borderBodyDef.position.Set(winSize.width/2, winSize.height/2);

        auto borderBody = m_fields->world->CreateBody(&borderBodyDef);

        b2EdgeShape edgeShape;
        b2Vec2 bottomLeft(0.0f, 0.0f);
        b2Vec2 bottomRight(winSize.width, 0.0f);
        b2Vec2 topRight(winSize.width, winSize.height);
        b2Vec2 topLeft(0.0f, winSize.height);

        edgeShape.SetTwoSided(bottomLeft, bottomRight);
        borderBody->CreateFixture(&edgeShape, 0.0f);

        edgeShape.SetTwoSided(bottomRight, topRight);
        borderBody->CreateFixture(&edgeShape, 0.0f);

        edgeShape.SetTwoSided(topRight, topLeft);
        borderBody->CreateFixture(&edgeShape, 0.0f);

        edgeShape.SetTwoSided(topLeft, bottomLeft);
        borderBody->CreateFixture(&edgeShape, 0.0f);

        std::random_device rd;
        std::mt19937 gen(rd());

        std::uniform_real_distribution<float> forceX(-1.0f, 1.0f);
        std::uniform_real_distribution<float> forceY(0.0f, 5.0f);
        std::uniform_real_distribution<float> torque(-1.0f, 1.0f);

        for (const auto& node : *m_fields->nodes) {
            auto safeNode = node.lock();
            if (!safeNode) return;

            centralizeAnchorPoint(safeNode);
            auto position = safeNode->convertToWorldSpace(safeNode->getContentSize() * 0.5f);

            b2BodyDef bodyDef;
            bodyDef.type = b2_dynamicBody;
            bodyDef.position.Set(position.x, position.y);
            bodyDef.angle = CC_DEGREES_TO_RADIANS(safeNode->getRotation());

            auto body = m_fields->world->CreateBody(&bodyDef);

            auto massData = b2MassData{ .mass = safeNode->getScaledContentWidth() * safeNode->getScaledContentHeight() / winSize.width / winSize.height, .center={0,0}, .I=2.5 };
            body->SetMassData(&massData);

            body->ApplyLinearImpulseToCenter({forceX(gen) * massData.mass, forceY(gen) * massData.mass}, true);
            body->ApplyAngularImpulse(torque(gen) * massData.mass, true);

            m_fields->maps->insert_or_assign(node, body);
            log::info("Created a body definition at {}, {}", m_fields->maps->at(node)->GetPosition().x, m_fields->maps->at(node)->GetPosition().y);
        }
    }

    void step(float dt) {
        auto map = *m_fields->maps;
        auto world = m_fields->world;
        std::erase_if(*m_fields->nodes, [&map, &world](WeakRef<CCNode> node) {
            if (node.valid()) return false;
            world->DestroyBody(map[node]);
            map.erase(node);
            return true;
        });

        world->Step(dt, 8, 3);
        for (const auto& node : *m_fields->nodes) {
            auto safeNode = node.lock();
            auto position = map[node]->GetPosition();
            auto rotation = -CC_RADIANS_TO_DEGREES(map[node]->GetAngle());
            safeNode->setPosition(
                (safeNode->getParent()) ?
                safeNode->getParent()->convertToNodeSpace({position.x, position.y}) :
                ccp(position.x, position.y)
            );
            safeNode->setRotation(rotation);
        }
    }

    void onExit() {
        this->unschedule(schedule_selector(AntigravityLayer::step));
        CCLayer::onExit();
    }
};
