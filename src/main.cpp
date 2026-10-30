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

CCPoint getAbsolutePosition(CCNode* node) {
    if (!node) return CCPointZero;
    return node->convertToWorldSpace(node->getContentSize() * 0.5);
}

bool nodeIsInsideViewport(CCNode* node, CCSize& winSize) {
    if (!node) return false;

    auto boundingBox = node->boundingBox();
    auto origin = getAbsolutePosition(node);
    auto left = origin.x - boundingBox.size.width / 2;
    auto right = origin.x + boundingBox.size.width / 2;
    auto bottom = origin.y - boundingBox.size.height / 2;
    auto top = origin.y + boundingBox.size.height / 2;
    return left <= winSize.width && right >= 0 && bottom <= winSize.height && top >= 0;
}

void collectDescendants(CCNode* node, std::vector<CCNode*>* visited, CCSize& winSize) {
    if (!node) return;

    if (is_any_of<CCMenuItemSpriteExtra*>(node)) {
        visited->push_back(node);
        return;
    }

    auto isValid = is_any_of<CCLabelBMFont*, CCSprite*, CCMenuItemToggler*, Slider*,
        CCLayerColor*, CCScale9Sprite*, NineSlice*, ScrollLayer*, TextInput*, Label*>(node);
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

void centralizeAnchorPoint(CCNode* node) {
    if (!node) return;
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
    if (!node) return;
    node->setRotation(angle - getAbsoluteRotation(node->getParent()));
}

#include <Geode/modify/CCNode.hpp>
class $modify(AntigravityContainer, CCNode) {
    struct Fields {
        std::unordered_map<CCNode*, PriorData> candidateMaps;
        std::unordered_map<CCNode*, PriorData> currentMaps; // Just to see if something else decided to modify the UI in the meantime.
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

        PriorData data{getAbsolutePosition(node), getAbsoluteRotation(node)};
        m_fields->currentMaps.insert_or_assign(node, data);
    }

    void step(float dt) {
        if (!b2World_IsValid(m_fields->worldId)) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        std::vector<CCNode*> nodes;

        collectDescendants(this, &nodes, winSize);

        std::unordered_set<CCNode*> lookup(nodes.begin(), nodes.end());
        std::erase_if(m_fields->maps, [this, &lookup, &winSize](const auto& pair) {
            if (lookup.contains(pair.first)) {
                PriorData expectedData = m_fields->currentMaps[pair.first];
                if ((expectedData.position - getAbsolutePosition(pair.first)).getLength() <= 0 && abs(expectedData.angle - getAbsoluteRotation(pair.first)) <= 0) return false;
            }
            b2DestroyBody(pair.second);
            m_fields->currentMaps.erase(pair.first);
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

                if (velocity.getLength() > 0 || abs(angularVelocity) > 0) goto update_data; // Sorry
                setUpPhysics(node, winSize, {0, 0}, 0); // TODO: add optional menu setting here
                m_fields->candidateMaps.erase(node);
            } else {
                update_data:
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

        for (auto node : nodes) {
            if (!m_fields->maps.contains(node)) continue;
            PriorData data{getAbsolutePosition(node), getAbsoluteRotation(node)};
            m_fields->currentMaps.insert_or_assign(node, data);
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
