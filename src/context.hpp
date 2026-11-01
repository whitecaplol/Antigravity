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

class AntigravityContext {
private:
    std::unordered_map<CCNode*, PriorData> candidateMaps;
    std::unordered_map<CCNode*, PriorData> currentMaps; // Just to see if something else decided to modify the UI in the meantime.
    std::unordered_map<CCNode*, b2BodyId> maps;
    CCNode* root;
    b2WorldId worldId = b2_nullWorldId;

    AntigravityContext(CCNode* root, b2WorldId worldId) : root(root), worldId(worldId) {}

    void setUpWorldBorder(CCSize& winSize);
    void setUpPhysics(CCNode* node, CCSize& winSize, b2Vec2 velocity = {0, 0}, float angularVelocity = 0.0f);
public:
    static AntigravityContext* create(CCNode* root, CCSize& winSize, float gravityX = 0.0f, float gravityY = 0.0f);

    bool step(float dt);
    ~AntigravityContext();
};
