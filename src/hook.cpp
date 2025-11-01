#include "ll/api/memory/Hook.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerUseItemEvent.h"
#include "ll\api\event\player\PlayerInteractBlockEvent.h"
#include "logger.h"
#include "mc\world\level\block/LeavesBlock.h"
#include "mc\world\level\block\LogBlock.h"
#include "mc\world\level\block\block_events\BlockRandomTickEvent.h"

#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/BlockType.h"
#include "mc\world\level\block/Block.h"
#include "mc\world\level\block\states\vanilla_states\VanillaStates.h"
#include <utility>
#include <queue> 
#include <set>   
#include <vector> 


#include "ll/api/service/Bedrock.h"
#include "mc/world/level/Level.h"

/*
void registerEvent() {
    ll::event::EventBus::getInstance().emplaceListener<ll::event::player::PlayerInteractBlockEvent>(
        [](ll::event::player::PlayerInteractBlockEvent& ev) {
            auto& player = ev.self();
            auto  block  = ev.block();
            player.sendMessage("You interacted with block: " + block->getTypeName());

            auto persistentBit = block->getState<bool>(VanillaStates::PersistentBit());
            auto updateBit     = block->getState<bool>(VanillaStates::UpdateBit());

            std::string message = "Block: " + block->getTypeName();
            if (persistentBit.has_value()) {
                message += ", PersistentBit: " + std::string(persistentBit.value() ? "true" : "false");
            }
            if (updateBit.has_value()) {
                message += ", UpdateBit: " + std::string(updateBit.value() ? "true" : "false");
            }
            player.sendMessage(message);
        }
    );
}
*/
namespace my_mod {

// 辅助函数：计算树叶方块到最近原木的距离 (BFS)
int calculateDistanceToLog(::BlockSource& region, ::BlockPos const& startPos, ::BlockPos const& removedLogPos) {
    std::queue<std::pair<::BlockPos, int>> q;
    std::set<::BlockPos> visited;

    q.push({startPos, 0});
    visited.insert(startPos);

    // 定义6个方向的偏移量
    std::vector<::BlockPos> directions = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };

    while (!q.empty()) {
        auto [currentPos, dist] = q.front();
        q.pop();

        if (dist > 6) {
            continue; // 距离超过6，不再继续搜索
        }

        auto& currentBlock = region.getBlock(currentPos);
        std::string currentBlockName = currentBlock.getTypeName();

        if (currentBlockName.find("log") != std::string::npos) {
            // 找到原木，但要排除刚刚被移除的原木方块本身
            if (currentPos == removedLogPos) {
                // 如果找到的是被移除的原木，则不能算作支撑，继续搜索
            } else {
                return dist; // 找到有效的原木支撑
            }
        }

        // 遍历相邻方块
        for (const auto& dir : directions) {
            ::BlockPos nextPos = {currentPos.x + dir.x, currentPos.y + dir.y, currentPos.z + dir.z};

            if (visited.find(nextPos) == visited.end()) {
                auto& nextBlock = region.getBlock(nextPos);
                std::string nextBlockName = nextBlock.getTypeName();

                // 只有树叶方块和原木方块可以传播距离
                if (nextBlockName.find("leaves") != std::string::npos || nextBlockName.find("log") != std::string::npos) {
                    visited.insert(nextPos);
                    q.push({nextPos, dist + 1});
                }
            }
        }
    }

    return 7; // 没有找到原木，返回距离7 (表示腐烂)
}

/*
LL_AUTO_TYPE_INSTANCE_HOOK(
    hookadsasd,
    HookPriority::Normal,
    LeavesBlock,
    &LeavesBlock::$onRemove,
    void,
    ::BlockSource&    region,
    ::BlockPos const& pos
) {
    logger.info("onRemove");
    logger.info("pos: x={}, y={}, z={}", pos.x, pos.y, pos.z);
    origin(region, pos);
}
*/

LL_AUTO_TYPE_INSTANCE_HOOK(
    hookadsasd2,
    HookPriority::Normal,
    LogBlock,
    &LogBlock::$onRemove,
    void,
    ::BlockSource&    region,
    ::BlockPos const& pos
) {
    //logger.info("onLogBlock: pos=({},{},{})", pos.x, pos.y, pos.z);

    // 遍历以当前被移除的原木方块为中心，6个方块范围内的所有树叶方块
    for (int x = -6; x <= 6; ++x) {
        for (int y = -6; y <= 6; ++y) {
            for (int z = -6; z <= 6; ++z) {
                ::BlockPos currentLeavesPos = {pos.x + x, pos.y + y, pos.z + z};
                auto&      currentBlock     = region.getBlock(currentLeavesPos);
                std::string currentBlockName = currentBlock.getTypeName();

                // 检查是否是树叶方块
                if (currentBlockName == "minecraft:oak_leaves" || currentBlockName.find("leaves") != std::string::npos) {
                    //logger.info("  Found leaves at ({},{},{}): {}", currentLeavesPos.x, currentLeavesPos.y, currentLeavesPos.z, currentBlockName);

                    // 获取PersistentBit状态
                    auto persistentBit = currentBlock.getState<bool>(VanillaStates::PersistentBit());
                    if (persistentBit.has_value() && persistentBit.value()) {
                        //logger.info("    Leaves are persistent, skipping.");
                        continue; // 如果是持久树叶，则跳过
                    }
                    //logger.info("    Leaves are NOT persistent.");

                    // 计算该树叶方块到最近原木的距离
                    int distance = calculateDistanceToLog(region, currentLeavesPos, pos);

                    if (distance > 6) {
                       // logger.info("    No nearby log found (distance > 6) for leaves at ({},{},{}), decaying...", currentLeavesPos.x, currentLeavesPos.y, currentLeavesPos.z);
                        // 没有找到原木，执行腐烂逻辑
                        auto& leavesBlock = static_cast<const LeavesBlock&>(currentBlock.getBlockType());
                        leavesBlock._die(region, currentLeavesPos);
                        ll::service::getLevel()->destroyBlock(region, currentLeavesPos, true);
                    } else {
                        //logger.info("    Nearby log found (distance <= 6) for leaves at ({},{},{}), NOT decaying. Distance: {}", currentLeavesPos.x, currentLeavesPos.y, currentLeavesPos.z, distance);
                    }
                }
            }
        }
    }
    origin(region, pos);
}

/*
LL_AUTO_STATIC_HOOK(
    hook44,
    ll::memory::HookPriority::Normal,
    &LeavesBlock::runDecay,
    void,
    ::BlockSource&    region,
    ::BlockPos const& pos,
    int               range
) {
    logger.info("runDecay");
    logger.info("range: {}", range);
    logger.info("pos: x={}, y={}, z={}", pos.x, pos.y, pos.z);


    origin(region, pos, range);
}
    */
} // namespace my_mod
