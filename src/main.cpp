#include <box2d/box2d.h>

using namespace geode::prelude;

constexpr float halfThickness = 0.5f;

struct PriorData {
    CCPoint position;
    float angle;
};

template <typename... Types>
bool is_any_of(CCNode* node) {
    return (... || typeinfo_cast<Types>(node));
}

bool nodeIsInsideViewport(CCNode* node, CCSize& winSize) {
    auto boundingBox = node->boundingBox();
    auto origin = boundingBox.origin;
    if (node->getParent()) {
        origin = node->getParent()->convertToWorldSpace(origin);
    }
    auto left = origin.x;
    auto right = origin.x + boundingBox.size.width;
    auto bottom = origin.y;
    auto top = origin.y + boundingBox.size.height;
    return left <= winSize.width && right >= 0 && bottom <= winSize.height && top >= 0;
}

void collectDescendants(CCNode* node, std::vector<CCNode*>* visited, CCSize& winSize) {
    if (!node) return;

    if (is_any_of<CCMenuItemSpriteExtra*>(node)) {
        visited->push_back(node);
        return;
    }

    auto isValid = is_any_of<CCLabelBMFont*, CCSprite*, CCMenuItemToggler*, Slider*,
        CCLayerColor*, CCScale9Sprite*, NineSlice*, ScrollLayer*, TextInput*>(node);
    auto width = node->getScaledContentWidth(), height = node->getScaledContentHeight();
    auto isSmallEnough = height < winSize.height && width < winSize.width;
    auto isLargeEnough = width > 0 && height > 0;
    if (isValid && isSmallEnough && isLargeEnough && node->isVisible() && nodeIsInsideViewport(node, winSize)) {
        visited->push_back(node);
        return;
    }

    for (auto child : node->getChildrenExt()) {
        collectDescendants(child, visited, winSize);
    }
}

CCPoint getAbsolutePosition(CCNode* node) {
    return node->convertToWorldSpace(node->getContentSize() * 0.5);
}

void centralizeAnchorPoint(CCNode* node) {
    auto oldAnchor = node->getAnchorPoint();
    if (node->isIgnoreAnchorPointForPosition()) {
        node->ignoreAnchorPointForPosition(false);
        node->setAnchorPoint({0, 0});
    }
    node->setPosition(node->getParent()->convertToNodeSpace(getAbsolutePosition(node)));
    node->setAnchorPoint({0.5, 0.5});
}

float getAbsoluteRotation(CCNode* node) {
    auto rotation = 0.0f;
    while (node) {
        rotation += node->getRotation();
        node = node->getParent();
    }
    return clampf(rotation, -180.0f, 180.0f);
}

void setAbsoluteRotation(CCNode* node, float angle) {
    node->setRotation(angle - getAbsoluteRotation(node->getParent()));
}

#include <Geode/modify/CCNode.hpp>
class $modify(AntigravityContainer, CCNode) {
    struct Fields {
        std::unordered_map<CCNode*, PriorData> candidateMaps;
        std::unordered_map<CCNode*, b2BodyId> maps;
        b2WorldId worldId = b2_nullWorldId;

        ~Fields() {
            if (b2World_IsValid(worldId)) b2DestroyWorld(worldId);
            worldId = b2_nullWorldId;
        }
    };

    bool initPhysics(CCSize winSize, float gravityX = 0.0f, float gravityY = 0.0f) {
        auto worldDef = b2DefaultWorldDef();
        worldDef.gravity = {gravityX, gravityY};
        m_fields->worldId = b2CreateWorld(&worldDef);

        if (!b2World_IsValid(m_fields->worldId)) {
            return false;
        }

        setUpWorldBorder(winSize);
        schedule(schedule_selector(AntigravityContainer::step));

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

    void setUpPhysics(CCNode* node, CCSize& winSize, b2Vec2 velocity = {0, 0}, float angularVelocity = 0.0f) {
        if (!node) return;

        centralizeAnchorPoint(node);

        auto halfSize = node->getScaledContentSize() * 0.5f;
        auto position = node->convertToWorldSpaceAR(CCPointZero);

        auto bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = {position.x, position.y};
        bodyDef.rotation = b2MakeRot(CC_DEGREES_TO_RADIANS(getAbsoluteRotation(node)));

        auto bodyId = b2CreateBody(m_fields->worldId, &bodyDef);

        auto shapeDef = b2DefaultShapeDef();
        if (auto opacityNode = typeinfo_cast<CCNodeRGBA*>(node)) {
            shapeDef.density = opacityNode->getOpacity() / 255.0f;
        }

        if (is_any_of<CircleButtonSprite*, LoadingCircleSprite*>(node)) {
            b2Circle circle = {{0.0f, 0.0f}, 0.5f * (halfSize.width + halfSize.height)};
            b2CreateCircleShape(bodyId, &shapeDef, &circle);
        } else {
            auto leftBox = b2MakeOffsetBox(halfThickness, halfSize.height, {-(halfSize.width-halfThickness), 0}, b2Rot_identity);
            b2CreatePolygonShape(bodyId, &shapeDef, &leftBox);

            auto rightBox = b2MakeOffsetBox(halfThickness, halfSize.height, {(halfSize.width-halfThickness), 0}, b2Rot_identity);
            b2CreatePolygonShape(bodyId, &shapeDef, &rightBox);

            auto upBox = b2MakeOffsetBox(halfSize.width, halfThickness, {0, (halfSize.height-halfThickness)}, b2Rot_identity);
            b2CreatePolygonShape(bodyId, &shapeDef, &upBox);

            auto downBox = b2MakeOffsetBox(halfSize.width, halfThickness, {0, -(halfSize.height-halfThickness)}, b2Rot_identity);
            b2CreatePolygonShape(bodyId, &shapeDef, &downBox);
        }

        auto mass = b2Body_GetMass(bodyId);
        b2Body_ApplyLinearImpulseToCenter(bodyId, mass * velocity, true);
        b2Body_ApplyAngularImpulse(bodyId, mass * angularVelocity, true);

        m_fields->maps.insert_or_assign(node, bodyId);
    }

    void step(float dt) {
        if (!b2World_IsValid(m_fields->worldId)) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        std::vector<CCNode*> nodes;

        collectDescendants(this, &nodes, winSize);

        std::unordered_set<CCNode*> lookup(nodes.begin(), nodes.end());
        std::erase_if(m_fields->maps, [&lookup](const auto& pair) {
            if (lookup.contains(pair.first)) return false;
            b2DestroyBody(pair.second);
            return true;
        });

        std::erase_if(m_fields->candidateMaps, [&lookup](const auto& pair) {
            return !lookup.contains(pair.first);
        });

        for (auto node : nodes) {
            if (m_fields->maps.contains(node)) continue; // TODO for future: Update boxes as needed.
            if (m_fields->candidateMaps.contains(node)) {
                auto prior = m_fields->candidateMaps[node];
                auto velocity = (getAbsolutePosition(node) - prior.position) / dt;
                auto angularVelocity = -(getAbsoluteRotation(node) - prior.angle) / dt;
                setUpPhysics(node, winSize, {velocity.x, velocity.y}, angularVelocity);
                m_fields->candidateMaps.erase(node);
            } else {
                PriorData data{getAbsolutePosition(node), getAbsoluteRotation(node)};
                m_fields->candidateMaps.insert_or_assign(node, data);
            }
        }

        b2World_Step(m_fields->worldId, dt, 4);
        for (auto node : nodes) {
            if (!m_fields->maps.contains(node)) continue;
            auto bodyId = m_fields->maps.at(node);
            auto position = b2Body_GetPosition(m_fields->maps.at(node));
            auto rotation = -CC_RADIANS_TO_DEGREES(b2Rot_GetAngle(b2Body_GetRotation(bodyId)));
            node->setPosition(
                node->getParent()->convertToNodeSpace({position.x, position.y})
            );
            setAbsoluteRotation(node, rotation);
        }
    }

    void onExit() {
        unschedule(schedule_selector(AntigravityContainer::step));
        CCNode::onExit();
    }
};

// Because CCDirectors do not support fields
std::unordered_set<AntigravityContainer*> activeLayers;

#include <Geode/modify/CCDirector.hpp>
class $modify(CCDirector) {
    void drawScene() {
        CCDirector::drawScene();

        auto mod = Mod::get();
        if (!mod->getSettingValue<bool>("enabled")) return;

        auto winSize = getWinSize();

        auto children = getRunningScene()->getChildrenExt();
        std::unordered_set<CCNode*> current(children.begin(), children.end());

        std::erase_if(activeLayers, [&current](const auto& item) {
            return current.find(item) == current.end();
        });

        for (auto child : children) {
            if (!mod->getSettingValue<bool>("enable-everywhere") && is_any_of<PlayLayer*, LevelEditorLayer*>(child)) continue;
            auto agContainer = static_cast<AntigravityContainer*>(child);
            if (activeLayers.contains(agContainer)) continue;
            agContainer->initPhysics(
                winSize,
                mod->getSettingValue<float>("gravity-x"),
                mod->getSettingValue<float>("gravity-y")
            );
            activeLayers.insert(agContainer);
        }
    }
};
