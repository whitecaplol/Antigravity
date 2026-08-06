#include <Geode/Geode.hpp>
#include <arc/prelude.hpp>
#include <box2d/box2d.h>

using namespace geode::prelude;

constexpr float halfThickness = 1.0f;

std::random_device rd;
std::mt19937 gen(rd());

std::uniform_real_distribution<float> forceX(-1.0f, 1.0f);
std::uniform_real_distribution<float> forceY(0.0f, 5.0f);
std::uniform_real_distribution<float> torque(-1.0f, 1.0f);

template <typename... Types>
bool is_any_of(CCNode* node) {
    return (... || typeinfo_cast<Types>(node));
}

void collectDescendants(CCNode* node, std::vector<CCNode*>* visited, CCSize& winSize) {
    if (!node) return;

    bool isCorrectType = is_any_of<CCMenuItemSpriteExtra*, CCLabelBMFont*,
        CCSprite*, CCMenuItemToggler*, Slider*, CCScale9Sprite*>(node);
    bool isSmallEnough = node->getScaledContentHeight() < winSize.height &&
        node->getScaledContentWidth() < winSize.width;

    if (isCorrectType && isSmallEnough) {
        visited->push_back(node);
        return;
    } else if (isCorrectType && !isSmallEnough) {
        return;
    }

    for (CCNode* child : node->getChildrenExt()) {
        collectDescendants(child, visited, winSize);
    }
}

void centralizeAnchorPoint(CCNode* node) {
    auto oldAnchor = node->getAnchorPoint();
    node->setPosition(node->getPosition() + ccp((0.5 - oldAnchor.x) * node->getContentWidth(), (0.5 - oldAnchor.y) * node->getContentHeight()));
    node->setAnchorPoint({0.5, 0.5});
}

float getAbsoluteRotation(CCNode* node) {
    float rotation = 0.0f;
    while (node) {
        rotation += node->getRotation();
        node = node->getParent();
    }
    return clampf(rotation, -180.0f, 180.0f);
}

#include <Geode/modify/CCLayer.hpp>
class $modify(AntigravityLayer, CCLayer) {
    struct Fields {
        std::unordered_map<CCNode*, b2BodyId>* maps = new std::unordered_map<CCNode*, b2BodyId>();
        b2WorldId worldId;

        Fields() {
            b2WorldDef worldDef = b2DefaultWorldDef();
            worldDef.gravity = {0.0f, -9.8f};
            worldId = b2CreateWorld(&worldDef);
        }

        ~Fields() {
            delete maps;
            maps = nullptr;

            b2DestroyWorld(worldId);
            worldId = b2_nullWorldId;
        }
    };

    bool initPhysics(CCSize winSize) {
        if (!b2World_IsValid(m_fields->worldId)) {
            return true;
        }
        setUpWorldBorder(winSize);

        this->schedule(schedule_selector(AntigravityLayer::step));

        return true;
    }

    void setUpWorldBorder(CCSize& winSize) {
        auto borderBodyDef = b2DefaultBodyDef();
        borderBodyDef.type = b2_staticBody;

        auto borderBodyId = b2CreateBody(m_fields->worldId, &borderBodyDef);

        b2Vec2 vertices[4] = {{0, 0}, {0, winSize.height}, {winSize.width, winSize.height}, {winSize.width, 0}};
        auto chainDef = b2DefaultChainDef();
        chainDef.points = vertices;
        chainDef.count = 4;
        chainDef.isLoop = true;

        b2CreateChain(borderBodyId, &chainDef);
    }

    void setUpPhysics(CCNode* node, CCSize& winSize) {
        if (!node) return;

        centralizeAnchorPoint(node);

        auto halfSize = node->getScaledContentSize() * 0.5f;
        auto position = node->convertToWorldSpace(halfSize);

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = {position.x, position.y};
        bodyDef.rotation = b2MakeRot(CC_DEGREES_TO_RADIANS(getAbsoluteRotation(node)));

        auto bodyId = b2CreateBody(m_fields->worldId, &bodyDef);

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        if (auto opacityNode = typeinfo_cast<CCNodeRGBA*>(node)) {
            shapeDef.density = opacityNode->getOpacity() / 255.0f;
        }

        auto leftBox = b2MakeOffsetBox(halfThickness, halfSize.height, {-(halfSize.width-halfThickness), 0}, b2Rot_identity);
        b2CreatePolygonShape(bodyId, &shapeDef, &leftBox);

        auto rightBox = b2MakeOffsetBox(halfThickness, halfSize.height, {(halfSize.width-halfThickness), 0}, b2Rot_identity);
        b2CreatePolygonShape(bodyId, &shapeDef, &rightBox);

        auto upBox = b2MakeOffsetBox(halfSize.width, halfThickness, {0, (halfSize.height-halfThickness)}, b2Rot_identity);
        b2CreatePolygonShape(bodyId, &shapeDef, &upBox);

        auto downBox = b2MakeOffsetBox(halfSize.width, halfThickness, {0, -(halfSize.height-halfThickness)}, b2Rot_identity);
        b2CreatePolygonShape(bodyId, &shapeDef, &downBox);

        auto normalizedWeight = halfSize.height / winSize.height * halfSize.width / winSize.width;

        b2Body_ApplyLinearImpulseToCenter(bodyId, {forceX(gen) * normalizedWeight, forceY(gen) * normalizedWeight}, true);
        b2Body_ApplyAngularImpulse(bodyId, torque(gen) * normalizedWeight, true);

        m_fields->maps->insert_or_assign(node, bodyId);
        log::debug("Created a body definition at {}, {}", position.x, position.y);
    }

    void step(float dt) {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        std::vector<CCNode*> nodes;

        collectDescendants(this, &nodes, winSize);

        std::unordered_set<CCNode*> lookup(nodes.begin(), nodes.end());
        std::erase_if(*m_fields->maps, [&lookup](const auto& pair) {
            if (lookup.contains(pair.first)) return false;
            b2DestroyBody(pair.second);
            return true;
        });

        for (auto node : nodes) {
            if (!m_fields->maps->contains(node)) {
                setUpPhysics(node, winSize);
            }
        }

        b2World_Step(m_fields->worldId, dt, 4);
        for (auto node : nodes) {
            if (!m_fields->maps->contains(node)) continue;
            auto bodyId = m_fields->maps->at(node);
            auto position = b2Body_GetPosition(m_fields->maps->at(node));
            auto rotation = -CC_RADIANS_TO_DEGREES(b2Rot_GetAngle(b2Body_GetRotation(bodyId)));
            node->setPosition(
                (node->getParent()) ?
                node->getParent()->convertToNodeSpace({position.x, position.y}) :
                ccp(position.x, position.y)
            );
            node->setRotation(rotation);
        }
    }

    void onExit() {
        this->unschedule(schedule_selector(AntigravityLayer::step));
        CCLayer::onExit();

    }
};

#include<Geode/modify/CCDirector.hpp>
class $modify(CCDirector) {
    void willSwitchToScene(CCScene* scene) {
        CCDirector::willSwitchToScene(scene);
        for (auto child : scene->getChildrenExt()) {
            if (auto layer = typeinfo_cast<CCLayer*>(child)) {
                static_cast<AntigravityLayer*>(layer)->initPhysics(getWinSize());
            }
        }
    }
};
