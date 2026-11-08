#include "context.hpp"

CCPoint getAbsolutePosition(CCNode* node) {
    if (!node) return CCPointZero;
    return node->convertToWorldSpace(node->getContentSize() * 0.5);
}

CCSize getTruerSize(CCNode* node) {
    if (!node) return CCSizeZero;
    auto result = node->getContentSize();
    while (node != nullptr) {
        result.setSize(result.width * node->getScaleX(), result.height * node->getScaleY());
        node = node->getParent();
    }
    return result;
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
        if (auto opacityNode = typeinfo_cast<CCNodeRGBA*>(node)) {
            if (opacityNode->getOpacity() == 0) {
                return false;
            }
        }
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

void collectDescendants(CCNode* node, std::vector<CCNode*>* visited, CCSize& winSize, std::unordered_set<CCNode*>* contextNodes, std::unordered_map<CCNode*, b2BodyId>* currentMap, bool isEditor = false, bool isRoot = true) {
    if (!node) return;

    if (currentMap->contains(node)) {
        visited->push_back(node);
        return;
    }

    if (!isRoot && (contextNodes->contains(node) || (node->getChildrenCount() == 1 && contextNodes->contains(node->getChildByIndex(0))))) return;

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
            collectDescendants(child, visited, winSize, contextNodes, currentMap, isEditor, false);
        }
        return;
    }

    if (isAnyOf<CCTextInputNode*>(node)) {
        if (isActuallyVisible(node)) {
            visited->push_back(node);
            return;
        }
    }

    if (isAnyOf<CCMenuItemSpriteExtra*>(node)) {
        if (auto sprite = node->getChildByType<CCSprite*>()) {
            if (!sprite->getOpacity()) return;
        }
        if (isActuallyVisible(node)) {
            visited->push_back(node);
        }
        return;
    }

    auto shouldAlsoGetChildren = isAnyOf<ButtonSprite*>(node);
    auto isValid = isAnyOf<CCSprite*, CCMenuItemToggler*, Slider*, CCFontSprite*, SimplePlayer*, GJRobotSprite*, GJSpiderSprite*,
        CCLayerColor*, CCScale9Sprite*, NineSlice*, ScrollLayer*, TextInput*, Label*, Scrollbar*, ListBorders*, GJListLayer*, InfoAlertButton*,
        MDTextArea*, LazySprite*, GJDifficultySprite*, EmoteSprite*>(node);
    auto size = getTruerSize(node);
    auto width = size.width;
    auto height = size.height;
    auto isSmallEnough = height < winSize.height && width < winSize.width;
    auto isLargeEnough = width > 0 && height > 0;
    auto needOnlyIntersect = isAnyOf<CCScale9Sprite*, NineSlice*>(node);

    if ((isValid || shouldAlsoGetChildren) && isSmallEnough && isLargeEnough && isActuallyVisible(node) && (nodeIsFullyInsideViewport(node, winSize) || (needOnlyIntersect && nodeIntersectsViewport(node, winSize)))) {
        visited->push_back(node);
        if (!shouldAlsoGetChildren) return;
    }

    for (auto child : node->getChildrenExt()) {
        collectDescendants(child, visited, winSize, contextNodes, currentMap, isEditor, false);
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
    return remainder(rotation, 360);
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

    if (!b2World_IsValid(worldId)) return nullptr;

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

    if (!initialContext.contains(node)) {
        initialContext.insert_or_assign(node, priorContext.at(node));
    }

    centralizeAnchorPoint(node);

    auto halfSize = getTruerSize(node) * 0.5f;
    auto position = getAbsolutePosition(node);

    auto bodyDef = b2DefaultBodyDef();
    bodyDef.type = (isEditor && isDescendantOfType<GJGroundLayer*>(node)) ? b2_staticBody : b2_dynamicBody;
    bodyDef.position = {position.x, position.y};
    float mathematicalAngle = -CC_DEGREES_TO_RADIANS(getAbsoluteRotation(node));
    bodyDef.rotation = b2MakeRot(mathematicalAngle);

    auto bodyId = b2CreateBody(worldId, &bodyDef);

    auto shapeDef = b2DefaultShapeDef();
    if (auto opacityNode = typeinfo_cast<CCNodeRGBA*>(node)) {
        shapeDef.density = opacityNode->getOpacity() / 255.0f;
    }

    if (auto fontSprite = typeinfo_cast<CCFontSprite*>(node)) {
        auto texture = fontSprite->getTexture();
        // Thanks Devtools
        auto cachedTextures = CCTextureCache::sharedTextureCache()->m_pTextures;
        for (auto [key, obj] : CCDictionaryExt<std::string, CCTexture2D*>(cachedTextures)) {
            if (obj != texture) continue;
            auto fileName = std::filesystem::path(key).filename().string();
            if (fileName.contains("goldFont") || fileName.compare("bigFont")) {
                halfSize *= 0.2f;
            }
            break;
        }
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

    PriorData data{getAbsolutePosition(node), getTruerSize(node), getAbsoluteRotation(node)};
    currentMaps.insert_or_assign(node, data);
}

bool AntigravityContext::step(float dt, std::unordered_set<CCNode*>* contextNodes, bool poke) {
    if (!b2World_IsValid(worldId)) return false;

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    std::erase_if(maps, [this, &winSize](const auto& pair) {
        CCNode* node = pair.first;
        if (b2Body_IsValid(pair.second) && nodeIntersectsViewport(node, winSize) && isActuallyVisible(node) && isCurrentlyWithinSceneGraph(node)) {
            PriorData expectedData = currentMaps[node];
            auto sizeDiff = (getTruerSize(node) - expectedData.size);
            if ((expectedData.position - getAbsolutePosition(pair.first)).getLength() <= 0 && abs(expectedData.angle - getAbsoluteRotation(pair.first)) <= 0 && abs(sizeDiff.width) <= 0 && abs(sizeDiff.height) <= 0) return false;
        }
        if (priorContext.contains(node)) {
            auto isGroundLayerSprite = isDescendantOfType<GJGroundLayer*>(node);
            PriorContext ctx = isGroundLayerSprite ? initialContext.at(node) : priorContext.at(node);
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
    collectDescendants(root, &nodes, winSize, contextNodes, &maps, isEditor);
    std::unordered_set<CCNode*> lookup(nodes.begin(), nodes.end());

    std::erase_if(candidateMaps, [&lookup](const auto& pair) {
        return !lookup.contains(pair.first);
    });

    auto initialVelocity = (poke ? -b2World_GetGravity(worldId) : b2Vec2_zero);

    for (auto node : nodes) {
        if (maps.contains(node)) {
            auto id = maps.at(node);
            if (poke && b2Body_GetType(id) != b2_staticBody) {
                b2Body_SetLinearVelocity(id, b2Body_GetLinearVelocity(id) + initialVelocity);
            }
            continue;
        }
        if (!isEditor) {
            if (candidateMaps.contains(node)) {
                auto prior = candidateMaps[node];
                auto velocity = (getAbsolutePosition(node) - prior.position) / dt;
                auto angularVelocity = -(getAbsoluteRotation(node) - prior.angle) / dt;
                auto sizeDiff = (getTruerSize(node) - prior.size);

                if (!maps.contains(node->getParent()) && (velocity.getLength() > 0 || abs(angularVelocity) > 0 || abs(sizeDiff.width) > 0 || abs(sizeDiff.height) > 0)) goto update_data; // Sorry
                setUpPhysics(node, winSize, initialVelocity, 0);
                candidateMaps.erase(node);
            } else {
                update_data:
                PriorData data{getAbsolutePosition(node), getTruerSize(node), getAbsoluteRotation(node)};
                candidateMaps.insert_or_assign(node, data);
            }
        } else {
            if (!maps.contains(node)) {
                setUpPhysics(node, winSize, initialVelocity, 0);
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
        PriorData data{getAbsolutePosition(node), getTruerSize(node), getAbsoluteRotation(node)};
        currentMaps.insert_or_assign(node, data);
    }

    return true;
}

void AntigravityContext::reset() {
    for (const auto& [node, bodyId] : maps) {
        if (initialContext.contains(node)) {
            PriorContext ctx = initialContext.at(node);
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
