// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Physics_PhysicsWorld_h
#define incl_Physics_PhysicsWorld_h

#include "Core.h"
#include "ForwardDefines.h"
#include "PhysicsModuleApi.h"

#include <set>
#include <QObject>
#include <QVector>

class btCollisionConfiguration;
class btBroadphaseInterface;
class btConstraintSolver;
class btDiscreteDynamicsWorld;
class btDispatcher;
class btDynamicsWorld;
class btCollisionObject;
class PhysicsContact;

namespace Physics
{

class PhysicsModule;

//! A physics world that encapsulates a Bullet physics world
class PHYSICS_MODULE_API PhysicsWorld : public QObject
{
    Q_OBJECT
    
public:
    PhysicsWorld(PhysicsModule* owner);
    virtual ~PhysicsWorld();
    
    //! Step the physics world. May trigger several internal simulation substeps, according to the deltatime given.
    void Simulate(f64 frametime);
    
    //! Process collision from an internal sub-step (Bullet post-tick callback)
    void ProcessPostTick(float substeptime);
    
public slots:
    //! Set internal physics timestep (= length of each simulation step.) By default 1/60th of a second.
    void SetPhysicsUpdatePeriod(float updatePeriod);
    
    //! Return internal physics timestep
    float GetPhysicsUpdatePeriod() const { return physicsUpdatePeriod_; }
    
    //! Set gravity
    /*! Note: Bullet may behave oddly if gravity is changed after objects have already been created.
     */
    void SetGravity(const Vector3df& gravity);
    
    //! Return gravity
    Vector3df GetGravity() const;
    
    //! Return the Bullet world object
    btDynamicsWorld* GetWorld() const;
    
signals:
    //! A physics collision has happened between two entities. 
    /*! Note: both rigidbodies participating in the collision will also emit a signal separately.
        \param entityA The first entity
        \param entityB The second entity
        \param contacts A vector of contacts, which contain position, normal, distance, impulse information
     */
    void PhysicsCollision(Scene::Entity* entityA, Scene::Entity* entityB, const QVector<PhysicsContact*>& contacts);
    
private:
    //! Bullet collision config
    btCollisionConfiguration* collisionConfiguration_;
    //! Bullet collision dispatcher
    btDispatcher* collisionDispatcher_;
    //! Bullet collision broadphase
    btBroadphaseInterface* broadphase_;
    //! Bullet constraint equation solver
    btConstraintSolver* solver_;
    //! Bullet physics world
    btDiscreteDynamicsWorld* world_;
    
    //! Length of internal physics timestep
    float physicsUpdatePeriod_;
    
    //! Previous frame's collisions. We store these to know whether the collision was new or "ongoing"
    std::set<std::pair<btCollisionObject*, btCollisionObject*> > previousCollisions_;
    
    //! Storage for re-using PhysicsContact structures
    std::vector<PhysicsContact*> contactsStore_;
};

}

#endif

