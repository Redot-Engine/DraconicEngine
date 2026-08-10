using Draconic.Engine;

namespace Draconic.Engine.Tests;

public class WorldTests
{
    private readonly struct TestComponent(int value) : IComponent
    {
        public int Value { get; } = value;
    }

    private sealed class TrackingBehavior : Behavior
    {
        public int UpdateCalls { get; private set; }
        public int FixedUpdateCalls { get; private set; }
        public int LateUpdateCalls { get; private set; }
        public float LastDeltaTime { get; private set; }
        public Entity AssignedEntity => Entity;
        public World AssignedWorld => World;

        public TrackingBehavior() : base(new Entity(), new World())
        {
        }

        public override void Update(float deltaTime)
        {
            UpdateCalls++;
            LastDeltaTime = deltaTime;
        }

        public override void FixedUpdate(float deltaTime)
        {
            FixedUpdateCalls++;
            LastDeltaTime = deltaTime;
        }

        public override void LateUpdate(float deltaTime)
        {
            LateUpdateCalls++;
            LastDeltaTime = deltaTime;
        }
    }

    private sealed class ThrowingBehavior : Behavior
    {
        public ThrowingBehavior() : base(new Entity(), new World())
        {
        }

        public override void Update(float deltaTime)
        {
            throw new InvalidOperationException("Expected test exception.");
        }
    }

    [Fact]
    public void CreateEntity_WithoutId_CreatesEntityAndAllowsComponentAndBehaviorOperations()
    {
        var world = new World();
        var entity = world.CreateEntity();

        world.CreateComponent<TestComponent>(entity.EntityId);
        var behavior = new TrackingBehavior();
        world.AddBehavior(entity.EntityId, behavior);

        Assert.True(world.TryGetComponent<TestComponent>(entity.EntityId, out _));
        Assert.True(world.TryGetBehavior<TrackingBehavior>(entity.EntityId, out var fetchedBehavior));
        Assert.Same(behavior, fetchedBehavior);
    }

    [Fact]
    public void CreateEntity_WithProvidedId_UsesProvidedId()
    {
        var world = new World();
        const ulong entityId = 42;

        var entity = world.CreateEntity(entityId);

        Assert.Equal(entityId, entity.EntityId);
    }

    [Fact]
    public void CreateEntity_WithDuplicateProvidedId_ThrowsInvalidOperationException()
    {
        var world = new World();
        const ulong entityId = 42;
        world.CreateEntity(entityId);

        Assert.Throws<InvalidOperationException>(() => world.CreateEntity(entityId));
    }

    [Fact]
    public void CreateEntity_WithProvidedId_AdvancesGeneratedEntityIdPastProvidedId()
    {
        var world = new World();
        const ulong providedId = 1_000_000;

        world.CreateEntity(providedId);
        var generatedEntity = world.CreateEntity();

        Assert.True(generatedEntity.EntityId > providedId);
    }

    [Fact]
    public void DestroyEntity_ExistingEntity_RemovesEntityDataAndReturnsTrue()
    {
        var world = new World();
        var entity = world.CreateEntity();
        world.CreateComponent<TestComponent>(entity.EntityId);
        world.AddBehavior(entity.EntityId, new TrackingBehavior());

        var destroyed = world.DestroyEntity(entity.EntityId);

        Assert.True(destroyed);
        Assert.False(world.TryGetComponent<TestComponent>(entity.EntityId, out _));
        Assert.False(world.TryGetBehavior<TrackingBehavior>(entity.EntityId, out _));
        Assert.False(world.RemoveComponent<TestComponent>(entity.EntityId));
        Assert.False(world.RemoveBehavior<TrackingBehavior>(entity.EntityId));
    }

    [Fact]
    public void DestroyEntity_NonExistentEntity_ReturnsFalse()
    {
        var world = new World();

        var destroyed = world.DestroyEntity(1234);

        Assert.False(destroyed);
    }

    [Fact]
    public void CreateComponent_ExistingEntity_CanBeRetrieved()
    {
        var world = new World();
        var entity = world.CreateEntity();

        world.CreateComponent<TestComponent>(entity.EntityId);

        Assert.True(world.TryGetComponent<TestComponent>(entity.EntityId, out var component));
        Assert.Equal(0, component.Value);
    }

    [Fact]
    public void CreateComponent_DuplicateType_ThrowsInvalidOperationException()
    {
        var world = new World();
        var entity = world.CreateEntity();
        world.CreateComponent<TestComponent>(entity.EntityId);

        Assert.Throws<InvalidOperationException>(() => world.CreateComponent<TestComponent>(entity.EntityId));
    }

    [Fact]
    public void CreateComponent_ForMissingEntity_ThrowsKeyNotFoundException()
    {
        var world = new World();

        Assert.Throws<KeyNotFoundException>(() => world.CreateComponent<TestComponent>(999));
    }

    [Fact]
    public void TryUpdateComponent_ExistingEntity_UpdatesStoredValue()
    {
        var world = new World();
        var entity = world.CreateEntity();
        world.CreateComponent<TestComponent>(entity.EntityId);
        var updated = new TestComponent(77);

        var updateResult = world.TryUpdateComponent(entity.EntityId, ref updated);
        var getResult = world.TryGetComponent<TestComponent>(entity.EntityId, out var component);

        Assert.True(updateResult);
        Assert.True(getResult);
        Assert.Equal(77, component.Value);
    }

    [Fact]
    public void TryUpdateComponent_MissingEntity_ReturnsFalse()
    {
        var world = new World();
        var component = new TestComponent(11);

        var result = world.TryUpdateComponent(444, ref component);

        Assert.False(result);
    }

    [Fact]
    public void RemoveComponent_ExistingComponent_RemovesAndReturnsTrue()
    {
        var world = new World();
        var entity = world.CreateEntity();
        world.CreateComponent<TestComponent>(entity.EntityId);

        var removed = world.RemoveComponent<TestComponent>(entity.EntityId);

        Assert.True(removed);
        Assert.False(world.TryGetComponent<TestComponent>(entity.EntityId, out _));
    }

    [Fact]
    public void AddBehavior_ExistingEntity_SetsWorldAndEntityAndCanBeRetrieved()
    {
        var world = new World();
        var entity = world.CreateEntity();
        var behavior = new TrackingBehavior();

        world.AddBehavior(entity.EntityId, behavior);

        Assert.True(world.TryGetBehavior<TrackingBehavior>(entity.EntityId, out var fetched));
        Assert.Same(behavior, fetched);
        Assert.Same(entity, behavior.AssignedEntity);
        Assert.Same(world, behavior.AssignedWorld);
    }

    [Fact]
    public void AddBehavior_DuplicateType_ThrowsInvalidOperationException()
    {
        var world = new World();
        var entity = world.CreateEntity();
        world.AddBehavior(entity.EntityId, new TrackingBehavior());

        Assert.Throws<InvalidOperationException>(() => world.AddBehavior(entity.EntityId, new TrackingBehavior()));
    }

    [Fact]
    public void AddBehavior_ForMissingEntity_ThrowsKeyNotFoundException()
    {
        var world = new World();

        Assert.Throws<KeyNotFoundException>(() => world.AddBehavior(99, new TrackingBehavior()));
    }

    [Fact]
    public void RemoveBehavior_ExistingBehavior_RemovesAndReturnsTrue()
    {
        var world = new World();
        var entity = world.CreateEntity();
        world.AddBehavior(entity.EntityId, new TrackingBehavior());

        var removed = world.RemoveBehavior<TrackingBehavior>(entity.EntityId);

        Assert.True(removed);
        Assert.False(world.TryGetBehavior<TrackingBehavior>(entity.EntityId, out _));
    }

    [Fact]
    public void ExecuteUpdate_CallsUpdateForRegisteredBehaviors()
    {
        var world = new World();
        var entity = world.CreateEntity();
        var behavior = new TrackingBehavior();
        world.AddBehavior(entity.EntityId, behavior);

        world.ExecuteUpdate(0.25f);

        Assert.Equal(1, behavior.UpdateCalls);
        Assert.Equal(0.25f, behavior.LastDeltaTime);
    }

    [Fact]
    public void ExecuteFixedUpdate_CallsFixedUpdateForRegisteredBehaviors()
    {
        var world = new World();
        var entity = world.CreateEntity();
        var behavior = new TrackingBehavior();
        world.AddBehavior(entity.EntityId, behavior);

        world.ExecuteFixedUpdate(0.5f);

        Assert.Equal(1, behavior.FixedUpdateCalls);
        Assert.Equal(0.5f, behavior.LastDeltaTime);
    }

    [Fact]
    public void ExecuteLateUpdate_CallsLateUpdateForRegisteredBehaviors()
    {
        var world = new World();
        var entity = world.CreateEntity();
        var behavior = new TrackingBehavior();
        world.AddBehavior(entity.EntityId, behavior);

        world.ExecuteLateUpdate(1f);

        Assert.Equal(1, behavior.LateUpdateCalls);
        Assert.Equal(1f, behavior.LastDeltaTime);
    }

    [Fact]
    public void ExecuteUpdate_WhenBehaviorThrows_ContinuesProcessingOthers()
    {
        var world = new World();
        var first = world.CreateEntity();
        var second = world.CreateEntity();
        var trackingBehavior = new TrackingBehavior();

        world.AddBehavior(first.EntityId, new ThrowingBehavior());
        world.AddBehavior(second.EntityId, trackingBehavior);

        var exception = Record.Exception(() => world.ExecuteUpdate(0.16f));

        Assert.Null(exception);
        Assert.Equal(1, trackingBehavior.UpdateCalls);
    }
}