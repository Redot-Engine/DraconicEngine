using System.Collections.Concurrent;
using System.Diagnostics.CodeAnalysis;

namespace Draconic.Engine;

/// <summary>
/// Represents the main context for managing entities, components, and behaviors within the Draconic Engine.
/// </summary>
/// <remarks>
/// The World class provides functionality for creating and destroying entities, attaching and managing components,
/// and adding, retrieving, or removing behaviors associated with entities. It also handles the update cycles
/// (Update, FixedUpdate, LateUpdate) for behaviors.
/// </remarks>
public class World
{
    //ToDo: refactor as a sparse set.

    #region Fields

    /// <summary>
    /// Stores a collection of all entities in the current world context,
    /// providing fast, thread-safe access to entity instances by their unique identifiers.
    /// </summary>
    private readonly ConcurrentDictionary<ulong, Entity> _entities = new();

    /// <summary>
    /// Stores all components associated with entities in the world.
    /// </summary>
    private readonly ConcurrentDictionary<ulong, ConcurrentDictionary<Type, IComponent>> _components = new();

    /// <summary>
    /// Maintains a thread-safe collection of behaviors associated with each entity in the <see cref="World"/>.
    /// </summary>
    private readonly ConcurrentDictionary<ulong, ConcurrentDictionary<Type, Behavior>> _behaviors = new();

    #endregion

    #region Entity Management

    /// Creates a new entity within the world and initializes its associated data structures
    /// for components and behaviors.
    /// The method generates a new entity, adds it to the world's collection of entities,
    /// and registers empty sets of components and behaviors associated with the entity's ID.
    /// <returns>
    /// An instance of the newly created entity.
    /// </returns>
    public Entity CreateEntity()
    {
        var entity = Entity.Create();

        _entities[entity.EntityId] = entity;
        _components.TryAdd(entity.EntityId, new());
        _behaviors.TryAdd(entity.EntityId, new());
        return entity;
    }

    /// <summary>
    /// Creates a new entity with the specified unique identifier.
    /// </summary>
    /// <param name="entityId">The unique identifier to associate with the new entity.</param>
    /// <returns>A newly created <see cref="Entity"/> with the specified identifier.</returns>
    public Entity CreateEntity(ulong entityId)
    {
        if (_entities.ContainsKey(entityId))
        {
            throw new InvalidOperationException($"Entity {entityId} already exists in world.");
        }

        var entity = new Entity(entityId);
        _entities[entity.EntityId] = entity;
        _components.TryAdd(entity.EntityId, new());
        _behaviors.TryAdd(entity.EntityId, new());
        return entity;
    }

    /// <summary>
    /// Removes the specified entity and all associated components and behaviors from the world.
    /// </summary>
    /// <param name="entityId">The unique identifier of the entity to be removed.</param>
    /// <returns>
    /// A boolean value indicating whether the entity was successfully removed.
    /// Returns <c>true</c> if the entity existed and was removed; otherwise, <c>false</c>.
    /// </returns>
    public bool DestroyEntity(ulong entityId)
    {
        var removedEntity = _entities.TryRemove(entityId, out _);
        _components.TryRemove(entityId, out _);
        _behaviors.TryRemove(entityId, out _);

        return removedEntity;
    }

    #endregion

    #region Component Management

    /// Adds a component of the specified type to the given entity within the world.
    /// This method registers the provided component instance and associates it with the entity ID.
    /// If a component of the same type already exists for the entity, an exception is thrown.
    /// <typeparam name="T">The type of the component to add, implementing IComponent.</typeparam>
    /// <param name="entityId">The unique identifier of the entity to which the component will be added.</param>
    /// <param name="component">A reference to the component instance that will be added to the entity.</param>
    /// <exception cref="KeyNotFoundException">Thrown if the entity with the specified ID does not exist in the world.</exception>
    /// <exception cref="InvalidOperationException">Thrown if a component of the same type already exists for the entity.</exception>
    internal void AddComponent<T>(ulong entityId, ref T component) where T : struct, IComponent
    {
        if (!_entities.ContainsKey(entityId))
        {
            throw new KeyNotFoundException($"Entity {entityId} does not exist in world.");
        }

        if (!_components.TryGetValue(entityId, out var components))
        {
            components = new();

            _components[entityId] = components;
        }


        if (!components.TryAdd(component.GetType(), component))
        {
            throw new InvalidOperationException($"Component of type {component.GetType()} already exists for entity {entityId}");
        }
    }

    /// Creates and registers a new component of the specified type for the given entity.
    /// The method initializes the component with its default values and associates it with
    /// the provided entity ID. This allows the component to be included in the entity's data
    /// structure, making it accessible for subsequent operations such as retrieval or updates.
    /// Throws an exception if a component of the same type is already associated with the entity.
    /// <typeparam name="T">
    /// The type of the component to create and associate with the entity. The type must
    /// implement the IComponent interface and be a value type.
    /// </typeparam>
    /// <param name="entityId">
    /// The unique identifier of the entity to which the new component will be associated.
    /// </param>
    public void CreateComponent<T>(ulong entityId) where T : struct, IComponent
    {
        T component = new T();
        AddComponent(entityId, ref component);
    }

    /// Removes a specified component from the entity identified by the given entity ID.
    /// This method ensures that the component of the specified type is removed only if the entity exists
    /// and the component of the given type is currently associated with the entity.
    /// <typeparam name="T">
    /// The type of the component to be removed. Must implement the IComponent interface.
    /// </typeparam>
    /// <param name="entityId">
    /// The unique identifier of the entity from which the component is to be removed.
    /// </param>
    /// <returns>
    /// True if the component was successfully removed; otherwise, false. Returns false if the entity
    /// does not exist or if the specified component is not associated with the entity.
    /// </returns>
    public bool RemoveComponent<T>(ulong entityId) where T : IComponent
    {
        if (!_entities.ContainsKey(entityId))
        {
            return false;
        }

        if (_components.TryGetValue(entityId, out var components))
        {

            return components.TryRemove(typeof(T), out _);
        }

        return false;
    }

    /// Attempts to retrieve a component of the specified type associated with the given entity ID.
    /// If the entity exists and has a component of the specified type, the method outputs the component
    /// and returns true. Otherwise, the method outputs the default value for the component type and returns false.
    /// <typeparam name="T">
    /// The type of the component to retrieve. Must be a struct and implement the IComponent interface.
    /// </typeparam>
    /// <param name="entityId">
    /// The ID of the entity from which the component should be retrieved.
    /// </param>
    /// <param name="component">
    /// When the method returns, contains the component of the specified type if retrieval was successful;
    /// otherwise, contains the default value for the specified type.
    /// </param>
    /// <returns>
    /// True if the entity exists and has a component of the specified type; otherwise, false.
    /// </returns>
    public bool TryGetComponent<T>(ulong entityId, out T component) where T : struct, IComponent
    {
        if (!_entities.ContainsKey(entityId))
        {
            component = default;
            return false;
        }

        if (_components.TryGetValue(entityId, out var components))
        {
            components.TryGetValue(typeof(T), out var componentObj);
            if (componentObj is not null && componentObj is T typedComponent)
            {
                component = typedComponent;
                return true;
            }

        }

        component = default;
        return false;
    }

    /// Attempts to update a component of the specified type for the given entity with the provided value.
    /// If the entity exists and the component type is associated with the entity, the component value is updated,
    /// and the method returns true. If the entity does not exist or the component type is not associated with the entity,
    /// the method returns false.
    /// <param name="entityId">
    /// The unique identifier of the entity for which the component is to be updated.
    /// </param>
    /// <param name="component">
    /// The new value of the component to update. Passed by reference.
    /// </param>
    /// <typeparam name="T">
    /// The type of the component to be updated. Must implement the IComponent interface.
    /// </typeparam>
    /// <returns>
    /// true if the component was successfully updated; otherwise, false.
    /// </returns>
    public bool TryUpdateComponent<T>(ulong entityId, ref T component) where T : struct, IComponent
    {
        if (!_entities.ContainsKey(entityId))
        {
            return false;
        }

        if (_components.TryGetValue(entityId, out var components))
        {
            components[typeof(T)] = component;
            return true;
        }

        return false;
    }

    #endregion

    #region Behavior Management

    /// Adds a behavior to the specified entity within the world.
    /// The behavior is associated with the provided entity ID and stored in the world's behavior collection.
    /// The method ensures that the behavior is assigned to both the entity and the world,
    /// and validates that there are no duplicate behaviors of the same type for the entity.
    /// <typeparam name="T">
    /// The type of the behavior to be added. Must be a subclass of Behavior.
    /// </typeparam>
    /// <param name="entityId">
    /// The unique identifier of the entity to which the behavior is added.
    /// </param>
    /// <param name="behavior">
    /// The behavior instance to be added to the entity.
    /// </param>
    /// <exception cref="KeyNotFoundException">
    /// Thrown if no entity with the specified ID exists in the world.
    /// </exception>
    /// <exception cref="InvalidOperationException">
    /// Thrown if a behavior of the same type already exists for the specified entity.
    /// </exception>
    public void AddBehavior<T>(ulong entityId, T behavior) where T : Behavior
    {
        if (!_entities.TryGetValue(entityId, out var entity))
        {
            throw new KeyNotFoundException($"Entity {entityId} does not exist in world.");
        }

        if (!_behaviors.TryGetValue(entityId, out var behaviors))
        {
            behaviors = new();
            _behaviors[entityId] = behaviors;
        }

        behavior.Entity = entity;
        behavior.World = this;

        if (!behaviors.TryAdd(behavior.GetType(), behavior))
        {
            throw new InvalidOperationException($"Behavior of type {behavior.GetType()} already exists for entity {entityId}");
        }
    }

    /// Attempts to retrieve a behavior of the specified type associated with a given entity ID.
    /// If the behavior exists and is of the requested type, it is returned in the output parameter.
    /// Otherwise, the output parameter is set to null, and the method returns false.
    /// <typeparam name="T">The type of the behavior to retrieve. Must inherit from the Behavior class.</typeparam>
    /// <param name="entityId">The unique identifier for the entity whose behavior is being retrieved.</param>
    /// <param name="behavior">
    /// When the method completes, this parameter contains the behavior of the requested type if it exists;
    /// otherwise, it is set to null.
    /// </param>
    /// <returns>
    /// True if the specified behavior is found and is of the requested type; otherwise, false.
    /// </returns>
    public bool TryGetBehavior<T>(ulong entityId, [NotNullWhen(true)] out T? behavior) where T : Behavior
    {
        if (!_entities.ContainsKey(entityId))
        {
            behavior = null;
            return false;
        }

        if (_behaviors.TryGetValue(entityId, out var behaviors))
        {
            behaviors.TryGetValue(typeof(T), out var behaviorObj);
            if (behaviorObj is T typedBehavior)
            {
                behavior = typedBehavior;
                return true;
            }
        }

        behavior = null;
        return false;
    }

    /// Removes a behavior of the specified type from the given entity.
    /// If the entity exists and the behavior is found, it is removed from the
    /// collection of behaviors associated with the entity.
    /// <typeparam name="T">
    /// The type of behavior to be removed, which must inherit from the Behavior class.
    /// </typeparam>
    /// <param name="entityId">
    /// The unique identifier of the entity from which the behavior should be removed.
    /// </param>
    /// <returns>
    /// True if the behavior was successfully removed; otherwise, false.
    /// </returns>
    public bool RemoveBehavior<T>(ulong entityId) where T : Behavior
    {
        if (!_entities.ContainsKey(entityId))
        {
            return false;
        }

        if (_behaviors.TryGetValue(entityId, out var behaviors))
        {
            return behaviors.TryRemove(typeof(T), out _);
        }

        return false;
    }

    /// Executes the update process for all behaviors registered within the world.
    /// Each behavior's `Update` method is called with the provided delta time to
    /// perform time-based logic or operations. If a behavior throws an exception
    /// during its update, the process continues with the remaining behaviors.
    /// <param name="deltaTime">The time, in seconds, since the last update.</param>
    public void ExecuteUpdate(float deltaTime)
    {
        foreach (var behaviors in _behaviors.Values)
            foreach (var behavior in behaviors.Values)
            {
                try
                {
                    behavior.Update(deltaTime);
                }
                catch (Exception e)
                {
                    Console.Error.WriteLine(e);
                }

            }
    }

    /// Executes the FixedUpdate lifecycle step for all registered behaviors in the world.
    /// This method iterates through all behaviors associated with entities and invokes their
    /// FixedUpdate method with the provided delta time.
    /// If an exception is thrown during the execution of a behavior's FixedUpdate method,
    /// it is caught and logged to the error output stream without interrupting the remaining updates.
    /// <param name="deltaTime">
    /// The time interval, in seconds, that has elapsed since the previous FixedUpdate call.
    /// This value is passed to each behavior's FixedUpdate method to support time-dependent updates.
    /// </param>
    public void ExecuteFixedUpdate(float deltaTime)
    {
        foreach (var behaviors in _behaviors.Values)
            foreach (var behavior in behaviors.Values)
            {
                try
                {
                    behavior.FixedUpdate(deltaTime);
                }
                catch (Exception e)
                {
                    Console.Error.WriteLine(e);
                }
            }
    }

    /// Executes the LateUpdate phase for all registered behaviors in the world.
    /// This method iterates through all behaviors associated with entities in the world
    /// and invokes their LateUpdate method, passing the provided delta time.
    /// Any exceptions thrown during the execution of a behavior's LateUpdate method
    /// are caught and logged to the error output.
    /// <param name="deltaTime">
    /// The time interval, in seconds, since the last LateUpdate cycle.
    /// This value is provided to each behavior to adjust their logic based on the elapsed time.
    /// </param>
    public void ExecuteLateUpdate(float deltaTime)
    {
        foreach (var behaviors in _behaviors.Values)
            foreach (var behavior in behaviors.Values)
            {
                try
                {
                    behavior.LateUpdate(deltaTime);
                }
                catch (Exception e)
                {
                    Console.Error.WriteLine(e);
                }
            }
    }

    #endregion
}