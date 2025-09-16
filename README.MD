# 中文版本：

[README_CN.MD](README_CN.MD)

# Gameplay Message Router (UGC Enhanced Version)

This project is based on Unreal Engine's Lyra project ([GameplayMessageRouter](https://github.com/EpicGames/UnrealEngine/tree/ue5-main/Samples/Games/Lyra/Plugins/GameplayMessageRouter)) and has been enhanced with design concepts inspired by Minecraft's messaging system. The system is optimized for User Generated Content (UGC) workflows, aiming to achieve intuitive and flexible communication between independent game objects while using unified message structs as keys for both sending and listening. This approach significantly simplifies the development process (as sending and receiving use the same data structure) while retaining GameplayTag-based channel filtering for better compatibility and fine-grained control. 

## Core Systems

### GameplayMessageSubsystem (Global Messaging)
The traditional message system that handles global and object-specific communication:

- **Structured Message System:**  
  Uses message struct types as unique keys for sending and receiving messages, allowing both sender and receiver to work with the same data structure while maintaining support for GameplayTag-based channel filtering.
- **Object-Specific Messages:**  
  Enables sending messages to specific objects, ensuring only listeners related to that object receive the data, perfect for local or specific scenario event handling.
- **Priority Handling:**  
  Allows specifying priority when registering listeners, ensuring high-priority listeners process messages before others, preventing unexpected behaviors due to execution order.
- **Cancellation Mechanism:**  
  Listeners can cancel current message execution in callbacks and optionally block subsequent listener calls, with message senders able to detect if their message was cancelled.

### GameplayWorldMessageSubsystem (Spatial Messaging)
An advanced spatial messaging system that enables location-based communication:

- **Coordinate-Based Broadcasting:**  
  Messages are broadcast at specific world coordinates, allowing for location-aware communication between game objects.
- **Radius-Based Listening:**  
  Listeners can register to receive messages within a specified radius from their listening position, enabling proximity-based interactions.
- **Grid-Optimized Performance:**  
  Uses an internal grid system (16m x 16m cells) to efficiently manage spatial queries and minimize performance overhead during message broadcasting.
- **Dynamic Listener Updates:**  
  Supports updating listener positions and radii at runtime, perfect for moving objects that need to maintain spatial awareness.
- **All Core Features:**  
  Maintains all the benefits of the global system including structured messaging, priority handling, and cancellation mechanisms.

## Examples

Simply copy this plugin to your project's `Plugins` folder (e.g., `YourProject/Plugins/GameplayMessageRouter`) and regenerate project files to start using the enhanced Gameplay Message Router. Supports both Blueprint and C++ projects.

## Global Message System Examples

1. **Global Message Sending**

  - Message Notification (Simplified Version)

    - C++ Implementation

    ```cpp
        // Template parameter <FFireModeStandardMessage_AfterDamage> struct type serves as the message key.
        // First parameter is the message body (carrying specific data).
        // Second parameter is the target object (optional), when not passed, the message is global (all Listeners can receive).
        // Broadcast world message
        MessageSubsystem.BroadcastSimpleMessage<FFireModeStandardMessage_AfterDamage>(FireModeStandardMessage_AfterDamage);
        // Only the projectile shooter can receive the message
        MessageSubsystem.BroadcastSimpleMessage<FFireModeStandardMessage_AfterDamage>(FireModeStandardMessage_AfterDamage, BasicProjectile->ProjectileInstigator);
    ```

    - Blueprint Implementation

    ![1_SimpleMessageSample](./imgs/1_SimpleMessageSample.jpg)

  - Message Notification (Full Version – retains Lyra's native Channel support)

    - C++ Implementation

    ```cpp
        // Broadcast world message
        MessageSubsystem.BroadcastMessage<FFireModeStandardMessage_AfterDamage>(FireModeStandardMessage_AfterDamage, FGameplayTag::EmptyTag);
        // Only the projectile shooter can receive the message
        MessageSubsystem.BroadcastMessage<FFireModeStandardMessage_AfterDamage>(FireModeStandardMessage_AfterDamage, FGameplayTag::EmptyTag, BasicProjectile->ProjectileInstigator);
    ```

    - Blueprint Implementation

    ![2_SimpleMessageSample](./imgs/2_MessageSample.jpg)

2. **Global Message Listening**

  - Message Listener (Simplified Version)

    - C++ Implementation

    ```cpp
        // Template parameter <FGameplayShipEnterEventData> struct type serves as the message key.
        // First parameter is the callback function, called when message triggers.
        // Second parameter specifies listener priority, in this case EGameplayMessagePriority::HIGHER, ensuring this callback executes first.
        // Third parameter is target object (optional), when not passed, listens to global messages.
        // Listen world message
        MessageSubsystem.RegisterListener<FGameplayShipEnterEventData>([WeakThis](FGameplayTag Channel, const FGameplayShipEnterEventData& Event)
            {
                // gameplay logic ...
            },
            EGameplayMessagePriority::HIGHER);
        // Only listener the target ship message
        MessageSubsystem.RegisterListener<FGameplayShipEnterEventData>([WeakThis](FGameplayTag Channel, const FGameplayShipEnterEventData& Event)
                {
                    // gameplay logic ...
                },
                EGameplayMessagePriority::HIGHER,
                GetOwner());
    ```

    - Blueprint Implementation

    ![3_SimpleListenMessageSample](./imgs/3_SimpleListenMessageSample.jpg)

  - Message Listener (Full Version – retains Lyra's native Channel support)

    - C++ Implementation

    ```cpp
        // Listen world message
        MessageSubsystem.RegisterListener<FGameplayShipEnterEventData>(FGameplayTag::RequestGameplayTag("Message"), [WeakThis](FGameplayTag Channel, const FGameplayShipEnterEventData& Event)
            {
                // gameplay logic ...
            },
            EGameplayMessageMatch::ExactMatch,
            EGameplayMessagePriority::HIGHER);
        // Only listener the target ship message
        MessageSubsystem.RegisterListener<FGameplayShipEnterEventData>(FGameplayTag::RequestGameplayTag("Message"), [WeakThis](FGameplayTag Channel, const FGameplayShipEnterEventData& Event)
                {
                    // gameplay logic ...
                },
                EGameplayMessageMatch::ExactMatch,
                EGameplayMessagePriority::HIGHER,
                GetOwner());
    ```

    - Blueprint Implementation

    ![4_ListenMessageSample](./imgs/4_ListenMessageSample.jpg)

3. **Global Message Cancellation**

    - C++ Implementation

    ```cpp
        // Cancel current message in context
        // First parameter true indicates cancelling the message, sender can detect this cancellation flag.
        // Second parameter true indicates blocking all subsequent Listener execution after cancellation.
        MessageSubsystem.CancelMessage(true, true);
    ```

    - Blueprint Implementation

    ![5_CancelMessage](./imgs/5_CancelMessage.jpg)

## Spatial Message System Examples

The Spatial Message System enables location-based communication, perfect for proximity-based interactions, area effects, and local notifications.

1. **Spatial Message Sending**

  - Spatial Message Notification (Simplified Version)

    - C++ Implementation

    ```cpp
        // Get the world message subsystem
        UGameplayWorldMessageSubsystem& WorldMessageSubsystem = UGameplayWorldMessageSubsystem::Get(this);
        
        // Template parameter <FExplosionEventData> struct type serves as the message key.
        // First parameter is the message body (carrying specific data like damage, radius, etc.).
        // Second parameter is the world position where the message is broadcast.
        FExplosionEventData ExplosionData;
        ExplosionData.Damage = 100.0f;
        ExplosionData.ExplosionRadius = 500.0f;
        ExplosionData.Instigator = GetOwner();
        
        // Broadcast spatial message at explosion location
        WorldMessageSubsystem.BroadcastSimpleMessage<FExplosionEventData>(ExplosionData, GetActorLocation());
    ```

    - Blueprint Implementation

    Use the "Broadcast Simple Spatial Message" node with your message struct and world position.

  - Spatial Message Notification (Full Version – with Channel support)

    - C++ Implementation

    ```cpp
        // Broadcast spatial message with specific channel
        FGameplayTag ExplosionChannel = FGameplayTag::RequestGameplayTag("Combat.Explosion");
        WorldMessageSubsystem.BroadcastMessage<FExplosionEventData>(ExplosionData, ExplosionChannel, GetActorLocation());
    ```

    - Blueprint Implementation

    Use the "Broadcast Spatial Message" node with channel, message struct, and world position.

2. **Spatial Message Listening**

  - Spatial Message Listener (Simplified Version)

    - C++ Implementation

    ```cpp
        // Template parameter <FExplosionEventData> struct type serves as the message key.
        // First parameter is the callback function, called when message triggers.
        // Second parameter is the listening position (usually the actor's location).
        // Third parameter is the listening radius (how far away messages can be received).
        // Fourth parameter specifies listener priority.
        
        FVector ListenPosition = GetActorLocation();
        float ListenRadius = 1000.0f; // Listen to explosions within 10 meters
        
        // Register spatial listener
        FGameplayWorldMessageListenerHandle Handle = WorldMessageSubsystem.RegisterListener<FExplosionEventData>(
            [WeakThis](FGameplayTag Channel, const FExplosionEventData& Event)
            {
                if (auto* StrongThis = WeakThis.Get())
                {
                    // Handle explosion event - apply damage, effects, etc.
                    StrongThis->OnExplosionReceived(Event);
                }
            },
            ListenPosition,
            ListenRadius,
            EGameplayMessagePriority::HIGHER
        );
    ```

    - Blueprint Implementation

    Use the "Simple Listen For Gameplay World Messages" async node with your message struct, listen position, and radius.

  - Spatial Message Listener (Full Version – with Channel support)

    - C++ Implementation

    ```cpp
        // Listen for spatial messages on specific channel
        FGameplayTag ExplosionChannel = FGameplayTag::RequestGameplayTag("Combat.Explosion");
        
        FGameplayWorldMessageListenerHandle Handle = WorldMessageSubsystem.RegisterListener<FExplosionEventData>(
            ExplosionChannel,
            [WeakThis](FGameplayTag Channel, const FExplosionEventData& Event)
            {
                if (auto* StrongThis = WeakThis.Get())
                {
                    StrongThis->OnExplosionReceived(Event);
                }
            },
            ListenPosition,
            ListenRadius,
            EGameplayMessageMatch::ExactMatch,
            EGameplayMessagePriority::HIGHER
        );
    ```

    - Blueprint Implementation

    Use the "Listen For Gameplay World Messages" async node with channel, message struct, listen position, and radius.

3. **Dynamic Listener Updates**

    - C++ Implementation

    ```cpp
        // Update listener position as the actor moves
        // This is useful for moving objects that need to maintain spatial awareness
        
        FVector NewPosition = GetActorLocation();
        float NewRadius = 1500.0f; // Optionally update the listening radius
        
        // Update existing listener location
        bool bSuccess = WorldMessageSubsystem.UpdateRegisterListenerLocation(ListenerHandle, NewPosition, NewRadius);
        
        if (!bSuccess)
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to update listener location"));
        }
    ```

4. **Spatial Message Cancellation**

    - C++ Implementation

    ```cpp
        // Cancel current spatial message in context (same as global system)
        // First parameter true indicates cancelling the message, sender can detect this cancellation flag.
        // Second parameter true indicates blocking all subsequent Listener execution after cancellation.
        WorldMessageSubsystem.CancelMessage(true, true);
    ```

    - Blueprint Implementation

    Use the "Cancel Message" node (same as global system).

## Technical Implementation Details

### Grid System Architecture

The Spatial Message System uses an optimized grid-based approach for performance:

- **Grid Size**: Each grid cell covers a 16m x 16m area (1600 UE units)
- **Grid ID Encoding**: Uses 64-bit integers where high 32 bits = X coordinate, low 32 bits = Y coordinate
- **Listener Registration**: When registering a listener, it's automatically added to all grid cells that intersect with its listening radius
- **Message Broadcasting**: Only searches the single grid cell containing the broadcast position, then performs distance checks on listeners in that cell
- **Dynamic Updates**: Efficiently moves listeners between grid cells when their position or radius changes

### Performance Considerations

1. **Optimal Listening Radius**: Keep listening radii reasonable (typically 500-2000 UE units) to minimize grid cell overlap
2. **Listener Updates**: Use `UpdateRegisterListenerLocation()` for moving objects rather than unregistering and re-registering
3. **Message Frequency**: The system is optimized for moderate message frequencies; avoid broadcasting hundreds of spatial messages per frame
4. **Grid Alignment**: The 16m grid size works well for most gameplay scenarios; contact the developers if you need different grid sizes

## Best Practices Guide

### Message Structure Design
- Recommend using a reasonable object inheritance system to organize message structures
- Define common properties through base classes to avoid duplicate field definitions in child message classes
- Maintain clarity and maintainability of message structures

### Global vs Spatial Messaging

**Use Global Messages for:**
- UI notifications and state changes
- Game mode events and transitions
- Player progression and achievements
- System-wide configuration changes

**Use Spatial Messages for:**
- Combat events (explosions, gunfire, impacts)
- Environmental interactions (door opening, switch activation)
- Proximity-based notifications (entering/leaving areas)
- Local multiplayer events

### Message Object Granularity Selection
- Recommend using Character or Monster (Pawn) level as message object granularity for global messages
- For spatial messages, consider the actual interaction range and gameplay requirements
- Avoid overly broad global messages:
  * May lead to message handling logic being interfered with by unexpected objects
  * Increases system complexity and debugging difficulty
- Avoid too fine granularity (like individual bullets) as message objects:
  * Reduces reusability of message handling logic
  * May lead to duplicate implementation of similar functionality

### Priority Standards
Recommend using unified standards to define priorities to avoid random insertion of high-priority logic making business order difficult to maintain.

| Priority | Usage Scenario | Example |
|----------|---------------|---------|
| HIGHEST | Critical Process Control | Behavior Restrictions, State Locking |
| HIGHER | Core Business Logic | Attribute Buffs, Passive Skills |
| DEFAULT | Regular Business Logic | Standard Feature Implementation |
| LOWER | Secondary Business Processing | Post-value Calculations |
| LOWEST | Cleanup Logic | State Cleanup, Resource Recovery |
| MONITOR | System Monitoring | Anti-cheat Detection, Logging |

### Message Cancellation
  - The cancellation logic in the message system only serves as a marker; message senders need to implement specific cancellation handling logic. Processing logic includes:
    * Terminating skill casting process when skill release is cancelled
    * Correctly handling related resource (like skill CD, consumables) rollback operations
    * Ensuring system state consistency
  - For example: When fire logic detects fire cancellation, it needs to cancel the current fire operation and correctly handle CD and other value processes.

### Spatial Message Best Practices

1. **Listening Radius Guidelines**
   - Combat events: 500-1000 UE units (5-10 meters)
   - Environmental sounds: 1000-3000 UE units (10-30 meters)
   - Area notifications: 2000-5000 UE units (20-50 meters)
   - Avoid extremely large radii (>10000 units) as they reduce performance benefits

2. **Moving Objects**
   - Always use `UpdateRegisterListenerLocation()` for objects that move frequently
   - Consider unregistering listeners for objects that become inactive or are destroyed
   - For rapidly moving objects, consider updating listener position every few frames rather than every tick

3. **Message Frequency**
   - Batch similar messages when possible (e.g., combine multiple damage events into one)
   - Use appropriate priority levels to ensure critical messages are processed first
   - Consider using object pools for frequently created message structs

## License

This project is developed based on Epic Games' Lyra sample project. According to the [Unreal Engine EULA](https://www.unrealengine.com/eula), you can freely use and modify this project to develop your games.

If you plan to use or modify this project, please ensure compliance with:
1. Unreal Engine End User License Agreement (EULA)
2. Epic Games' Intellectual Property Policy