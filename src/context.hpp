#include <box2d/box2d.h>

using namespace geode::prelude;

constexpr float halfThickness = 1.0f;

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

class AntigravityContext {
private:
    std::unordered_map<CCNode*, PriorData> candidateMaps;
    std::unordered_map<CCNode*, PriorData> currentMaps;
    std::unordered_map<CCNode*, b2BodyId> maps;
    std::unordered_map<CCNode*, PriorContext> priorContext;
    std::unordered_map<CCNode*, PriorContext> initialContext;
    CCNode* root;
    CCDirector* currentDirector;
    b2WorldId worldId = b2_nullWorldId;
    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    bool isEditor;
    bool glueFonts;

    AntigravityContext(CCNode* root, b2WorldId worldId, bool glueFonts) : root(root), worldId(worldId), isEditor(isAnyOf<LevelEditorLayer*>(root)), glueFonts(glueFonts) {}

    void setUpWorldBorder(CCSize& winSize);
    void setUpPhysics(CCNode* node, CCSize& winSize, b2Vec2 velocity = {0, 0}, float angularVelocity = 0.0f);
public:
    static AntigravityContext* create(CCNode* root, CCSize& winSize, float gravityX = 0.0f, float gravityY = 0.0f, bool glueFonts = true);

    bool step(float dt, std::unordered_set<CCNode*>* contextNodes, bool poke);
    void setGravity(float gravityX, float gravityY);
    void setGlueFonts(bool set);
    void reset();

    ~AntigravityContext();
};
