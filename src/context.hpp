#include <box2d/box2d.h>

using namespace geode::prelude;

enum ObjectType {
    Circle,
    Solid,
    HollowRight,
    HollowUp,
    HollowLeft,
    HollowDown,
    HollowSensor
};

struct PriorData {
    CCPoint position;
    CCSize size;
    float angle;
};

struct PriorContext {
    WeakRef<CCNode> ref;
    CCPoint anchorPoint;
    CCPoint position;
    float rotation;
    bool ignoreAnchorPointForPosition;
};

struct IntermediateState {
    b2Vec2 velocity;
    float angularVelocity;
};

template <typename... Types>
bool isAnyOf(CCNode* node) {
    return (... || exact_cast<Types>(node));
}

template <typename... Types>
bool isRelaxedAnyOf(CCNode* node) {
    return (... || typeinfo_cast<Types>(node));
}

template <typename T>
bool isDescendantOfType(CCNode* node) {
    while (node != nullptr) {
        if (isAnyOf<T>(node)) return true;
        node = node->getParent();
    }
    return false;
}

template <typename T>
bool isDescendantOfRelaxedType(CCNode* node) {
    while (node != nullptr) {
        if (isRelaxedAnyOf<T>(node)) return true;
        node = node->getParent();
    }
    return false;
}

class AntigravityContext {
private:
    std::unordered_map<CCNode*, PriorData> candidateMaps;
    std::unordered_map<CCNode*, PriorData> currentMaps;
    std::unordered_map<CCNode*, b2BodyId> maps;
    std::unordered_map<CCNode*, PriorContext> priorContext;
    std::unordered_map<CCNode*, PriorContext> initialContext;
    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    WeakRef<CCNode> currentlyDragging = nullptr;
    b2JointId dragJointId = b2_nullJointId;
    CCNode* root;
    CCDirector* currentDirector;
    b2Vec2 gravity = b2Vec2_zero;
    b2WorldId worldId = b2_nullWorldId;
    b2BodyId borderId = b2_nullBodyId;
    bool isEditor;
    bool glueFonts;

    AntigravityContext(CCNode* root, b2WorldId worldId, bool glueFonts, b2Vec2 gravity) : root(root), worldId(worldId), isEditor(isAnyOf<LevelEditorLayer*>(root)), glueFonts(glueFonts), gravity(gravity) {}

    void setUpWorldBorder(CCSize& winSize);
    void setUpPhysics(CCNode* node, b2Vec2 velocity = {0.0f, 0.0f}, float angularVelocity = 0.0f, float halfThickness = 0.5f);
    void updateShapes(CCNode* node, float halfThickness = 0.5f);
public:
    static AntigravityContext* create(CCNode* root, CCSize& winSize, float gravityX = 0.0f, float gravityY = 0.0f, bool glueFonts = true);

    bool step(float dt, std::unordered_set<CCNode*>* contextNodes, bool poke);
    void beginDrag(b2Vec2 click);
    void moveDrag(b2Vec2 click);
    void endDrag();
    void setGravity(float gravityX, float gravityY);
    void setGlueFonts(bool set);
    void reset();

    ~AntigravityContext();
};
