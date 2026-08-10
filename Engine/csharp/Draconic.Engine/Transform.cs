using System.Numerics;
using System.Runtime.InteropServices;

namespace Draconic.Engine;

/// <summary>
/// Represents the transformation data for an entity, including its position, scale, and rotation.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Transform : IComponent
{
    /// <summary>
    /// Represents the position of a transform in 3D space.
    /// </summary>
    public Vector3 Position { get; set; }

    /// <summary>
    /// Represents the scaling factor applied to an object in 3D space.
    /// </summary>
    public Vector3 Scale { get; set; }

    /// <summary>
    /// Represents the rotational orientation of the transform in 3D space.
    /// </summary>
    public Quaternion Rotation { get; set; }

}