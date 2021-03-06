// For conditions of distribution and use, see copyright notice in LICENSE

#pragma once

#include "TundraCoreApi.h"
#include "CoreTypes.h"
#include "Math/float2.h"
#include "Math/float3.h"
#include "LoggingFunctions.h" // Note: This is a temporary inclusion for RaycastResult deprecation warning prints.

#include <limits>
#include <QObject>

class Scene;
class Entity;
class IComponent;
class EC_Camera;

/// Result of a raycast to the rendered representation of a scene.
/** Other fields are valid only if entity is non-null
    @sa OgreWorld
    @todo Remove the QObject inheritance here, and expose as a struct to scripts. */
class TUNDRACORE_API RaycastResult : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Entity* entity READ getentity); /**< @copydoc entity */
    Q_PROPERTY(IComponent *component READ getcomponent); /**< @copydoc component */
    Q_PROPERTY(float3 pos READ getpos); /**< @copydoc pos */
    Q_PROPERTY(float3 normal READ getnormal); /**< @copydoc normal */
    Q_PROPERTY(unsigned submeshIndex READ getsubmeshIndex); ///< Submesh index in entity, starting from 0
    Q_PROPERTY(unsigned triangleIndex READ gettriangeIndex); ///< Triangle index in submesh
    Q_PROPERTY(float2 uv READ getuv); ///< The UV coordinates of the first texture channel in the mesh. (0,0) if no texture mapping.
    Q_PROPERTY(float t READ gett);  /**< @copydoc t */

    /// @cond PRIVATE
    Q_PROPERTY(unsigned submesh READ getsubmesh);
    Q_PROPERTY(unsigned index READ getindex);
    Q_PROPERTY(float u READ getu);
    Q_PROPERTY(float v READ getv);
    Entity* getentity() const { return entity; }
    IComponent *getcomponent() const { return component; }
    float3 getpos() const { return pos; }
    float3 getnormal() const { return normal; }
    unsigned getsubmesh() const { LogWarning("RaycastResult.submesh will be deprecated. Use RaycastResult.submeshIndex instead."); return submesh; }
    unsigned getindex() const { LogWarning("RaycastResult.index will be deprecated. Use RaycastResult.triangleIndex nstead."); return index; }
    float getu() const { LogWarning("RaycastResult.u will be deprecated. Use RaycastResult.uv.x instead."); return u; }
    float getv() const { LogWarning("RaycastResult.v will be deprecated. Use RaycastResult.uv.y instead."); return v; }
    unsigned getsubmeshIndex() const { return submesh; }
    unsigned gettriangeIndex() const { return index; }
    float2 getuv() const { return float2(u, v); }
    float gett() const { return t; }
    /// @endcond
public:
    /// Entity that was hit, null if none
    Entity* entity;
    /// Component which was hit.
    IComponent *component;
    /// World coordinates of hit position
    float3 pos;
    /// World face normal of hit
    float3 normal;
    /// Submesh index in entity, starting from 0
    /// @note 'submesh' will be deprecated, use 'submeshIndex' from QtScript instead.
    unsigned submesh;
    /// Triangle index in submesh
    /// @note 'index' will be deprecated, use 'triangleIndex' from QtScript instead.
    unsigned index;
    /// U coord in entity. 0 if no texture mapping
    /// @note 'u' will be deprecated, use 'uv' from QtScript instead.
    float u;
    /// V coord in entity. 0 if no texture mapping
    /// @note 'v' will be deprecated, use 'uv' from QtScript instead.
    float v;
    /// Distance along the ray to the point of intersection. If this is FLOAT_INF, then no intersection has occurred.
    float t;
};

///\todo This structure replaces and obsoletes the above. Delete the above one.
struct TUNDRACORE_API RayQueryResult
{
    RayQueryResult()
    :entity(0), component(0), pos(float3::nan), 
    normal(float3::nan), submeshIndex((unsigned)-1), triangleIndex((unsigned)-1), 
    uv(float2::nan), barycentricUV(float2::nan), t(std::numeric_limits<float>::quiet_NaN())
    {
    }

    /// Entity that was hit, null if none
    Entity* entity;
    /// Component which was hit.
    IComponent *component;
    /// World coordinates of hit position
    float3 pos;
    /// World face normal of hit
    float3 normal;
    /// Submesh index in entity, starting from 0
    unsigned submeshIndex;
    /// Triangle index in submesh
    unsigned triangleIndex;
    /// The UV coordinates of the first texture channel in the mesh. (0,0) if no texture mapping.
    float2 uv;
    /// Barycentric coordinates along the triangle.
    float2 barycentricUV;
    // With barycentric coordinates (0,0) corresponds to triangle's v0, (1,0) to v1, and (0,1) to v2. barycentricU + barycentricV <= 1.
    /// Distance along the ray to the point of intersection. If this is FLOAT_INF, then no intersection has occurred.
    float t;
};

/// Describes the system renderer.
/** @note This class is not an abstract reimplementable interface, but exists only internally for DLL dependency inversion
        purposes between Framework and OgreRenderer::OgreRenderingModule. This interface is only internal to Framework.
        Do not extend this interface. Avoid using it in client code, and prefer directly getting the OgreRenderer::Renderer
        object from OgreWorld or OgreRenderer::OgreRenderingModule.*/
class TUNDRACORE_API IRenderer
{
public:
    IRenderer() {}

    virtual ~IRenderer() {}

    /// Renders the scene.
    /// Do not call this function yourself. It is only used internally by Framework to drive Ogre rendering.
    virtual void Render(float frameTime) = 0;

    /// Returns the Entity which contains the currently active camera that is used to render on the main window.
    /// The returned Entity is guaranteed to have an EC_Camera component, and it is guaranteed to be attached to a scene.
    virtual Entity *MainCamera() = 0;

    /// Returns the EC_Camera of the main camera, or 0 if no main camera is active.
    virtual EC_Camera *MainCameraComponent() = 0;

    /// Returns the Scene the current active main camera is in, or 0 if no main camera is active.
    virtual Scene *MainCameraScene() = 0;

    /// Returns an unique string name for a new object. This function is intended to be used to generate unique names for Ogre resources.
    /// @todo Rename to GenerateUniqueObjectName
    virtual std::string GetUniqueObjectName(const std::string &prefix) = 0;
};
