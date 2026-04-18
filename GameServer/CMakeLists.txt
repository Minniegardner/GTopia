cmake_minimum_required(VERSION 3.20)
project(GameServer)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /D_NDEBUG /O2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /D_NDEBUG /O2")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D_DEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_DEBUG")
else()
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -D_NDEBUG -O2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_NDEBUG -O2")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
endif()

add_definitions(-DSERVER_GAME)

include(${CMAKE_CURRENT_LIST_DIR}/../CMake/Source.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../CMake/FindMySQL.cmake)

set(GAME_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(GAME_PLAYER "${GAME_ROOT}/Player")
set(GAME_SERVER "${GAME_ROOT}/Server")
set(GAME_EVENT "${GAME_ROOT}/Event")
set(GAME_WORLD "${GAME_ROOT}/World")
set(GAME_COMMAND "${GAME_ROOT}/Command")
set(GAME_DIALOG "${GAME_PLAYER}/Dialog")
set(GAME_STORE "${GAME_ROOT}/Store")
set(GAME_PRIZE "${GAME_ROOT}/Prize")

set(GAME_FILES PRIVATE
    ${SOURCE_DATABASE}/Table/PlayerDBTable.cpp
    ${SOURCE_DATABASE}/Table/WorldDBTable.cpp
    ${SOURCE_UTILS}/GameConfig.cpp
    ${SOURCE_UTILS}/DialogBuilder.cpp

    ${SOURCE_PACKET}/NetPacket.cpp

    ${SOURCE_ITEM}/ItemInfo.cpp
    ${SOURCE_ALGORITHM}/LockAlgorithm.cpp
    ${SOURCE_ALGORITHM}/ChemsynthAlgorithm.cpp
    ${SOURCE_ALGORITHM}/GhostAlgorithm.cpp
    ${SOURCE_ITEM}/ItemInfoManager.cpp
    ${SOURCE_ITEM}/ItemUtils.cpp

    ${GAME_EVENT}/TCP/TCPEventHello.cpp
    ${GAME_EVENT}/TCP/TCPEventAuth.cpp
    ${GAME_EVENT}/TCP/TCPEventPlayerSession.cpp
    ${GAME_EVENT}/TCP/TCPEventWorldInit.cpp
    ${GAME_EVENT}/TCP/TCPEventRenderWorld.cpp
    ${GAME_EVENT}/TCP/TCPEventWorldSendPlayer.cpp
    ${GAME_EVENT}/TCP/TCPEventCrossServerAction.cpp
    ${GAME_EVENT}/TCP/TCPEventHeartBeat.cpp
    ${GAME_EVENT}/TCP/TCPEventKillServer.cpp

    ${GAME_EVENT}/UDP/GameMessage/RefreshItemData.cpp
    ${GAME_EVENT}/UDP/GameMessage/EnterGame.cpp
    ${GAME_EVENT}/UDP/GameMessage/RefreshTributeData.cpp
    ${GAME_EVENT}/UDP/GameMessage/JoinRequest.cpp
    ${GAME_EVENT}/UDP/GameMessage/Input.cpp
    ${GAME_EVENT}/UDP/GameMessage/QuitToExit.cpp
    ${GAME_EVENT}/UDP/GameMessage/DialogReturn.cpp
    ${GAME_EVENT}/UDP/GameMessage/Trash.cpp
    ${GAME_EVENT}/UDP/GameMessage/Drop.cpp
    ${GAME_EVENT}/UDP/GameMessage/SetSkin.cpp
    ${GAME_EVENT}/UDP/GameMessage/Respawn.cpp
    ${GAME_EVENT}/UDP/GameMessage/MenuClickSocial.cpp
    ${GAME_EVENT}/UDP/GameMessage/MenuClickInfo.cpp
    ${GAME_EVENT}/UDP/GameMessage/MenuClickFavorite.cpp
    ${GAME_EVENT}/UDP/GameMessage/MenuClickLeaderboards.cpp
    ${GAME_EVENT}/UDP/GameMessage/MenuClickSecure.cpp
    ${GAME_EVENT}/UDP/GameMessage/MenuClickRecycle.cpp
    ${GAME_EVENT}/UDP/GameMessage/Wrench.cpp
    ${GAME_EVENT}/UDP/GameMessage/Store.cpp
    ${GAME_EVENT}/UDP/GameMessage/StoreNavigate.cpp
    ${GAME_EVENT}/UDP/GameMessage/Buy.cpp
    ${GAME_EVENT}/UDP/GameMessage/GrowID.cpp

    ${GAME_EVENT}/UDP/GamePacket/ItemActivateRequest.cpp
    ${GAME_EVENT}/UDP/GamePacket/PlayerCollect.cpp
    ${GAME_EVENT}/UDP/GamePacket/UseDoorRequest.cpp
    ${GAME_EVENT}/UDP/GamePacket/PlayerSendIconState.cpp
    ${GAME_EVENT}/UDP/GamePacket/BlockActivate.cpp
    ${GAME_EVENT}/UDP/GamePacket/PlayerPunched.cpp
    ${GAME_EVENT}/UDP/GamePacket/TileChangeRequest.cpp
    ${GAME_EVENT}/UDP/GamePacket/State.cpp

    ${GAME_COMMAND}/RenderWorld.cpp
    ${GAME_COMMAND}/GiveItem.cpp
    ${GAME_COMMAND}/Ghost.cpp
    ${GAME_COMMAND}/SpawnGhost.cpp
    ${GAME_COMMAND}/TogglePlayMod.cpp
    ${GAME_COMMAND}/Magic.cpp
    ${GAME_COMMAND}/Trade.cpp
    ${GAME_COMMAND}/Help.cpp
    ${GAME_COMMAND}/Kick.cpp
    ${GAME_COMMAND}/Pull.cpp
    ${GAME_COMMAND}/Mod.cpp
    ${GAME_COMMAND}/Find.cpp
    ${GAME_COMMAND}/Warp.cpp
    ${GAME_COMMAND}/Who.cpp
    ${GAME_COMMAND}/Online.cpp
    ${GAME_COMMAND}/Time.cpp
    ${GAME_COMMAND}/News.cpp
    ${GAME_COMMAND}/Stats.cpp
    ${GAME_COMMAND}/Ping.cpp
    ${GAME_COMMAND}/Msg.cpp
    ${GAME_COMMAND}/Reply.cpp
    ${GAME_COMMAND}/Ban.cpp
    ${GAME_COMMAND}/RandomPull.cpp
    ${GAME_COMMAND}/Warn.cpp
    ${GAME_COMMAND}/WarpTo.cpp
    ${GAME_COMMAND}/Uba.cpp
    ${GAME_COMMAND}/KickAll.cpp
    ${GAME_COMMAND}/Summon.cpp
    ${GAME_COMMAND}/Suspend.cpp
    ${GAME_COMMAND}/PBan.cpp
    ${GAME_COMMAND}/GrowIDCmd.cpp
    ${GAME_COMMAND}/SuperBroadcast.cpp
    ${GAME_COMMAND}/Weather.cpp
    ${GAME_COMMAND}/Effect.cpp
    ${GAME_COMMAND}/SetGems.cpp
    ${GAME_COMMAND}/PInfo.cpp
    ${GAME_COMMAND}/InvSee.cpp
    ${GAME_COMMAND}/ItemInfo.cpp
    ${GAME_COMMAND}/ReplaceBlocks.cpp
    ${GAME_COMMAND}/RepairMagplantWorlds.cpp

    ${SOURCE_WORLD}/TileExtra.cpp
    ${SOURCE_WORLD}/TileInfo.cpp
    ${SOURCE_WORLD}/WorldInfo.cpp
    ${SOURCE_WORLD}/WorldTileManager.cpp
    ${SOURCE_WORLD}/WorldObjectManager.cpp
    ${GAME_WORLD}/WorldManager.cpp
    ${GAME_WORLD}/WorldPathfinding.cpp
    ${GAME_WORLD}/World.cpp

    ${GAME_DIALOG}/PlayerDialog.cpp
    ${GAME_DIALOG}/SignDialog.cpp
    ${GAME_DIALOG}/TrashDialog.cpp
    ${GAME_DIALOG}/DropDialog.cpp
    ${GAME_DIALOG}/LockDialog.cpp
    ${GAME_DIALOG}/DoorDialog.cpp
    ${GAME_DIALOG}/MachineDialog.cpp
    ${GAME_DIALOG}/ChemsynthDialog.cpp
    ${GAME_DIALOG}/RenderWorldDialog.cpp
    ${GAME_DIALOG}/RegisterDialog.cpp

    ${GAME_STORE}/StoreCatalog.cpp

    ${GAME_PRIZE}/PrizeManager.cpp

    ${SOURCE_PLAYER}/Role.cpp
    ${SOURCE_PLAYER}/RoleManager.cpp
    ${SOURCE_PLAYER}/Player.cpp
    ${SOURCE_PLAYER}/PlayerTribute.cpp
    ${SOURCE_PLAYER}/PlayerInventory.cpp
    ${SOURCE_PLAYER}/PlayerLoginDetail.cpp
    ${SOURCE_PLAYER}/CharacterData.cpp
    ${SOURCE_PLAYER}/PlayMod.cpp
    ${SOURCE_PLAYER}/PlayModManager.cpp
    ${GAME_PLAYER}/AchievementCatalog.cpp
    ${GAME_PLAYER}/GamePlayer.cpp

    ${SOURCE_SERVER}/ServerBase.cpp
    ${SOURCE_SERVER}/ServerBroadwayBase.cpp
    ${GAME_SERVER}/GameServer.cpp
    ${GAME_SERVER}/MasterBroadway.cpp

    ${GAME_ROOT}/Context.cpp
    ${GAME_ROOT}/Main.cpp
)

add_executable(GameServer)
add_default_sources(GameServer)
add_database_sources(GameServer)
add_network_sources(GameServer)
target_sources(GameServer PRIVATE ${GAME_FILES})

set_target_properties(GameServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/Runtime"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Runtime"
)

target_include_directories(GameServer PRIVATE
    ${SOURCE_ROOT}
    ${GAME_ROOT}
    ${MYSQL_INCLUDE_DIR}
)

target_link_libraries(GameServer PRIVATE
    ${MYSQL_LIB}
)

if(WIN32)
    target_link_libraries(GameServer PRIVATE
        winmm ws2_32
    )
    target_compile_definitions(GameServer PRIVATE WIN32_LEAN_AND_MEAN)
endif(WIN32)

copy_dll(GameServer "${MYSQL_LIB_DIR}/libmysql.dll")