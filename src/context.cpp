#include "context.hpp"

CCPoint getAbsolutePosition(CCNode* node) {
    if (!node) return CCPointZero;
    return node->convertToWorldSpace(node->getContentSize() * 0.5);
}

CCSize getTruerSize(CCNode* node) {
    if (!node) return CCSizeZero;
    auto result = node->getContentSize();
    while (node != nullptr) {
        result.setSize(abs(result.width * node->getScaleX()), abs(result.height * node->getScaleY()));
        node = node->getParent();
    }
    return result;
}

bool nodeIsFullyInsideViewport(CCNode* node, CCSize& winSize) {
    if (!node) return false;

    auto c1 = node->convertToWorldSpace({0.0f, 0.0f});
    auto c2 = node->convertToWorldSpace({node->getContentWidth(), 0.0f});
    auto c3 = node->convertToWorldSpace(node->getContentSize());
    auto c4 = node->convertToWorldSpace({0.0f, node->getContentHeight()});

    auto left = fmin(fmin(c1.x, c2.x), fmin(c3.x, c4.x));
    auto right = fmax(fmax(c1.x, c2.x), fmax(c3.x, c4.x));
    auto bottom = fmin(fmin(c1.y, c2.y), fmin(c3.y, c4.y));
    auto top = fmax(fmax(c1.y, c2.y), fmax(c3.y, c4.y));
    return left >= 0 && right <= winSize.width && bottom >= 0 && top <= winSize.height;
}

bool nodeIntersectsViewport(CCNode* node, CCSize& winSize) {
    if (!node) return false;

    auto c1 = node->convertToWorldSpace({0.0f, 0.0f});
    auto c2 = node->convertToWorldSpace({node->getContentWidth(), 0.0f});
    auto c3 = node->convertToWorldSpace(node->getContentSize());
    auto c4 = node->convertToWorldSpace({0.0f, node->getContentHeight()});

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

void collectDescendants(
    CCNode* node,
    std::vector<CCNode*>* visited,
    CCSize& winSize,
    std::unordered_set<CCNode*>* contextNodes,
    std::unordered_map<CCNode*, b2BodyId>* currentMap,
    bool isEditor = false,
    bool glueFonts = true,
    bool isRoot = true
) {
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
        if (typeinfo_cast<GameObject*>(node) && isActuallyVisible(node) && nodeIntersectsViewport(node, winSize)) {
            visited->push_back(node);
            return;
        }
        for (auto child : node->getChildrenExt()) {
            collectDescendants(child, visited, winSize, contextNodes, currentMap, isEditor, glueFonts, false);
        }
        return;
    }

    if (isAnyOf<GJGroundLayer*>(node)) return;

    if (isAnyOf<CCTextInputNode*, Slider*>(node) || isRelaxedAnyOf<CCParticleSystem*>(node)) {
        if (isActuallyVisible(node) && nodeIntersectsViewport(node, winSize)) {
            if (auto textInput = exact_cast<CCTextInputNode*>(node)) {
                if (!(textInput->getTextLabel() || textInput->m_textArea)) return;
            }
            visited->push_back(node);
            return;
        }
    }

    if (isRelaxedAnyOf<CCMenuItemSpriteExtra*>(node)) {
        if (auto sprite = node->getChildByType<CCSprite*>()) {
            if (!sprite->getOpacity()) return;
        }
        if (isActuallyVisible(node) && nodeIntersectsViewport(node, winSize)) {
            visited->push_back(node);
        }
        return;
    }

    if (isAnyOf<CCSpriteBatchNode*>(node)) return;

    auto isValid = isRelaxedAnyOf<CCNodeRGBA*, CCScrollLayerExt*, TextInput*, Scrollbar*, Label*,
        ListBorders*, ScrollableContentLayer*, MDTextArea*, CCLayerColor*>(node) ||
        (glueFonts ? isRelaxedAnyOf<CCLabelBMFont*>(node) : isAnyOf<CCFontSprite*>(node));

    if (isValid) {
        auto size = getTruerSize(node);
        auto width = size.width;
        auto height = size.height;
        auto isSmallEnough = height < winSize.height && width < winSize.width;
        auto isLargeEnough = width > 0 && height > 0;
        auto dontCareAboutViewport = isDescendantOfRelaxedType<ScrollableContentLayer*>(node);
        auto needOnlyIntersect = isAnyOf<CCScale9Sprite*, NineSlice*>(node);

        if (isSmallEnough && isLargeEnough && isActuallyVisible(node) && (dontCareAboutViewport || nodeIsFullyInsideViewport(node, winSize) || (needOnlyIntersect && nodeIntersectsViewport(node, winSize)))) {
            visited->push_back(node);
            return;
        }

        if (glueFonts && isRelaxedAnyOf<CCLabelBMFont*>(node)) return;
    }

    for (auto child : node->getChildrenExt()) {
        collectDescendants(child, visited, winSize, contextNodes, currentMap, isEditor, glueFonts, false);
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
    chainDef.pointCount = 4;
    chainDef.isLoop = true;

    b2CreateChain(borderBodyId, &chainDef);
};

AntigravityContext* AntigravityContext::create(CCNode* root, CCSize& winSize, float gravityX, float gravityY, bool glueFonts) {
    auto worldDef = b2DefaultWorldDef();
    worldDef.gravity = {gravityX, gravityY};
    auto worldId = b2CreateWorld(&worldDef);

    if (!b2World_IsValid(worldId)) return nullptr;

    AntigravityContext* object = new AntigravityContext(root, worldId, glueFonts);
    object->setUpWorldBorder(winSize);

    object->material.friction = 0.025f;
    object->material.rollingResistance = 0.025f;
    object->material.restitution = 0.5f;

    return object;
}

void AntigravityContext::setGravity(float gravityX, float gravityY) {
    if (!b2World_IsValid(worldId)) return;
    b2World_SetGravity(worldId, {gravityX, gravityY});
}

void AntigravityContext::setGlueFonts(bool set) {
    glueFonts = set;
}

void updateSlider(Slider* slider) {
    auto size = slider->m_groove->getScaledContentSize();
    auto theta = -CC_DEGREES_TO_RADIANS(slider->m_groove->getRotation());
    size.setSize(size.width * cos(theta) - size.height * sin(theta), size.width * sin(theta) + size.height * cos(theta));
    slider->setScaledContentSize(size);
    slider->setAnchorPoint({0.5f, 0.5f});
    slider->ignoreAnchorPointForPosition(false);
    slider->m_groove->setPosition(size * 0.5f);
    slider->m_touchLogic->setPosition(size * 0.5f);
}

void updateTextInputNode(CCTextInputNode* textInput) {
    CCNode* primary = textInput->getTextLabel();
    if (!primary) primary = textInput->m_textArea;
    auto textAlignment = primary->getAnchorPoint();
    auto size = textInput->getScaledContentSize();

    std::unordered_map<CCNode*, CCPoint> initialPositions;

    for (auto child : textInput->getChildrenExt()) {
        initialPositions.insert_or_assign(child, getAbsolutePosition(child));
    }

    centralizeAnchorPoint(textInput);
    textInput->setPositionX(textInput->getPositionX() - size.width * textAlignment.x);
    textInput->setPositionY(textInput->getPositionY() - size.height * textAlignment.y);

    for (auto [child, position] : initialPositions) {
        child->setPosition(child->getPosition() + (position - getAbsolutePosition(child)));
    }
}

CCSize getCCFontSpriteTrueSize(CCFontSprite* fontSprite) {
    auto size = getTruerSize(fontSprite);
    auto texture = fontSprite->getTexture();
    // Thanks Devtools
    auto cachedTextures = CCTextureCache::sharedTextureCache()->m_pTextures;
    for (auto [key, obj] : CCDictionaryExt<std::string, CCTexture2D*>(cachedTextures)) {
        if (obj != texture) continue;
        auto fileName = std::filesystem::path(key).filename().string();
        if (fileName.contains("goldFont") || fileName.compare("bigFont")) {
            size.setSize(size.width * 0.35f, size.height * 0.5f);
        }
        break;
    }
    return size;
}

void AntigravityContext::setUpPhysics(CCNode* node, CCSize& winSize, b2Vec2 velocity, float angularVelocity, float halfThickness) {
    if (!node) return;

    if (auto slider = exact_cast<Slider*>(node)) {
        updateSlider(slider);
    } else if (auto textInput = exact_cast<CCTextInputNode*>(node)) {
        updateTextInputNode(textInput);
    }

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

    auto halfSize = (isAnyOf<CCFontSprite*>(node) ? getCCFontSpriteTrueSize(exact_cast<CCFontSprite*>(node)) : getTruerSize(node)) * 0.5f;
    halfSize.setSize(fmax(halfSize.width, halfThickness), fmax(halfSize.height, halfThickness));
    auto position = getAbsolutePosition(node);

    auto bodyDef = b2DefaultBodyDef();
    bodyDef.type = (isEditor && isDescendantOfType<GJGroundLayer*>(node)) ? b2_staticBody : b2_dynamicBody;
    bodyDef.position = {position.x, position.y};
    float mathematicalAngle = -CC_DEGREES_TO_RADIANS(getAbsoluteRotation(node));
    bodyDef.rotation = b2MakeRot(mathematicalAngle);

    auto bodyId = b2CreateBody(worldId, &bodyDef);

    auto shapeDef = b2DefaultShapeDef();
    shapeDef.material = material;
    if (auto opacityNode = typeinfo_cast<CCNodeRGBA*>(node)) {
        shapeDef.density = opacityNode->getOpacity() / 255.0f;
    }

    auto doSolidBox = isAnyOf<CCMenuItemSpriteExtra*, CCTextInputNode*, Slider*>(node) ||
        isRelaxedAnyOf<GameObject*, Scrollbar*, TextInput*, Label*, CCFontSprite*>(node);

    if (isAnyOf<CircleButtonSprite*>(node)) {
        b2Circle circle = {{0.0f, 0.0f}, 0.5f * (halfSize.width + halfSize.height)};
        b2CreateCircleShape(bodyId, &shapeDef, &circle);
    } else if (doSolidBox) {
        auto box = b2MakeBox(halfSize.width, halfSize.height);
        b2CreatePolygonShape(bodyId, &shapeDef, &box);
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
        if (priorContext.contains(node)) {
            auto isGroundLayerSprite = isDescendantOfType<GJGroundLayer*>(node);
            PriorContext ctx = isGroundLayerSprite ? initialContext.at(node) : priorContext.at(node);

            if (ctx.ref.valid()) {
                if (b2Body_IsValid(pair.second) && nodeIntersectsViewport(node, winSize) && isActuallyVisible(node) && isCurrentlyWithinSceneGraph(node)) {
                    PriorData expectedData = currentMaps[node];
                    auto sizeDiff = (getTruerSize(node) - expectedData.size);
                    if ((expectedData.position - getAbsolutePosition(pair.first)).getLength() <= 0 && abs(expectedData.angle - getAbsoluteRotation(pair.first)) <= 0 && abs(sizeDiff.width) <= 0 && abs(sizeDiff.height) <= 0) return false;
                }

                if (!(isActuallyVisible(node) && isCurrentlyWithinSceneGraph(node)) || isDescendantOfRelaxedType<CustomSongWidget*>(node)) {
                    node->setAnchorPoint(ctx.anchorPoint);
                    node->setPosition(ctx.position);
                    node->setRotation(ctx.rotation);
                    node->ignoreAnchorPointForPosition(ctx.ignoreAnchorPointForPosition);
                }
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
    collectDescendants(root, &nodes, winSize, contextNodes, &maps, isEditor, glueFonts);
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
    for (const auto& [node, ctx] : initialContext) {
        if (maps.contains(node)) {
            auto bodyId = maps.at(node);
            if (b2Body_IsValid(bodyId)) {
                b2DestroyBody(bodyId);
            }
        }

        if (!ctx.ref.valid()) continue;
        node->setAnchorPoint(ctx.anchorPoint);
        node->setPosition(ctx.position);
        node->setRotation(ctx.rotation);
        node->ignoreAnchorPointForPosition(ctx.ignoreAnchorPointForPosition);
    }

    currentMaps.clear();
    priorContext.clear();
    initialContext.clear();
}

AntigravityContext::~AntigravityContext() {
    reset();
    if (b2World_IsValid(worldId)) b2DestroyWorld(worldId);
    worldId = b2_nullWorldId;
}
