using System.Runtime.InteropServices;

namespace Draconic.Engine;

/// <summary>
/// Represents a fundamental unit within the game engine.
/// Each instance of the <see cref="Entity"/> class is uniquely identified by an <see cref="EntityId"/>.
/// Entities can serve as containers for components and behaviors, enabling flexible and modular interaction within the game world.
/// </summary>
public class Entity
{
    /// <summary>
    /// Gets or sets the unique identifier for an entity within the game engine.
    /// </summary>
    public ulong EntityId { get; set; }

    private static ulong _lastEntityId = 0;
    private static readonly Lock _lock = new Lock();

    public Entity()
    {
        lock (_lock)
        {
            EntityId = ++_lastEntityId;
        }
    }
    public Entity(ulong entityId)
    {
        lock (_lock)
        {
            EntityId = entityId;

            if (entityId > _lastEntityId)
            {
                _lastEntityId = entityId;
            }
        }
    }

    public static Entity Create()
    {
        return new Entity();
    }
}