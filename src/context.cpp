#include "context.hpp"

CCPoint getAbsolutePosition(CCNode* node) {
    if (!node) return CCPointZero;
    return node->convertToWorldSpace(node->getContentSize() * 0.5);
}

bool nodeIsFullyInsideViewport(CCNode* node, CCSize& winSize) {
    if (!node) return false;

    auto c1 = node->convertToWorldSpace(ccp(0, 0));
    auto c2 = node->convertToWorldSpace(ccp(node->getContentWidth(), 0));
    auto c3 = node->convertToWorldSpace(node->getContentSize());
    auto c4 = node->convertToWorldSpace(ccp(0, node->getContentHeight()));

    auto left = fmin(fmin(c1.x, c2.x), fmin(c3.x, c4.x));
    auto right = fmax(fmax(c1.x, c2.x), fmax(c3.x, c4.x));
    auto bottom = fmin(fmin(c1.y, c2.y), fmin(c3.y, c4.y));
    auto top = fmax(fmax(c1.y, c2.y), fmax(c3.y, c4.y));
    return left >= 0 && right <= winSize.width && bottom >= 0 && top <= winSize.height;
}

bool nodeIntersectsViewport(CCNode* node, CCSize& winSize) {
    if (!node) return false;

    auto c1 = node->convertToWorldSpace(ccp(0, 0));
    auto c2 = node->convertToWorldSpace(ccp(node->getContentWidth(), 0));
    auto c3 = node->convertToWorldSpace(node->getContentSize());
    auto c4 = node->convertToWorldSpace(ccp(0, node->getContentHeight()));

    auto left = fmin(fmin(c1.x, c2.x), fmin(c3.x, c4.x));
    auto right = fmax(fmax(c1.x, c2.x), fmax(c3.x, c4.x));
    auto bottom = fmin(fmin(c1.y, c2.y), fmin(c3.y, c4.y));
    auto top = fmax(fmax(c1.y, c2.y), fmax(c3.y, c4.y));
    return left <= winSize.width && right >= 0 && bottom <= winSize.height && top >= 0;
}

bool isActuallyVisible(CCNode* node) {
    while (node != nullptr) {
        if (!node->isVisible()) return false;
        node = node->getParent();
    }
    return true;
}

bool isCurrentlyWithinSceneGraph(CCNode* node) {
    auto root = CCDirector::sharedDirector()->getRunningScene();
    while (node != root) {
        if (!node) return false;
        node = node->getParent();
    }
    return true;
}

void collectDescendants(CCNode* node, std::vector<CCNode*>* visited, CCSize& winSize, bool isEditor = false) {
    if (!node) return;

    if (isEditor) {
        if (isAnyOf<EditorUI*>(node)) return;
        if (auto ground = typeinfo_cast<GJGroundLayer*>(node)) {
            if (auto child = node->getChildByType<CCSpriteBatchNode*>()) {
                if (auto blockContainer = child->getChildByType<CCSprite*>()) {
                    for (auto child : blockContainer->getChildrenExt()) {
                        visited->push_back(child);
                    }
                }
            }
            return;
        }
        if (isAnyOf<GameObject*>(node)) {
            visited->push_back(node);
            return;
        }
        for (auto child : node->getChildrenExt()) {
            collectDescendants(child, visited, winSize, isEditor);
        }
        return;
    }

    auto onlyGetChildren = isAnyOf<CCLabelBMFont*>(node);

    if (onlyGetChildren) {
        for (auto child : node->getChildrenExt()) {
            collectDescendants(child, visited, winSize);
        }
        return;
    }

    if (isAnyOf<CCMenuItemSpriteExtra*>(node)) {
        visited->push_back(node);
        return;
    }

    auto shouldAlsoGetChildren = isAnyOf<ButtonSprite*>(node);
    auto isValid = isAnyOf<CCSprite*, CCMenuItemToggler*, Slider*,
        CCLayerColor*, CCScale9Sprite*, NineSlice*, ScrollLayer*, TextInput*, Label*, Scrollbar*>(node);
    auto width = node->getScaledContentWidth(), height = node->getScaledContentHeight();
    auto isSmallEnough = height < winSize.height && width < winSize.width;
    auto isLargeEnough = width > 0 && height > 0;

    auto needOnlyIntersect = isAnyOf<CCScale9Sprite*, NineSlice*>(node);

    if ((isValid || shouldAlsoGetChildren) && isSmallEnough && isLargeEnough && isActuallyVisible(node) && (nodeIsFullyInsideViewport(node, winSize) || (needOnlyIntersect && nodeIntersectsViewport(node, winSize)))) {
        visited->push_back(node);
        if (!shouldAlsoGetChildren) return;
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


void AntigravityContext::setUpWorldBorder(CCSize& winSize) {
    auto borderBodyDef = b2DefaultBodyDef();
    borderBodyDef.type = b2_staticBody;

    auto borderBodyId = b2CreateBody(worldId, &borderBodyDef);

    b2Vec2 vertices[4] = {{0, 0}, {0, winSize.height}, {winSize.width, winSize.height}, {winSize.width, 0}};
    auto chainDef = b2DefaultChainDef();
    chainDef.points = vertices;
    chainDef.count = 4;
    chainDef.isLoop = true;

    b2CreateChain(borderBodyId, &chainDef);
};

AntigravityContext* AntigravityContext::create(CCNode* root, CCSize& winSize, float gravityX, float gravityY) {
    auto worldDef = b2DefaultWorldDef();
    worldDef.gravity = {gravityX, gravityY};
    auto worldId = b2CreateWorld(&worldDef);

    if (!b2World_IsValid(worldId)) {
        return nullptr;
    }

    AntigravityContext* object = new AntigravityContext(root, worldId);
    object->setUpWorldBorder(winSize);

    return object;
}

void AntigravityContext::setUpPhysics(CCNode* node, CCSize& winSize, b2Vec2 velocity, float angularVelocity) {
    if (!node) return;

    priorContext.insert_or_assign(node, PriorContext{
        WeakRef(node),
        node->getAnchorPoint(),
        node->getPosition(),
        node->getRotation(),
        node->isIgnoreAnchorPointForPosition()
    });

    centralizeAnchorPoint(node);

    auto halfSize = node->getScaledContentSize() * 0.5f;
    if (isAnyOf<CCFontSprite*>(node)) {
        halfSize *= 0.2f;
    }

    auto position = getAbsolutePosition(node);

    auto bodyDef = b2DefaultBodyDef();
    bodyDef.type = (isEditor && isDescendantOfType<GJGroundLayer*>(node)) ? b2_staticBody : b2_dynamicBody;
    bodyDef.position = {position.x, position.y};
    bodyDef.rotation = b2MakeRot(CC_DEGREES_TO_RADIANS(getAbsoluteRotation(node)));

    auto bodyId = b2CreateBody(worldId, &bodyDef);

    auto shapeDef = b2DefaultShapeDef();
    if (auto opacityNode = typeinfo_cast<CCNodeRGBA*>(node)) {
        shapeDef.density = opacityNode->getOpacity() / 255.0f;
    }

    if (isAnyOf<CircleButtonSprite*, LoadingCircleSprite*>(node)) {
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

    maps.insert_or_assign(node, bodyId);

    PriorData data{getAbsolutePosition(node), getAbsoluteRotation(node)};
    currentMaps.insert_or_assign(node, data);
}

bool AntigravityContext::step(float dt) {
    if (!b2World_IsValid(worldId)) return false;

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    std::erase_if(maps, [this, &winSize](const auto& pair) {
        CCNode* node = pair.first;
        if (nodeIntersectsViewport(node, winSize) && isActuallyVisible(node) && isCurrentlyWithinSceneGraph(node)) {
            PriorData expectedData = currentMaps[node];
            if ((expectedData.position - getAbsolutePosition(pair.first)).getLength() <= 0 && abs(expectedData.angle - getAbsoluteRotation(pair.first)) <= 0) return false;
        }
        if (priorContext.contains(node)) {
            PriorContext ctx = priorContext.at(node);
            if (ctx.ref.valid() && !(isActuallyVisible(node) && isCurrentlyWithinSceneGraph(node))) {
                node->setAnchorPoint(ctx.anchorPoint);
                node->setPosition(ctx.position);
                node->setRotation(ctx.rotation);
                node->ignoreAnchorPointForPosition(ctx.ignoreAnchorPointForPosition);
            }
        }
        if (b2Body_IsValid(pair.second)) {
            b2DestroyBody(pair.second);
        }
        currentMaps.erase(node);
        priorContext.erase(node);
        return true;
    });

    std::vector<CCNode*> nodes;
    collectDescendants(root, &nodes, winSize, isEditor);
    std::unordered_set<CCNode*> lookup(nodes.begin(), nodes.end());

    std::erase_if(candidateMaps, [&lookup](const auto& pair) {
        return !lookup.contains(pair.first);
    });

    for (auto node : nodes) {
        if (maps.contains(node)) continue; // TODO for future: Update boxes as needed.
        if (!isEditor) {
            if (candidateMaps.contains(node)) {
                auto prior = candidateMaps[node];
                auto velocity = (getAbsolutePosition(node) - prior.position) / dt;
                auto angularVelocity = -(getAbsoluteRotation(node) - prior.angle) / dt;

                if (!maps.contains(node->getParent()) && (velocity.getLength() > 0 || abs(angularVelocity) > 0)) goto update_data; // Sorry
                setUpPhysics(node, winSize, {0, 0}, 0); // TODO: add optional menu setting here
                candidateMaps.erase(node);
            } else {
                update_data:
                PriorData data{getAbsolutePosition(node), getAbsoluteRotation(node)};
                candidateMaps.insert_or_assign(node, data);
            }
        } else {
            if (!maps.contains(node)) {
                setUpPhysics(node, winSize, {0, 0}, 0);
            }
        }
    }

    b2World_Step(worldId, dt, 4);

    for (auto node : nodes) {
        if (!maps.contains(node)) continue;
        auto bodyId = maps.at(node);
        auto position = b2Body_GetPosition(maps.at(node));
        auto rotation = -CC_RADIANS_TO_DEGREES(b2Rot_GetAngle(b2Body_GetRotation(bodyId)));
        node->setPosition(
            node->getParent()->convertToNodeSpace({position.x, position.y})
        );
        setAbsoluteRotation(node, rotation);
    }

    for (auto node : nodes) {
        if (!maps.contains(node)) continue;
        PriorData data{getAbsolutePosition(node), getAbsoluteRotation(node)};
        currentMaps.insert_or_assign(node, data);
    }

    return true;
}

void AntigravityContext::reset() {
    for (const auto& [node, bodyId] : maps) {
        if (priorContext.contains(node)) {
            PriorContext ctx = priorContext.at(node);
            if (!ctx.ref.valid()) continue;
            node->setAnchorPoint(ctx.anchorPoint);
            node->setPosition(ctx.position);
            node->setRotation(ctx.rotation);
            node->ignoreAnchorPointForPosition(ctx.ignoreAnchorPointForPosition);
        }
        if (b2Body_IsValid(bodyId)) {
            b2DestroyBody(bodyId);
        }
        currentMaps.erase(node);
        priorContext.erase(node);
    }
}

AntigravityContext::~AntigravityContext() {
    if (b2World_IsValid(worldId)) b2DestroyWorld(worldId);
    worldId = b2_nullWorldId;
}
