namespace Draconic.Engine;

/// <summary>
/// Represents the base class for all behaviors that can be associated with an entity within a game world.
/// </summary>
/// <remarks>
/// A behavior encapsulates logic and functionality that can be attached to an entity in the world.
/// It provides methods for handling lifecycle events such as initialization, updates, and destruction,
/// which can be overridden by derived classes to implement specific behavior.
/// </remarks>
public abstract class Behavior
{
    /// <summary>
    /// Represents a fundamental unit within the game engine architecture.
    /// </summary>
    /// <remarks>
    /// The <see cref="Entity"/> class serves as the cornerstone for organizing objects in the game world.
    /// Each entity is uniquely identified by its <see cref="EntityId"/>.
    /// Entities can contain components and behaviors to define their characteristics and functionality.
    /// </remarks>
    protected internal Entity Entity { get; internal set; }

    /// <summary>
    /// Provides the primary context for managing entities, components, and behaviors in the gaming engine.
    /// </summary>
    /// <remarks>
    /// The World property links behaviors to the World instance that governs the lifecycle and interaction
    /// of all entities and components in the system. It acts as an interface for behaviors to access and
    /// interact with the overarching simulation environment.
    /// </remarks>
    protected internal World World { get; internal set; }

    /// <summary>
    /// Represents an abstract base class for behaviors attached to entities within the game world.
    /// A behavior defines specific functionality or actions that can be applied to an entity.
    /// </summary>
    protected Behavior(Entity entity, World world)
    {
        Entity = entity;
        World = world;
    }

    /// <summary> Updates the behavior with the logic defined by derived classes.
    /// This method is called during the world's update cycle and should
    /// be overridden by subclasses to implement specific behavior logic.
    /// </summary>
    /// <param name="deltaTime">The time, in seconds, since the last update.</param>
    public virtual void Update(float deltaTime)
    {

    }

    /// <summary>
    /// Invoked when a behavior is initialized and attached to an entity within the game world.
    /// This method is primarily intended for performing setup logic, initializing state, or registering any required resources
    /// specific to the behavior instance.
    /// Override this method in derived classes to implement custom initialization logic.
    /// </summary>
    public virtual void Awake()
    {

    }

    /// <summary>
    /// Executes logic for a fixed update cycle. This method is used in for processing
    /// physics-related updates or other time-dependent logic intended to run
    /// at a consistent interval, independent of the frame rate.
    /// </summary>
    /// <param name="deltaTime">
    /// The fixed time-step duration since the last fixed update cycle, typically used for
    /// calculations requiring a consistent time interval.
    /// </param>
    public virtual void FixedUpdate(float deltaTime)
    {

    }

    /// <summary>
    /// Invoked during the LateUpdate phase of the game loop for this behavior.
    /// This method is called after all Update methods have run in the current frame,
    /// allowing behaviors to perform operations that should occur later in the frame.
    /// </summary>
    /// <param name="deltaTime">
    /// The time elapsed, in seconds, since the last frame. Use this value to perform
    /// time-based calculations and ensure frame-rate independent behavior.
    /// </param>
    public virtual void LateUpdate(float deltaTime)
    {

    }

    /// <summary>
    /// Called when the behavior's associated entity is being destroyed.
    /// This method provides an opportunity to perform cleanup or finalization tasks
    /// related to the behavior before the entity's destruction is completed.
    /// By default, this method performs no operation, but it can be overridden
    /// in derived classes to implement specific destruction logic.
    /// </summary>
    public virtual void OnDestroyed()
    {

    }

}