English version: 

[README.MD](README.MD)

# Gameplay Message Router (UGC 增强版)

该项目基于 Unreal Engine 的 Lyra 项目([GameplayMessageRouter](https://github.com/EpicGames/UnrealEngine/tree/ue5-main/Samples/Games/Lyra/Plugins/GameplayMessageRouter))进行二次开发，并参考了 Minecraft 消息系统的设计理念进行改进。系统针对用户生成内容（UGC）的工作流程进行了优化，旨在实现独立游戏对象之间直观且灵活的信息传递，同时采用统一的消息结构体作为键值用于发送与监听。这样不仅大幅简化了开发流程（因为发送和接收均使用相同的数据结构），还保留了基于 GameplayTag 的频道过滤功能，实现了更好的兼容性与细致控制。

## 核心系统

### GameplayMessageSubsystem (全局消息系统)
传统的消息系统，处理全局和对象专属的通信：

- **结构化消息系统：**  
  通过使用消息结构体类型作为唯一键来发送和接收消息，使得发送方与接收方基于同一数据结构工作，同时依然支持基于 GameplayTag 的频道过滤功能。
- **对象专属消息：**  
  可以针对特定对象发送消息，从而确保只有与该对象相关的监听器接收到数据，非常适用于局部或特定场景的事件处理。
- **优先级处理：**  
  在注册监听器时可以指定优先级，确保高优先级的监听器能够先于其他监听器处理消息，避免因执行顺序不当而引发意外行为。
- **取消机制：**  
  监听器在回调中可以取消当前消息的执行，并可选择阻断后续监听器的调用，消息发送方也能获知该消息是否被取消。

### GameplayWorldMessageSubsystem (空间消息系统)
先进的空间消息系统，支持基于位置的通信：

- **基于坐标的广播：**  
  消息在特定的世界坐标处广播，实现游戏对象之间位置感知的通信。
- **基于半径的监听：**  
  监听器可以注册接收其监听位置指定半径内的消息，实现基于距离的交互。
- **网格优化性能：**  
  使用内部网格系统（16m x 16m 单元格）高效管理空间查询，最小化消息广播时的性能开销。
- **动态监听器更新：**  
  支持在运行时更新监听器位置和半径，非常适合需要保持空间感知的移动对象。
- **完整核心功能：**  
  保持全局系统的所有优势，包括结构化消息、优先级处理和取消机制。

## 示例

只需将本插件复制到您项目的 `Plugins` 文件夹中（例如：`YourProject/Plugins/GameplayMessageRouter`），然后重新生成项目文件，即可开始使用增强版 Gameplay Message Router。支持蓝图与C++项目。

## 全局消息系统示例

1. **全局消息发送**

  - Message Notification (Simplified Version)

    - C++实现

    ```cpp
    // 模板参数 <FFireModeStandardMessage_AfterDamage> 的结构提类型用作消息的 Key。
    // 第一个参数为消息体（携带具体数据）。
    // 第二个参数为目标对象（可选），未传时该消息为全局消息（所有 Listener 均可收到）。
    
    // Broadcast world message
    MessageSubsystem.BroadcastSimpleMessage<FFireModeStandardMessage_AfterDamage>(
        FireModeStandardMessage_AfterDamage
    );
    
    // Only the projectile shooter can receive the message
    MessageSubsystem.BroadcastSimpleMessage<FFireModeStandardMessage_AfterDamage>(
        FireModeStandardMessage_AfterDamage,
        BasicProjectile->ProjectileInstigator
    );
    ```

    - Blueprint实现

    ![1_SimpleMessageSample](./imgs/1_SimpleMessageSample.jpg)

  - Message Notification (Full Version – retains Lyra's native Channel support)

    - C++实现

    ```cpp
    // Broadcast world message
    MessageSubsystem.BroadcastMessage<FFireModeStandardMessage_AfterDamage>(
        FireModeStandardMessage_AfterDamage,
        FGameplayTag::EmptyTag
    );
    
    // Only the projectile shooter can receive the message
    MessageSubsystem.BroadcastMessage<FFireModeStandardMessage_AfterDamage>(
        FireModeStandardMessage_AfterDamage,
        FGameplayTag::EmptyTag,
        BasicProjectile->ProjectileInstigator
    );
    ```

    - Blueprint实现

    ![2_SimpleMessageSample](./imgs/2_MessageSample.jpg)

2. **全局消息监听**

  - Message Listener (Simplified Version)

    - C++实现

    ```cpp
    // 模板参数 <FGameplayShipEnterEventData> 的结构提类型用作消息的 Key。
    // 第一个参数为回调函数，当消息触发时调用。
    // 第二个参数指定了监听器的优先级，本例中为 EGameplayMessagePriority::HIGHER，确保该回调优先执行。
    // 第三个参数为目标对象（可选），未传时该消息为全局消息（所有 Listener 均可收到）。
    
    // Listen world message
    MessageSubsystem.RegisterListener<FGameplayShipEnterEventData>(
        [WeakThis](FGameplayTag Channel, const FGameplayShipEnterEventData& Event) {
            // gameplay logic ...
        },
        EGameplayMessagePriority::HIGHER
    );
    
    // Only listener the target ship message
    MessageSubsystem.RegisterListener<FGameplayShipEnterEventData>(
        [WeakThis](FGameplayTag Channel, const FGameplayShipEnterEventData& Event) {
            // gameplay logic ...
        },
        EGameplayMessagePriority::HIGHER,
        GetOwner()
    );
    ```

    - Blueprint实现

    ![3_SimpleListenMessageSample](./imgs/3_SimpleListenMessageSample.jpg)

  - Message Listener (Full Version – retains Lyra's native Channel support)

    - C++实现

    ```cpp
    // Listen world message
    MessageSubsystem.RegisterListener<FGameplayShipEnterEventData>(
        FGameplayTag::RequestGameplayTag("Message"),
        [WeakThis](FGameplayTag Channel, const FGameplayShipEnterEventData& Event) {
            // gameplay logic ...
        },
        EGameplayMessageMatch::ExactMatch,
        EGameplayMessagePriority::HIGHER
    );
    
    // Only listener the target ship message
    MessageSubsystem.RegisterListener<FGameplayShipEnterEventData>(
        FGameplayTag::RequestGameplayTag("Message"),
        [WeakThis](FGameplayTag Channel, const FGameplayShipEnterEventData& Event) {
            // gameplay logic ...
        },
        EGameplayMessageMatch::ExactMatch,
        EGameplayMessagePriority::HIGHER,
        GetOwner()
    );
    ```

    - Blueprint实现

    ![4_ListenMessageSample](./imgs/4_ListenMessageSample.jpg)

3. **全局消息取消**

    - C++实现

    ```cpp
    // Cancel current message in context
    // 取消当前消息上下文的处理：
    // 第一个参数 true 表示取消消息，发送方可以通过该标记检测到消息已被取消。
    // 第二个参数 true 表示在取消消息后阻断所有后续 Listener 的执行，后续监听器将不会再收到该消息。
    MessageSubsystem.CancelMessage(true, true);
    ```

    - Blueprint实现

    ![5_CancelMessage](./imgs/5_CancelMessage.jpg)

## 空间消息系统示例

空间消息系统支持基于位置的通信，非常适合基于距离的交互、区域效果和本地通知。

1. **空间消息发送**

  - 空间消息通知 (简化版本)

    - C++实现

    ```cpp
        // 获取世界消息子系统
        UGameplayWorldMessageSubsystem& WorldMessageSubsystem = UGameplayWorldMessageSubsystem::Get(this);
        
        // 模板参数 <FExplosionEventData> 的结构体类型用作消息的 Key。
        // 第一个参数为消息体（携带具体数据如伤害、半径等）。
        // 第二个参数为消息广播的世界位置。
        FExplosionEventData ExplosionData;
        ExplosionData.Damage = 100.0f;
        ExplosionData.ExplosionRadius = 500.0f;
        ExplosionData.Instigator = GetOwner();
        
        // 在爆炸位置广播空间消息
        WorldMessageSubsystem.BroadcastSimpleMessage<FExplosionEventData>(ExplosionData, GetActorLocation());
    ```

    - Blueprint实现

    使用 "Broadcast Simple Spatial Message" 节点，配合消息结构体和世界位置。

  - 空间消息通知 (完整版本 – 支持频道)

    - C++实现

    ```cpp
        // 使用特定频道广播空间消息
        FGameplayTag ExplosionChannel = FGameplayTag::RequestGameplayTag("Combat.Explosion");
        WorldMessageSubsystem.BroadcastMessage<FExplosionEventData>(ExplosionData, ExplosionChannel, GetActorLocation());
    ```

    - Blueprint实现

    使用 "Broadcast Spatial Message" 节点，配合频道、消息结构体和世界位置。

2. **空间消息监听**

  - 空间消息监听器 (简化版本)

    - C++实现

    ```cpp
        // 模板参数 <FExplosionEventData> 的结构体类型用作消息的 Key。
        // 第一个参数为回调函数，当消息触发时调用。
        // 第二个参数为监听位置（通常是Actor的位置）。
        // 第三个参数为监听半径（可以接收多远距离的消息）。
        // 第四个参数指定监听器优先级。
        
        FVector ListenPosition = GetActorLocation();
        float ListenRadius = 1000.0f; // 监听10米内的爆炸事件
        
        // 注册空间监听器
        FGameplayWorldMessageListenerHandle Handle = WorldMessageSubsystem.RegisterListener<FExplosionEventData>(
            [WeakThis](FGameplayTag Channel, const FExplosionEventData& Event)
            {
                if (auto* StrongThis = WeakThis.Get())
                {
                    // 处理爆炸事件 - 应用伤害、效果等
                    StrongThis->OnExplosionReceived(Event);
                }
            },
            ListenPosition,
            ListenRadius,
            EGameplayMessagePriority::HIGHER
        );
    ```

    - Blueprint实现

    使用 "Simple Listen For Gameplay World Messages" 异步节点，配合消息结构体、监听位置和半径。

  - 空间消息监听器 (完整版本 – 支持频道)

    - C++实现

    ```cpp
        // 监听特定频道的空间消息
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

    - Blueprint实现

    使用 "Listen For Gameplay World Messages" 异步节点，配合频道、消息结构体、监听位置和半径。

3. **动态监听器更新**

    - C++实现

    ```cpp
        // 当Actor移动时更新监听器位置
        // 这对需要保持空间感知的移动对象很有用
        
        FVector NewPosition = GetActorLocation();
        float NewRadius = 1500.0f; // 可选择更新监听半径
        
        // 更新现有监听器位置
        bool bSuccess = WorldMessageSubsystem.UpdateRegisterListenerLocation(ListenerHandle, NewPosition, NewRadius);
        
        if (!bSuccess)
        {
            UE_LOG(LogTemp, Warning, TEXT("更新监听器位置失败"));
        }
    ```

4. **空间消息取消**

    - C++实现

    ```cpp
        // 取消当前空间消息上下文（与全局系统相同）
        // 第一个参数 true 表示取消消息，发送方可以检测到此取消标志。
        // 第二个参数 true 表示在取消后阻断所有后续监听器的执行。
        WorldMessageSubsystem.CancelMessage(true, true);
    ```

    - Blueprint实现

    使用 "Cancel Message" 节点（与全局系统相同）。

## 技术实现细节

### 网格系统架构

空间消息系统使用优化的基于网格的方法来提高性能：

- **网格大小**: 每个网格单元覆盖 16m x 16m 区域（1600 UE 单位）
- **网格ID编码**: 使用64位整数，高32位 = X坐标，低32位 = Y坐标
- **监听器注册**: 注册监听器时，会自动添加到与其监听半径相交的所有网格单元中
- **消息广播**: 只搜索包含广播位置的单个网格单元，然后对该单元中的监听器执行距离检查
- **动态更新**: 当监听器位置或半径发生变化时，高效地在网格单元之间移动监听器

### 性能考虑

1. **最佳监听半径**: 保持合理的监听半径（通常为500-2000 UE单位）以最小化网格单元重叠
2. **监听器更新**: 对于移动对象，使用 `UpdateRegisterListenerLocation()` 而不是注销后重新注册
3. **消息频率**: 系统针对中等消息频率进行了优化；避免每帧广播数百条空间消息
4. **网格对齐**: 16m的网格大小适用于大多数游戏场景；如需不同的网格大小请联系开发者

## 最佳实践指南

### 消息结构设计
- 建议采用合理的对象继承体系来组织消息结构
- 通过基类定义共用属性，避免子类消息中的重复字段定义
- 保持消息结构的清晰性和可维护性

### 全局 vs 空间消息

**使用全局消息的场景：**
- UI通知和状态变化
- 游戏模式事件和转换
- 玩家进度和成就
- 系统级配置变更

**使用空间消息的场景：**
- 战斗事件（爆炸、枪声、撞击）
- 环境交互（门开启、开关激活）
- 基于距离的通知（进入/离开区域）
- 本地多人游戏事件

### 消息对象粒度选择
- 对于全局消息，建议以角色(Character)或Monster(Pawn)级别作为消息对象粒度
- 对于空间消息，考虑实际交互范围和游戏需求
- 避免使用过于宽泛的全局消息：
  * 可能导致消息处理逻辑被非预期对象干扰
  * 增加系统复杂度和调试难度
- 避免过细粒度(如单个子弹)作为消息对象：
  * 会降低消息处理逻辑的复用性
  * 可能导致相似功能代码的重复实现

### 优先级规范
推荐使用统一规范来定义优先级，避免随意插入高优先逻辑导致业务顺序难以维护。

| 优先级 | 使用场景 | 示例 |
|--------|----------|------|
| HIGHEST | 关键流程控制 | 行为限制、状态锁定 |
| HIGHER | 核心业务逻辑 | 属性增益、被动技能 |
| DEFAULT | 常规业务逻辑 | 标准功能实现 |
| LOWER | 次要业务处理 | 后置数值计算 |
| LOWEST | 收尾处理逻辑 | 状态清理、资源回收 |
| MONITOR | 系统监控功能 | 反作弊检测、日志记录 |

### 消息取消
  - 消息系统中的取消逻辑只起到标记作用，消息发送方需要自行实现具体的取消处理逻辑。处理逻辑包括：
    * 检测到技能释放被取消时，终止技能施放流程
    * 正确处理相关资源（如技能CD、消耗品等）的回退操作
    * 确保系统状态的一致性
  - 例如：开火逻辑检测到开火小区被取消后，需要取消本次开火操作，并正确处理CD等数值流程。

### 空间消息最佳实践

1. **监听半径指南**
   - 战斗事件：500-1000 UE单位（5-10米）
   - 环境声音：1000-3000 UE单位（10-30米）
   - 区域通知：2000-5000 UE单位（20-50米）
   - 避免极大的半径（>10000单位），因为这会降低性能优势

2. **移动对象**
   - 对于频繁移动的对象，始终使用 `UpdateRegisterListenerLocation()`
   - 考虑为变为非活跃或被销毁的对象注销监听器
   - 对于快速移动的对象，考虑每几帧更新一次监听器位置，而不是每个tick

3. **消息频率**
   - 尽可能批处理相似消息（例如，将多个伤害事件合并为一个）
   - 使用适当的优先级别确保关键消息优先处理
   - 考虑为频繁创建的消息结构体使用对象池

## License

本项目基于 Epic Games 的 Lyra 示例项目进行开发。根据 [Unreal Engine EULA](https://www.unrealengine.com/eula)，您可以自由使用和修改本项目用于开发您的游戏。

如果您计划使用或修改本项目，请确保遵守:
1. Unreal Engine 最终用户许可协议 (EULA)
2. Epic Games 的知识产权政策