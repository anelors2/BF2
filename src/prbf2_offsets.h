#pragma once
// prbf2_offsets.cpp
//
// Dump de offsets do Project Reality BF2 (build com arquivos de 08/05/2021).
//   PRBF2.exe    imagebase 0x00400000 -> enderecos absolutos (sem ASLR)
//   RendDX9.dll  imagebase 0x10000000 -> offsets relativos a base do modulo
//
// Legenda dos comentarios:
//   [IDA]      confirmado no disassembly/decompilacao
//   [live]     tambem conferido com o jogo rodando (coop, 30/09/2026)
//   [scripts]  tirado dos seus scripts em D:\0ct\B2\dump, nao reverificado no IDA
//   [obs]      so observado com o jogo rodando; significado nao confirmado no IDA
//
// Status dos testes com o jogo rodando (30/09/2026):
//   ok   player local, lista de players, time, vivo, posicao, tickets
//   ok   soldado -> arma na mao (slot 0) -> municao; municao desconta ao atirar (30 -> 27)
//   ok   veiculo: cada assento e um objeto; armas do assento em +0x1FC/+0x200 (motorista/piloto: -1 = infinita)
//   ok   passageiro que atira com a arma pessoal -> resolve para soldier + 0x1B8
//   ok   esqueleto 3P (soldier + 0x290): nomes, hierarquia e posicoes de mundo (bots deitado e agachado)
//   ok   esqueleto ativo (soldier + 0x2B8): bot = 1 (3P), jogador local em 1a pessoa = 0 (1P)
//   ok   player: nome, bot, vivo, esquadrao, lider; kit do soldado (soldier + 0x1F0 -> nome do template)
//   ok   vida do soldado (object + 0x3C -> Armor + 0x10 / template + 0xC8): muda com dano (bot 100 -> 13)
//   ok   RendDX9: base realocada, device, camera, olho, fov e ViewProj; cabeca projetada no centro da mira
//   ok   ObjectManager (44310 objetos), nomes de template de veiculo, vida de veiculo (jipes 1000/1000)
//   ok   postura (soldier + 0x230): 0 em pe, 1 agachado, 2 deitado
//   ok   veiculos: ocupantes, vida mudando (1000 -> 999), velocidade andando (fisica + 0x20)
//   --   nao testado: fisica 0x9179F8 (composta),
//        direcao (yaw),
//        pontuacao (score/kills/deaths; so vista zerada), caido, comandante,
//        ammo_template alem de magSize/nrOfMags, vtables de veiculo diferentes de 0x8F2430
//
// Uso:
//   - Referencia: so as constantes dos namespaces.
//   - Leitura: prbf2::Walker<Mem> com InternalMem (dentro do processo, DLL 32-bit)
//     ou ExternalMem (ReadProcessMemory, de fora).
//   - Demo externa, somente leitura:
//       cl /std:c++17 /EHsc /utf-8 /DPRBF2_OFFSETS_DEMO prbf2_offsets.cpp user32.lib
//     (rode como Administrador se o PRBF2.exe estiver elevado)

#include <windows.h>
#include <tlhelp32.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

namespace prbf2 {

using addr_t = std::uint32_t;  // o processo do jogo e 32-bit

// ============================================================================
// PRBF2.exe - globais
// ============================================================================
namespace exe {
constexpr addr_t kImageBase     = 0x00400000;
constexpr addr_t kPlayerManager = 0x00A08F64;  // [IDA][live] PlayerManager* ("dice.hfe.world.PlayerManager.Standard")
constexpr addr_t kGameLogic     = 0x009C5DA0;  // [scripts][live] singleton usado pelo tickets_trainer.ps1
constexpr addr_t kObjectManager = 0x00A08F58;  // [IDA][live] ObjectManager* ("dice.hfe.world.ObjectManager.Standard")
}  // namespace exe

// IDs passados para queryInterface (vtable +0x0C de qualquer objeto do mundo).
// O jogo guarda varios deles como constantes em .rdata (ex.: 50418 em 0x892DC8),
// por isso nao aparecem como imediatos no codigo.
namespace iid {
constexpr int kAmmo          = 50418;  // [IDA] IAmmo (componente de municao); 50349 tambem funciona
constexpr int kAmmoAlt       = 50349;
constexpr int kFireArm       = 38080;  // [IDA] interface da arma que o HUD usa (weapon + 0x170)
constexpr int kWeaponHandler = 50373;  // [IDA] gerenciador de armas do soldado/veiculo
constexpr int kSoldierHud    = 50391;  // [IDA] usado pelo HUD no soldado (soldier + 0x15C)
}  // namespace iid

// ID de tipo do template: object->template->vtbl[+0x10]() [IDA, registro em 0x537263]
namespace type_id {
constexpr int kSoldier              = 38035;
constexpr int kKit                  = 38038;
constexpr int kGenericFireArm       = 38099;
constexpr int kPlayerControlObject  = 50370;  // veiculos
constexpr int kDefaultAmmoComp      = 38112;
constexpr int kReplenishingAmmoComp = 38125;
}  // namespace type_id

// ============================================================================
// Vtables (PRBF2.exe) [IDA]
// ============================================================================
namespace vtable {
constexpr addr_t kPlayerManager        = 0x008AA5A8;
constexpr addr_t kPlayer               = 0x008AC530;
constexpr addr_t kSoldier              = 0x008E80F8;
constexpr addr_t kSoldierWeaponHandler = 0x008E7F80;  // soldier + 0x154
// Classes com gerenciador de armas em +0x154 e getWeapon = 0x5F67D0. So a PlayerControlObject e veiculo de
// verdade; as outras sao coisas que o soldado "usa" como veiculo:
//   0x8F2430 PlayerControlObject [live: carro, veiculo com piloto/co-piloto/passageiros; cada assento tambem]
//   0x8F2C48 Parachute, 0x8E9D88 LadderContainer, 0x8EC3D0 ZiplineContainer, 0x8EB9F8 GrapplingHookRopeContainer
constexpr addr_t kVehicles[] = {0x008E9D88, 0x008EB9F8, 0x008EC3D0, 0x008F2430, 0x008F2C48};
constexpr addr_t kPlayerControlObject  = 0x008F2430;
// Responde a queryInterface(50373) em +0x15C, mas getWeapon sempre retorna 0
constexpr addr_t kNoWeaponsObject      = 0x008FE7D0;
constexpr addr_t kArmor                = 0x0090B570;  // componente de vida (object + 0x3C)
constexpr addr_t kKitTemplate          = 0x008E5E48;  // ctor 0x596450
constexpr addr_t kKit                  = 0x008E5FB0;  // [obs] vtable do objeto em soldier + 0x1F0
constexpr addr_t kGenericFireArm       = 0x008ED768;
constexpr addr_t kFireArmIface         = 0x008ED558;  // weapon + 0x170
constexpr addr_t kDefaultAmmoComp      = 0x008F5500;
constexpr addr_t kDefaultAmmoCompIAmmo = 0x008F5420;  // ammoComp + 0x10
constexpr addr_t kDefaultAmmoTemplate  = 0x008F5544;
constexpr addr_t kReplenishingAmmoTemplate = 0x008F5640;
// As duas classes de fisica que todo template pode criar
constexpr addr_t kPhysicsA             = 0x00917578;  // [live] fisica do soldado
constexpr addr_t kPhysicsB             = 0x009179F8;  // [IDA] a outra (objetos compostos)
constexpr addr_t kPhysicsVehicle       = 0x00917FD8;  // [live] fisica do jipe; mesmo getVelocity da kPhysicsA
}  // namespace vtable

// Classe de instancia de cada ObjectTemplate registrado em 0x537263 (template -> createInstance -> vtable).
// Serve para saber o que e um objeto pela vtable (object + 0x00). [IDA]
namespace object_class {
constexpr addr_t kSoldier                    = 0x008E80F8;
constexpr addr_t kGenericFireArm             = 0x008ED768;
constexpr addr_t kPlayerControlObject        = 0x008F2430;  // veiculos
constexpr addr_t kParachute                  = 0x008F2C48;
constexpr addr_t kKit                        = 0x008E5FB0;
constexpr addr_t kGenericProjectile          = 0x008F8608;  // projeteis, granadas, foguetes
constexpr addr_t kControlPoint               = 0x008EE728;  // bandeiras
constexpr addr_t kSupplyDepot                = 0x008EB5F8;
constexpr addr_t kSupplyObject               = 0x008F30E0;
constexpr addr_t kObjectSpawner              = 0x008EB190;
constexpr addr_t kItem                       = 0x008EFE38;
constexpr addr_t kDestroyableObject          = 0x008F0A10;
constexpr addr_t kDestroyableWindowsBundle   = 0x008F0310;
constexpr addr_t kDestroyableWindow          = 0x008FC4C0;
constexpr addr_t kBundle                     = 0x008FC210;  // ItemContainer, KitPart, Obstacle, UAVVehicle
constexpr addr_t kAnimatedBundle             = 0x008F1530;  // tambem MaskObject
constexpr addr_t kAntennaObject              = 0x008F10A0;
constexpr addr_t kAreaObject                 = 0x008F3838;
constexpr addr_t kTargetObject               = 0x008EDFD0;
constexpr addr_t kLadder                     = 0x008EA038;
constexpr addr_t kLadderContainer            = 0x008E9D88;
constexpr addr_t kZipline                    = 0x008EC6E0;
constexpr addr_t kZiplineRope                = 0x008EC0D8;
constexpr addr_t kZiplineContainer           = 0x008EC3D0;
constexpr addr_t kGrapplingHookRope          = 0x008EBE10;
constexpr addr_t kGrapplingHookRopeContainer = 0x008EB9F8;
constexpr addr_t kRopeLink                   = 0x008F3B38;  // tambem HookLink
constexpr addr_t kRemoteControlledObject     = 0x008F3F38;
constexpr addr_t kTurnableRemoteControlled   = 0x008F4308;
constexpr addr_t kTrigger                    = 0x008F4978;
constexpr addr_t kAmbientEffectArea          = 0x008EDDD0;
constexpr addr_t kTriggerable                = 0x0090B250;
constexpr addr_t kTriggerableTarget          = 0x008C58C0;
constexpr addr_t kDropVehicle                = 0x008EE398;
}  // namespace object_class

// ============================================================================
// Estruturas (offsets relativos ao inicio do objeto)
// ============================================================================

// Handle usado para referenciar objetos (Player -> soldado/veiculo etc.)
namespace handle {
constexpr addr_t kRefCount = 0x00;
constexpr addr_t kObject   = 0x04;  // [IDA] objeto referenciado
}  // namespace handle

// Campos comuns a objetos do mundo (soldado, veiculo, arma...)
namespace object {
constexpr addr_t kVtable       = 0x00;
constexpr addr_t kFlags        = 0x04;  // [IDA] uint8; bit 0x20 = sendo apagado (o jogo trata o handle como nulo)
constexpr addr_t kId           = 0x10;  // [IDA] id do objeto (chave nos maps do ObjectManager)
// [IDA][live] no de fisica. O HUD de velocidade (0x78CB10) faz v = fisica->vtbl[0x5C]() (float[3]) e mostra
// int(|v|) * 7 / 2 (m/s * 3.5 ~ km/h). A classe muda entre soldado e veiculo.
constexpr addr_t kPhysics      = 0x44;
namespace physics_vfn {
constexpr addr_t kGetVelocity = 0x5C;
}  // namespace physics_vfn
// Fisica com velocidade embutida, getVelocity = 0x6EFF90 [IDA][live]. Usada pelo soldado (vtable 0x917578)
// e pelo jipe (vtable 0x917FD8, outra classe com o mesmo getter). Jipe andando: 3.47 m/s no campo:
//   se *(fisica + 0x10) (o dono) tem pai (+0x28 != 0, ou seja, esta num assento) -> (0,0,0)
//   senao -> float[3] em fisica + 0x20
namespace soldier_physics {
constexpr addr_t kVtable      = 0x00917578;
constexpr addr_t kGetVelocityFn = 0x006EFF90;
constexpr addr_t kOwner       = 0x10;
constexpr addr_t kVelocity    = 0x20;  // [live]
}  // namespace soldier_physics
// Fisica 0x9179F8 [IDA], getVelocity = 0x6EB390: devolve *(fisica + 0x24), um ponteiro para float[3]. Se o
// byte +0x20 estiver ligado e o dono (+0x18) nao tiver pai, recalcula o vetor antes de devolver
namespace composite_physics {
constexpr addr_t kVtable        = 0x009179F8;  // [IDA] (os jipes usaram 0x917FD8, nao esta)
constexpr addr_t kGetVelocityFn = 0x006EB390;
constexpr addr_t kOwner         = 0x18;
constexpr addr_t kDirty         = 0x20;
constexpr addr_t kVelocityPtr   = 0x24;
}  // namespace composite_physics
// [IDA][live] componente Armor (vida). O HUD (0x78A910) mostra ceil(armor->vtbl[0x3C]() / armor->vtbl[0x84]() * 100)
constexpr addr_t kArmor        = 0x3C;
// [obs] objeto raiz da hierarquia: no soldado a pe aponta para ele mesmo; num veiculo aponta
// para o objeto principal do veiculo (o mesmo que o piloto controla)
constexpr addr_t kRoot         = 0x0C;
constexpr addr_t kTemplate     = 0x14;  // [IDA] ObjectTemplate*; template->vtbl[+0x10]() = type_id
// [IDA] matriz de mundo 4x4 (float[16], linhas estilo D3D: eixos X/Y/Z e depois a posicao).
// vtbl+0x80 (0x71DB50) devolve this + 0x88; a posicao e a linha 3 (+0xB8)
constexpr addr_t kWorldMatrix  = 0x88;
// [IDA][live] objeto pai; o HUD sobe por aqui procurando o gerenciador de armas.
// No soldado: 0 a pe, e o objeto do assento quando esta num veiculo.
constexpr addr_t kParent       = 0x28;
constexpr addr_t kPosX         = 0xB8;  // [scripts][live] posicao (float x3)
constexpr addr_t kPosY         = 0xBC;
constexpr addr_t kPosZ         = 0xC0;
constexpr std::uint8_t kFlagDeleting = 0x20;
}  // namespace object

// PlayerManager: 0x68 bytes, ctor 0x4D9390, fabrica 0x434612
namespace player_manager {
constexpr addr_t kPlayersHead  = 0x4C;  // [IDA][live] std::map<int, Player*> (MSVC 7.1): no-cabeca
constexpr addr_t kPlayersCount = 0x50;  // [IDA] quantidade de players no map
constexpr addr_t kLocalPlayer  = 0x60;  // [IDA][live] Player* local
namespace vfn {
constexpr addr_t kGetLocalPlayer = 0x30;  // retorna *(this + 0x60)
constexpr addr_t kAddPlayer      = 0x78;  // aloca Player (0x498 bytes)
}  // namespace vfn
}  // namespace player_manager

// ObjectManager: 0x158 bytes, vtable 0x9202E0, ctor 0x728BB0 [IDA].
// omgr_getObjectsOfType (0x5929D0) = vtbl+0x34 (0x723B10) + filtro por type_id do template (0x592470)
namespace object_manager {
constexpr addr_t kAllHead   = 0xAC;   // [IDA][live] std::map<id, Object*> com TODOS os objetos (44310 num mapa)
constexpr addr_t kAllCount  = 0xB0;   // [IDA]
// [IDA] map so com os objetos raiz (sem pai e com o bit 0x04 do byte +5). E reconstruido a cada chamada de
// vtbl+0x34, entao de fora e melhor percorrer kAllHead e filtrar
constexpr addr_t kRootsHead  = 0xFC;
constexpr addr_t kRootsCount = 0x100;
namespace vfn {
constexpr addr_t kGetObjectById  = 0x30;  // (provavel) usado com ids em 0x61D450
constexpr addr_t kGetRootObjects = 0x34;
}  // namespace vfn
}  // namespace object_manager

// No do std::map do VS2003 (no-cabeca: parent = raiz; folhas apontam para o no-cabeca)
namespace map_node {
constexpr addr_t kLeft   = 0x00;
constexpr addr_t kParent = 0x04;
constexpr addr_t kRight  = 0x08;
constexpr addr_t kKey    = 0x0C;  // id do player
constexpr addr_t kValue  = 0x10;  // Player*
constexpr addr_t kColor  = 0x14;
constexpr addr_t kIsNil  = 0x15;  // uint8
}  // namespace map_node

// Player: 0x498 bytes, vtable 0x8AC530, ctor 0x5056F0.
// Nomes confirmados pela API Python do jogo (host.pmgr_p_get, 0x593240).
namespace player {
// [IDA][live] handle do objeto controlado agora: o soldado a pe, ou o ASSENTO atual num veiculo
// (cada assento e um objeto proprio; o veiculo inteiro e assento + object::kRoot)
constexpr addr_t kVehicleHandle = 0x80;
constexpr addr_t kName          = 0x60;  // [IDA][live] std::string ("name"; vtbl+0x20)
constexpr addr_t kSoldierHandle = 0xCC;  // [IDA][live] handle do soldado ("defaultvehicle") - seu "player + 204"
constexpr addr_t kAI            = 0xD4;  // [IDA][live] uint8 ("ai": 1 = bot)
constexpr addr_t kAlive         = 0xD5;  // [IDA][live] uint8 ("alive")
constexpr addr_t kSuicide       = 0xD6;  // [IDA] uint8 ("suicide")
constexpr addr_t kTeam          = 0xD8;  // [IDA][live] int32 ("team")
constexpr addr_t kManDown       = 0xE8;  // [IDA] uint8 ("mandown": caido esperando medico)
constexpr addr_t kSgr           = 0xF0;  // [IDA] int32 ("sgr"; significado nao identificado)
constexpr addr_t kPing          = 0xF8;  // [IDA] int32 ("ping")
constexpr addr_t kScore         = 0x108; // [IDA] ponteiro para score_block (vtbl+0x170, usado por pmgr_setScore)
constexpr addr_t kSquadId       = 0x10C; // [IDA][live] int32 ("sqid"; 0 = sem esquadrao)
constexpr addr_t kCommander     = 0x110; // [IDA] uint8 ("commander")
constexpr addr_t kSquadLeader   = 0x111; // [IDA][live] uint8 ("isql")
constexpr addr_t kInsideCP      = 0x124; // [IDA] uint8 ("isInsideCP": dentro da area de uma bandeira)
constexpr addr_t kFlagHolder    = 0x140; // [IDA] uint8 ("fholder")
constexpr addr_t kRemote        = 0x189; // [IDA] uint8 ("remote")
constexpr addr_t kConnected     = 0x1A8; // [IDA] uint8 ("connected")
constexpr addr_t kAddress       = 0x2DC; // [IDA] std::string ("addr": endereco de rede)
constexpr addr_t kProfileId     = 0x318; // [IDA] int32 ("profileid")
// "tts" (vtbl+0xF0) e calculado a partir do tempo atual, nao e um campo
namespace vfn {
constexpr addr_t kGetName           = 0x20;
constexpr addr_t kGetVehicle        = 0x44;
constexpr addr_t kIsRemote          = 0x58;
constexpr addr_t kIsAI              = 0x60;
constexpr addr_t kIsAlive           = 0x68;
constexpr addr_t kIsSuicide         = 0x70;
constexpr addr_t kIsConnected       = 0x74;
constexpr addr_t kGetDefaultVehicle = 0xA0;
constexpr addr_t kGetTeam           = 0xE4;
constexpr addr_t kGetTimeToSpawn    = 0xF0;
constexpr addr_t kIsManDown         = 0xFC;
constexpr addr_t kGetSquadId        = 0x108;
constexpr addr_t kIsCommander       = 0x110;
constexpr addr_t kIsSquadLeader     = 0x138;
constexpr addr_t kIsInsideCP        = 0x144;
constexpr addr_t kGetSgr            = 0x154;
constexpr addr_t kGetPing           = 0x164;
constexpr addr_t kGetScore          = 0x170;
constexpr addr_t kIsFlagHolder      = 0x178;
constexpr addr_t kGetAddress        = 0x264;
constexpr addr_t kGetProfileId      = 0x2A8;
}  // namespace vfn
}  // namespace player

// Componente Armor: *(object + 0x3C). vtable 0x90B570 (ctor 0x6891E0); 0x8A7D50, 0x8EE6B4 e 0x93C94C
// usam o mesmo getHitPoints, entao o campo +0x10 vale para as classes derivadas tambem. [IDA]
namespace armor {
constexpr addr_t kTemplate  = 0x08;  // [IDA][live] ArmorTemplate*
constexpr addr_t kHitPoints = 0x10;  // [IDA][live] float: vida atual (comeca com template + 0xC4); soldado 100/100
namespace vfn {
constexpr addr_t kGetHitPoints    = 0x3C;  // fld [this + 0x10]
constexpr addr_t kIsDestroyed     = 0x5C;  // hitPoints <= 0.0
constexpr addr_t kGetMaxHitPoints = 0x84;  // fld [template + 0xC8]
}  // namespace vfn
}  // namespace armor

// Template do Armor (valores do .con; registrado como TemplateComponent.Armor, id 50346, ctor 0x6559F0)
namespace armor_template {
constexpr addr_t kHitPoints    = 0xC4;  // [IDA] float ("armor.hitPoints": vida inicial)
constexpr addr_t kMaxHitPoints = 0xC8;  // [IDA][live] float ("armor.maxHitPoints"; soldado = 100)
}  // namespace armor_template

// Bloco de pontuacao: *(player + 0x108). Ordem tirada de pmgr_setScore (0x593A30) [IDA]
namespace score_block {
constexpr addr_t kScore      = 0x00;  // int32 ("score")
constexpr addr_t kRplScore   = 0x04;  // int32 ("rplScore")
constexpr addr_t kSkillScore = 0x08;  // int32 ("skillScore")
constexpr addr_t kCmdScore   = 0x0C;  // int32 ("cmdScore")
constexpr addr_t kDeaths     = 0x10;  // int32 ("deaths")
constexpr addr_t kKills      = 0x14;  // int32 ("kills")
constexpr addr_t kTeamKills  = 0x18;  // int32 ("TKs")
}  // namespace score_block

// ObjectTemplate (base comum dos templates) [IDA]
namespace object_template {
constexpr addr_t kName = 0x0C;  // [live] std::string (ex.: "meinsurgent_at_sp"); vtbl+0x28 (0x65DDD0) devolve this + 0x0C
namespace vfn {
constexpr addr_t kGetTypeId = 0x10;  // devolve o type_id (ex.: Kit = 38038)
constexpr addr_t kGetName   = 0x28;
}  // namespace vfn
}  // namespace object_template

// Soldado: vtable 0x8E80F8 [live], ctor 0x5AC3A0, queryInterface 0x5A5640
namespace soldier {
constexpr addr_t kWeaponHandler = 0x154;  // [IDA][live] queryInterface(50373); vtable da interface 0x8E7F80
constexpr addr_t kHudIface      = 0x15C;  // [IDA] queryInterface(50391); vtbl+0x54 devolve *(iface + 0x94) = kit
// [IDA][live] Kit* = *(soldier + 0x15C + 0x94). O "kit" do pmgr_p_get vem por aqui; com o jogo rodando
// esse campo apontou para um objeto de vtable 0x8E5FB0. Nome do kit = template (+0x14) -> object_template::kName
constexpr addr_t kKit           = 0x1F0;
// [IDA][live] int32 postura = *(soldier + 0x15C + 0xD4) (iface 50391, vtbl+0x38 = 0x615960):
// 0 = em pe, 1 = agachado, 2 = deitado (conferido; o olho da camera desceu 34.88 -> 34.36 -> 33.78).
// 3 cai no mesmo caso do 0 no switch de 0x60B048 (provavelmente nadando). O texto do .con usa outra
// numeracao (0x810420: Standing=1, Crouching=2, Lying=3, Swimming=4)
constexpr addr_t kPose          = 0x230;
enum Pose : int { kPoseStand = 0, kPoseCrouch = 1, kPoseProne = 2 };
constexpr addr_t kDeadFlag      = 0xEC;   // [scripts] uint8
constexpr addr_t kSeat          = object::kParent;  // [live] assento atual num veiculo; 0 a pe
constexpr addr_t kPlayerHandle  = 0x168;  // [IDA] handle do Player dono (o ctor compara com getLocalPlayer)
// [IDA][live] 2 pares {Skeleton*, objeto de animacao} de 8 bytes (0x61D8A0 usa [soldier + i*8 + 0x288]).
// Indice 0 = 1P, 1 = 3P. Num bot: 3P com pose real (80 ossos), 1P so com identidade (70 ossos).
constexpr addr_t kSkeletons      = 0x288;
constexpr addr_t kSkeletonStride = 0x08;
constexpr addr_t kSkeleton1P     = 0x288;
constexpr addr_t kSkeleton3P     = 0x290;
// [IDA][live] int32: qual dos dois esqueletos esta ativo (0 = 1P, 1 = 3P). A atualizacao por quadro
// (0x61F630) so calcula a pose do esqueleto ativo. Visto: bot = 1, jogador local em 1a pessoa = 0.
constexpr addr_t kActiveSkeleton = 0x2B8;
// [obs] ponteiros para GenericFireArm em +0x5C0, +0x5CC e +0x5D0 (+0x5C0 == +0x5D0; o de +0x5CC
// nao tem IAmmo). Talvez a lista de armas do kit; nenhum acesso a esses offsets foi achado no IDA.
constexpr addr_t kUnknownWeaponRefs = 0x5C0;
}  // namespace soldier

// Gerenciador de armas do soldado (vtable 0x8E7F80), getWeapon = 0x5AB260
namespace soldier_weapon_handler {
// [IDA][live] GenericFireArm*[slot], 5 entradas (+0x64..+0x74):
// slot 0 (primaria) = soldier + 0x1B8 -> acompanha a arma selecionada (fuzil, granada...)
// slot 1 (secundaria) = soldier + 0x1BC -> vazio a pe
// [obs] slots 3/4 (+0x1C4/+0x1C8) apareceram com o fuzil em duas leituras; significado nao confirmado
constexpr addr_t kWeapons = 0x64;
}  // namespace soldier_weapon_handler

// Esqueleto por instancia (Skeleton.cpp). Pose calculada em 0x7F5360: cada matriz = local * pai.
// A base passada e quase identidade (translacao y = -1), entao as matrizes ficam em ESPACO DO MODELO:
// posicao no mundo = translacao do osso * matriz de mundo do soldado (object::kWorldMatrix). [live]
// [live] Observacoes:
//   - soldados longe (~1500 m) nao tem pose calculada: todas as translacoes ficam 0
//   - os ossos auxiliares mesh2..mesh4 (filhos de right_ullna) as vezes guardam posicao de MUNDO
//     (visto num bot deitado; num bot agachado estavam normais) -> bones() trata os dois casos
//   - a origem do soldado fica ~1 m acima do chao: deitado, tudo entre -0.7 e -1.0; agachado,
//     pes em -0.9/-1.0 e cabeca em +0.2 (cabeca ~1.2 m acima dos pes)
//   - skeleton 3P de soldado: 80 ossos; root=0, pernas 1-10, spine2/spine3/torso 11-13,
//     braco esq. 14-29, braco dir. 30-44, joint20/neck/head 45-47, rosto 48-63, mesh* 64-79.
//     Use o nome ("head", "neck"...) em vez do indice: o esqueleto 1P tem outra numeracao.
namespace skeleton {
constexpr addr_t kLocal     = 0x04;  // [IDA] dados locais por osso (72 bytes cada)
constexpr addr_t kBoneDefs  = 0x08;  // [IDA][live] definicoes dos ossos, vindas do .ske (36 bytes cada)
constexpr addr_t kMatrices  = 0x10;  // [IDA][live] float[16] por osso (64 bytes cada)
constexpr addr_t kBoneCount = 0x14;  // [IDA][live] int32
constexpr addr_t kIkChains  = 0x1C;  // [IDA] cadeias de IK (84 bytes cada; 0x7F32A0 = IK de dois ossos)
constexpr addr_t kBoneDefSize = 36;
constexpr addr_t kDefNameId   = 0x04;  // [IDA][live] int16: indice na tabela global de nomes
constexpr addr_t kDefParent   = 0x06;  // [IDA][live] int16: osso pai (< 0 = raiz)
constexpr addr_t kMatrixSize  = 64;
constexpr addr_t kMatrixTranslation = 0x30;  // [IDA][live] float x3 (linha 3 da matriz)
// Acima disso a translacao nao e do espaco do modelo (ossos auxiliares com posicao de mundo)
constexpr float kMaxModelSpaceDistance = 50.0f;
}  // namespace skeleton

// Tabela global de nomes de ossos: std::vector<std::string> (singleton 0x7F7F80).
// 0x7F7E10 = nome -> id, 0x7F7E80 = id -> nome (fora do intervalo devolve "Invalid bone name index")
namespace bone_names {
constexpr addr_t kTable = 0x00A33AE4;  // [IDA][live]
constexpr addr_t kBegin = 0x04;        // [IDA] std::string*
constexpr addr_t kEnd   = 0x08;        // [IDA]
}  // namespace bone_names

// std::string do VS2003 (MSVCP71): 28 bytes
namespace msvc_string {
constexpr addr_t kSize     = 28;
constexpr addr_t kBuffer   = 0x04;  // char[16] inline, ou char* quando capacidade >= 16
constexpr addr_t kLength   = 0x14;
constexpr addr_t kCapacity = 0x18;
}  // namespace msvc_string

// Veiculos (vtable::kVehicles): gerenciador de armas em +0x154
namespace vehicle {
constexpr addr_t kWeaponHandler = 0x154;  // [IDA][live] vtable da interface 0x8F22B8 no carro (classe 0x8F2430)
}  // namespace vehicle

// Gerenciador de armas do veiculo, getWeapon = 0x5F67D0.
// Delega para o dono (0x5F6860) quando: *(iface+0x2C) == 0, os handles em +0x1C e +0x20
// sao validos e o objeto de +0x20 e um Soldier (type_id 38035). Nesse caso usa as armas do soldado.
// [live] Assento de passageiro que atira com a arma pessoal: activeWeapon(assento) == soldier + 0x1B8.
// Cada assento e um objeto proprio da mesma classe (ex.: 0x8F2430); piloto/co-piloto tem armas -1 (infinitas).
namespace vehicle_weapon_handler {
constexpr addr_t kDelegateBlock = 0x2C;  // [IDA] != 0 impede a delegacao
constexpr addr_t kHandleA       = 0x1C;  // [IDA] handle (precisa ser valido para delegar)
constexpr addr_t kOwnerHandle   = 0x20;  // [IDA] handle do dono
// [IDA][live] GenericFireArm*[slot] -> slot 0 = vehicle + 0x1FC, slot 1 = vehicle + 0x200
// (conferido no motorista de um carro, vtable 0x8F2430: as duas posicoes com armas)
constexpr addr_t kWeapons       = 0xA8;
}  // namespace vehicle_weapon_handler

// Slots que o HUD pede ao getWeapon (vtable +0x74 do gerenciador de armas) [IDA][live]
constexpr int kPrimaryWeaponSlot   = 0;  // 0x7A85F0 -> PrimaryAmmo   (hud + 0xB4)
constexpr int kSecondaryWeaponSlot = 1;  // 0x7A90C0 -> SecondaryAmmo (hud + 0xC4)

// Arma (GenericFireArm): vtable 0x8ED768 [live], ctor 0x5E48B0, queryInterface 0x5E4A60.
// 0x5E2530/0x5E4E40 guardam o ponteiro de cada componente logo apos a criacao.
namespace weapon {
constexpr addr_t kOwner        = object::kParent;  // [IDA] soldado/veiculo que segura a arma
constexpr addr_t kFireArmIface = 0x170;  // [IDA] queryInterface(38080) - o HUD le municao por aqui
constexpr addr_t kComp50438    = 0x18C;  // [IDA] componente 50438
constexpr addr_t kComp50430    = 0x190;  // [IDA] componente 50430 (copiado tambem em +0x1B0)
constexpr addr_t kComp50415    = 0x184;  // [IDA] componente 50415
constexpr addr_t kAmmo         = 0x194;  // [IDA][live] IAmmo* (queryInterface 50418); vtable 0x8F5420
constexpr addr_t kComp50422    = 0x198;  // [IDA]
constexpr addr_t kComp50442    = 0x19C;  // [IDA]
constexpr addr_t kComp50426    = 0x1A0;  // [IDA]
constexpr addr_t kComp50434    = 0x1A4;  // [IDA] (copiado tambem em +0x1B4)
}  // namespace weapon

// Interface da arma em weapon + 0x170 (vtable 0x8ED558). Todos repassam para *(this + 0x24) = weapon + 0x194.
namespace firearm_iface {
namespace vfn {
constexpr addr_t kHeat       = 0x24;  // [IDA] float
constexpr addr_t kReload     = 0x90;  // [IDA] progresso da recarga (o HUD mostra 1 - valor)
constexpr addr_t kNrOfMags   = 0x98;  // [IDA] do template; <= -1 = pentes infinitos
constexpr addr_t kMagsLeft   = 0xA0;  // [IDA] pentes restantes, contando o atual
constexpr addr_t kMagSize    = 0xA8;  // [IDA]
constexpr addr_t kAmmoInMag  = 0xAC;  // [IDA] numero que o HUD mostra ("%i")
}  // namespace vfn
}  // namespace firearm_iface

// IAmmo = AmmoComp + 0x10. DefaultAmmoComp: 0x68 bytes, ctor base 0x6230A0, criado por 0x605400.
// O vetor de pentes e std::vector<int>: um elemento por pente com as balas que restam nele.
namespace ammo {
constexpr std::int32_t kOwnerWeapon = -0x04;  // [IDA] GenericFireArm* (AmmoComp + 0x0C)
// [IDA][live] int*; [0] = balas no pente atual. -1 = infinito: com nrOfMags = magSize = -1 o vetor
// fica com um unico elemento -1 (visto em armas de motorista de veiculo)
constexpr addr_t kMagsBegin  = 0x08;
constexpr addr_t kMagsEnd    = 0x0C;  // [IDA][live]
constexpr addr_t kMagsCap    = 0x10;  // [IDA]
constexpr addr_t kReloadState = 0x3C;  // [IDA] 0 = sem recarga; 1/2 = recarregando/finalizando (provavel)
constexpr addr_t kTemplate   = 0x4C;  // [IDA][live] AmmoTemplate*
namespace vfn {
constexpr addr_t kSubtract   = 0x38;  // [IDA] 0x622DD0: mags[0] -= *n; nao desconta se mags[0] == -1
constexpr addr_t kMagSize    = 0xC4;  // [IDA] template + 0x38
constexpr addr_t kNrOfMags   = 0xC8;  // [IDA] template + 0x34
constexpr addr_t kAutoReload = 0xCC;  // [IDA] template + 0x28
constexpr addr_t kMagsLeft   = 0xD0;  // [IDA] (end - begin) / 4
constexpr addr_t kAmmoInMag  = 0xD4;  // [IDA] *begin
}  // namespace vfn
}  // namespace ammo

// Template de municao (valores do .con). A funcao que salva o .con (0x6227F0) recebe
// template + 4, por isso os offsets dela aparecem 4 bytes menores que estes.
namespace ammo_template {
constexpr addr_t kAmmoType              = 0x08;  // int
constexpr addr_t kReloadTime            = 0x0C;  // float (padrao 2.0)
constexpr addr_t kFirstShotExtraTime    = 0x14;  // float
constexpr addr_t kLastShotExtraTime     = 0x1C;  // float
constexpr addr_t kChangeMagAt           = 0x24;  // float (padrao 0.7)
constexpr addr_t kAutoReload            = 0x28;  // uint8
constexpr addr_t kInstantReloadOnEnable = 0x29;  // uint8
constexpr addr_t kMinimumTimeUntilReload = 0x2C; // float
constexpr addr_t kNrOfMags              = 0x34;  // int (padrao 5; -1 = infinito) [live]
constexpr addr_t kMagSize               = 0x38;  // int (padrao 30) [live]
constexpr addr_t kReloadAmount          = 0x3C;  // int
constexpr addr_t kToggleWhenNoAmmo      = 0x40;  // uint8
constexpr addr_t kReloadWithoutPlayer   = 0x41;  // uint8
constexpr addr_t kMagLinkWeapon         = 0x44;  // std::string (tamanho em +0x58)
}  // namespace ammo_template

// Logica da partida: *(exe::kGameLogic) [scripts]
namespace game_logic {
constexpr addr_t Tickets(int team) { return 0x90 + 0x94 * team; }  // int32; team = 1 ou 2
constexpr addr_t kWinner      = 0x9C;  // uint8
constexpr addr_t kVictoryType = 0x9D;  // uint8
}  // namespace game_logic

// ============================================================================
// Funcoes do PRBF2.exe (para estudo/breakpoints) [IDA]
// ============================================================================
namespace fn {
// Municao
constexpr addr_t kAmmoComp_Subtract          = 0x00622DD0;  // "Trying to subtract ammo when already 0!"
constexpr addr_t kAmmoComp_Ctor              = 0x006230A0;  // cria o vetor: nrOfMags pentes com magSize balas
constexpr addr_t kAmmoComp_StartReload       = 0x00622320;
constexpr addr_t kAmmoComp_QueryInterface    = 0x00605260;
constexpr addr_t kDefaultAmmoTemplate_Create = 0x00605400;
constexpr addr_t kAmmoTemplate_SaveCon       = 0x006227F0;  // escreve ObjectTemplate.ammo.*
// Arma
constexpr addr_t kGenericFireArm_Ctor            = 0x005E48B0;
constexpr addr_t kGenericFireArm_QueryInterface  = 0x005E4A60;
constexpr addr_t kGenericFireArm_CacheComponents = 0x005E2530;  // e 0x005E4E40
constexpr addr_t kSoldierWeaponHandler_GetWeapon = 0x005AB260;
constexpr addr_t kVehicleWeaponHandler_GetWeapon = 0x005F67D0;
constexpr addr_t kVehicleWeaponHandler_Delegates = 0x005F6860;
// Player / PlayerManager / soldado
constexpr addr_t kPlayerManager_Factory   = 0x00434612;
constexpr addr_t kPlayerManager_Ctor      = 0x004D9390;
constexpr addr_t kPlayerManager_AddPlayer = 0x004D7040;
constexpr addr_t kPlayer_Ctor             = 0x005056F0;
constexpr addr_t kPlayer_GetVehicle       = 0x004FDBF0;
constexpr addr_t kPlayer_GetDefaultVehicle = 0x004FE5E0;
constexpr addr_t kSoldier_Ctor            = 0x005AC3A0;
constexpr addr_t kSoldier_QueryInterface  = 0x005A5640;
constexpr addr_t kPython_PmgrPGet         = 0x00593240;  // host.pmgr_p_get: nomes dos campos do player
constexpr addr_t kPython_PmgrSetScore     = 0x00593A30;  // host.pmgr_setScore: layout do score_block
constexpr addr_t kPython_PmgrGetScore     = 0x00593FA0;  // (provavel) host.pmgr_getScore
constexpr addr_t kObject_GetWorldMatrix   = 0x0071DB50;  // devolve this + 0x88
// Esqueleto
constexpr addr_t kSkeleton_UpdatePose     = 0x007F5360;  // compoe as matrizes (local * pai) para todos os ossos
constexpr addr_t kSkeleton_TwoBoneIK      = 0x007F32A0;
constexpr addr_t kSkeleton_FindBone       = 0x007F2EE0;  // id do nome -> indice do osso
constexpr addr_t kSkeleton_SetCollisionBone = 0x007F47B0;
constexpr addr_t kBoneNames_Instance      = 0x007F7F80;
constexpr addr_t kBoneNames_Find          = 0x007F7E10;  // nome -> id
constexpr addr_t kBoneNames_Get           = 0x007F7E80;  // id -> std::string&
constexpr addr_t kSoldier_ResetPose3P     = 0x0061D450;  // usa *(soldier + 0x290)
constexpr addr_t kTemplateClassRegistry   = 0x00537263;  // registra ObjectTemplate.* com type_id
// HUD
constexpr addr_t kHud_UpdatePlayerHealth = 0x0078A910;  // PlayerHealth = vida / vida maxima (object + 0x3C)
constexpr addr_t kHud_UpdateSpeed       = 0x0078CB10;  // SpeedString = int(|fisica->vtbl[0x5C]()|) * 3.5
constexpr addr_t kSoldierPose_FromString = 0x00810420;  // "Standing"/"Crouching"/"Lying"/"Swimming" -> 1..4
constexpr addr_t kArmor_Ctor            = 0x006891E0;
constexpr addr_t kArmorTemplate_Ctor    = 0x006559F0;
constexpr addr_t kHud_Update            = 0x007AA400;
constexpr addr_t kHud_UpdatePrimaryAmmo = 0x007A85F0;  // getWeapon(0) -> PrimaryAmmo
constexpr addr_t kHud_UpdateSecondaryAmmo = 0x007A90C0;  // getWeapon(1) -> SecondaryAmmo; sobe pelos pais ate achar 50373
constexpr addr_t kHud_RegisterVars     = 0x007A62C0;  // PrimaryAmmo = hud+0xB4, PrimaryAmmoString = hud+0x144
}  // namespace fn

// ============================================================================
// RendDX9.dll - offsets relativos a base do modulo
// ============================================================================
namespace rend {
// [live] A DLL e REALOCADA: rodando, a base era 0x07B50000 (nao 0x10000000). Sempre use a base real do
// modulo (Toolhelp32 / EnumProcessModules) + estes offsets.
// Objeto dono do device (vtable RVA 0x1DDA38, ctor RVA 0xB810). E escrito em dois lugares:
// RVA 0xBA03 (ctor, this) e RVA 0x52331 (copia do "Renderer"). [live] Rodando, aponta para o objeto do
// device: device valido em +0x0C e camera em +0xF8 no mesmo objeto.
constexpr addr_t kDeviceOwner = 0x23B970;
namespace device_owner {
constexpr addr_t kD3D9         = 0x08;  // [IDA] IDirect3D9* (Direct3DCreate9)
constexpr addr_t kDevice       = 0x0C;  // [IDA][live] IDirect3DDevice9* (saida do CreateDevice, RVA 0xA764)
constexpr addr_t kAdapter      = 0x80;  // [IDA] argumento Adapter do CreateDevice
constexpr addr_t kDeviceType   = 0x84;  // [IDA] "GSDeviceType"
constexpr addr_t kBehavior     = 0x88;  // [IDA] "GSDeviceBehavior"
constexpr addr_t kFocusWindowP = 0xD4;  // [IDA] ponteiro para o HWND de foco
constexpr addr_t kCamera       = 0xF8;  // [scripts][live] camera ativa
}  // namespace device_owner

namespace camera {  // [scripts] *(device_owner + 0xF8)
constexpr addr_t kFov      = 0x24;  // [live] float (rad); 1.099 rad (~63 graus) a pe
constexpr addr_t kNear     = 0x28;
constexpr addr_t kFar      = 0x2C;
constexpr addr_t kAspect   = 0x30;
constexpr addr_t kEye      = 0x6C;  // [live] float x3; a pe = posicao do soldado + ~0.47 m em Y
// [live] float[16] ViewProj: vetor-linha * matriz (convencao D3D), ver Walker::worldToNdc.
// Mirando na cabeca de um bot a 3.8 m: ndc (0.003, -0.031) -> pixel 642,371 numa tela 1280x720
constexpr addr_t kViewProj = 0xFC;
}  // namespace camera

constexpr addr_t kCameraBlock    = 0x25E64C;  // [scripts] bloco de camera/projecao
constexpr addr_t kAuxRenderState = 0x23B984;  // [IDA] objeto de 0x3EA0 bytes com o device em +0x0C
constexpr addr_t kRenderer       = 0x23D098;  // [IDA] "Renderer" registrado no ClassManager (830001/831001)
constexpr addr_t kPostProduction = 0x23FAD4;  // [IDA] "PostProductionFrameWork"

// Objetos que o initDll (RVA 0x74470) pede ao ClassManager pelo nome [IDA]
namespace class_manager {
constexpr addr_t kClassManager         = 0x244A94;
constexpr addr_t kSettingsRepository   = 0x244A90;
constexpr addr_t kMainConsoleObjects   = 0x244A98;
constexpr addr_t kBfGlobalCallback     = 0x244A9C;
constexpr addr_t kBfGlobalCallbackData = 0x244AA0;
constexpr addr_t kDebugOn              = 0x244AA4;
constexpr addr_t kDebugModules         = 0x244AA8;
constexpr addr_t kEventManager         = 0x244AAC;
constexpr addr_t kFrameEventManager    = 0x244AB0;
constexpr addr_t kProfiler             = 0x244AB4;
constexpr addr_t kLoadStat             = 0x244AB8;
constexpr addr_t kSystem               = 0x244AC0;
constexpr addr_t kFileManager          = 0x244AC4;
constexpr addr_t kFileChangeManager    = 0x244AC8;
constexpr addr_t kMainConsole          = 0x244ACC;
constexpr addr_t kObjectManager        = 0x244AD8;
constexpr addr_t kObjectTemplateManager = 0x244ADC;
constexpr addr_t kSlayerManager        = 0x244AE0;  // na sua copia recebe *(0x00A08F64) = PlayerManager
constexpr addr_t kGame                 = 0x244AEC;
constexpr addr_t kAIConnection         = 0x244AF0;
}  // namespace class_manager

// Modificacoes presentes no seu RendDX9.dll (nao existem no original)
namespace patched {
constexpr addr_t kSlayerManagerRead = 0x74770;  // A1 64 8F A0 00 90 90 = mov eax,[0x00A08F64]; nop; nop
constexpr addr_t kInt3              = 0x74874;  // CC no lugar de 32 (xor bl,bl)
}  // namespace patched
}  // namespace rend

// ============================================================================
// Leitura de memoria
// ============================================================================
struct ExternalMem {
    HANDLE process = nullptr;

    template <class T>
    bool read(addr_t address, T& out) const {
        SIZE_T n = 0;
        return address &&
               ReadProcessMemory(process, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(address)), &out,
                                 sizeof(T), &n) &&
               n == sizeof(T);
    }

    bool readRaw(addr_t address, void* buffer, SIZE_T size) const {
        SIZE_T n = 0;
        return address && buffer && size &&
               ReadProcessMemory(process, reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(address)), buffer,
                                 size, &n) &&
               n == size;
    }
};

struct InternalMem {  // so faz sentido dentro do processo do jogo
    template <class T>
    bool read(addr_t address, T& out) const {
        const void* p = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address));
        if (!address || IsBadReadPtr(p, sizeof(T))) return false;
        std::memcpy(&out, p, sizeof(T));  // memcpy para aceitar arrays (float[16] etc.)
        return true;
    }

    bool readRaw(addr_t address, void* buffer, SIZE_T size) const {
        const void* p = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address));
        if (!address || !buffer || !size || IsBadReadPtr(p, size)) return false;
        std::memcpy(buffer, p, size);
        return true;
    }
};

struct PlayerInfo {
    std::string name;
    std::string kit;  // nome do template do kit (vazio se sem soldado)
    int team = 0, squad = 0, ping = 0;
    int score = 0, kills = 0, deaths = 0, teamKills = 0;
    float hp = 0.0f, maxHp = 0.0f;  // vida do soldado (Armor)
    bool ai = false, alive = false, manDown = false, squadLeader = false, commander = false;
};

struct VehicleInfo {
    addr_t obj = 0;                  // objeto raiz do veiculo (PlayerControlObject)
    std::string name;                // nome do template
    float hp = 0.0f, maxHp = 0.0f;
    float pos[3] = {};
    float vel[3] = {};
    bool hasVelocity = false;
    bool likelyVehicle = false;      // vida maxima > 0 e fisica conhecida (ver Walker::vehicles)
    std::vector<addr_t> occupants;   // Player* dentro do veiculo
};

struct BoneInfo {
    std::string name;
    int parent = -1;       // indice do osso pai (< 0 = raiz)
    float world[3] = {};   // posicao no mundo
};

struct AmmoInfo {
    int inMag    = 0;  // balas no pente atual (-1 = infinito)
    int magsLeft = 0;  // pentes restantes, contando o atual
    int magSize  = 0;  // do template
    int nrOfMags = 0;  // do template (-1 = infinito)
};

// Percorre as cadeias acima sem chamar codigo do jogo (serve tambem para leitura externa).
template <class Mem>
class Walker {
public:
    explicit Walker(const Mem& mem) : mem_(mem) {}

    addr_t ptr(addr_t address) const {
        addr_t v = 0;
        mem_.read(address, v);
        return v;
    }

    // Mesma regra do jogo: handle nulo, objeto nulo ou objeto sendo apagado -> 0
    addr_t resolveHandle(addr_t h) const {
        addr_t obj = h ? ptr(h + handle::kObject) : 0;
        std::uint8_t flags = 0;
        if (!obj || !mem_.read(obj + object::kFlags, flags) || (flags & object::kFlagDeleting)) return 0;
        return obj;
    }

    addr_t playerManager() const { return ptr(exe::kPlayerManager); }

    addr_t localPlayer() const {
        addr_t pm = playerManager();
        return pm ? ptr(pm + player_manager::kLocalPlayer) : 0;
    }

    // Todos os players do std::map em +0x4C (em ordem de id)
    std::vector<addr_t> players(std::size_t limit = 256) const {
        addr_t pm = playerManager();
        return mapValues(pm ? ptr(pm + player_manager::kPlayersHead) : 0, limit);
    }

    // Todos os objetos do mundo (ObjectManager, inclusive filhos como assentos e rodas)
    std::vector<addr_t> objects(std::size_t limit = 50000) const {
        addr_t om = ptr(exe::kObjectManager);
        return mapValues(om ? ptr(om + object_manager::kAllHead) : 0, limit);
    }

    // Sobe pelos pais (object + 0x28) ate o objeto raiz
    addr_t topParent(addr_t obj) const {
        for (int i = 0; obj && i < 16; ++i) {
            addr_t parent = ptr(obj + object::kParent);
            if (!parent) return obj;
            obj = parent;
        }
        return obj;
    }

#pragma pack(push, 1)
    struct RawMapNode {
        addr_t left;   // 0x00
        addr_t parent; // 0x04
        addr_t right;  // 0x08
        uint32_t key;  // 0x0C
        addr_t value;  // 0x10
        uint8_t color; // 0x14
        uint8_t isNil; // 0x15
    };
#pragma pack(pop)

    // Valores (segundo campo) de um std::map<chave de 4 bytes, ponteiro> do VS2003, em ordem de chave
    std::vector<addr_t> mapValues(addr_t head, std::size_t limit) const {
        std::vector<addr_t> out;
        if (!head) return out;
        out.reserve((std::min)(limit, static_cast<std::size_t>(256)));
        std::vector<std::pair<addr_t, RawMapNode>> stack;
        stack.reserve(48);
        addr_t node = ptr(head + map_node::kParent);  // raiz

        RawMapNode rn = {};
        auto getNode = [&](addr_t a, RawMapNode& n) -> bool {
            if (!a || a == head) return false;
            if (!mem_.readRaw(a, &n, sizeof(RawMapNode))) return false;
            return (n.isNil == 0);
        };

        while ((getNode(node, rn) || !stack.empty()) && out.size() < limit) {
            while (getNode(node, rn)) {
                if (stack.size() > 48) return out;  // arvore inconsistente
                stack.push_back({ node, rn });
                node = rn.left;
            }
            auto top = stack.back();
            stack.pop_back();
            if (top.second.value) out.push_back(top.second.value);
            node = top.second.right;
        }
        return out;
    }

    // Veiculos com os players que estao dentro deles. Toda PlayerControlObject sem pai entra na lista, mas no PR
    // essa classe tambem e usada por posicionaveis (deployable_*), marcadores, cameras de espectador, artilharia
    // e "dummies" de projeteis [live]. likelyVehicle = vida maxima > 0 e fisica conhecida (o que separou os
    // jipes do resto num mapa real); onlyLikely filtra por isso.
    std::vector<VehicleInfo> vehicles(bool onlyLikely = true) const {
        std::vector<VehicleInfo> out;
        std::vector<addr_t> plist = players();
        for (addr_t obj : objects()) {
            if (ptr(obj + object::kVtable) != object_class::kPlayerControlObject || ptr(obj + object::kParent))
                continue;
            VehicleInfo v;
            v.obj = obj;
            v.name = templateName(obj);
            health(obj, v.hp, v.maxHp);
            position(obj, v.pos);
            v.hasVelocity = velocity(obj, v.vel);
            v.likelyVehicle = v.maxHp > 0.0f && v.hasVelocity;
            if (onlyLikely && !v.likelyVehicle) continue;
            for (addr_t pl : plist) {
                addr_t ctl = controlledOf(pl);
                if (ctl && ctl != soldierOf(pl) && topParent(ctl) == obj) v.occupants.push_back(pl);
            }
            out.push_back(std::move(v));
        }
        return out;
    }

    addr_t soldierOf(addr_t pl) const { return pl ? resolveHandle(ptr(pl + player::kSoldierHandle)) : 0; }
    addr_t controlledOf(addr_t pl) const { return pl ? resolveHandle(ptr(pl + player::kVehicleHandle)) : 0; }

    // [live] Morto, o jogador controla a "MultiPlayerFreeCamera" (tambem um PlayerControlObject), entao
    // so conta como veiculo quando existe um soldado
    bool inVehicle(addr_t pl) const {
        addr_t sol = soldierOf(pl);
        addr_t c = controlledOf(pl);
        return sol && c && c != sol;
    }

    // Assento em que o soldado esta (0 a pe) e o objeto principal do veiculo
    addr_t seatOf(addr_t soldierObj) const { return soldierObj ? ptr(soldierObj + soldier::kSeat) : 0; }
    addr_t rootOf(addr_t obj) const { return obj ? ptr(obj + object::kRoot) : 0; }

    bool isAlive(addr_t pl) const {
        std::uint8_t v = 0;
        return pl && mem_.read(pl + player::kAlive, v) && v != 0;
    }

    int team(addr_t pl) const {
        int v = 0;
        if (pl) mem_.read(pl + player::kTeam, v);
        return v;
    }

    bool position(addr_t obj, float xyz[3]) const {
        return obj && mem_.read(obj + object::kPosX, xyz[0]) && mem_.read(obj + object::kPosY, xyz[1]) &&
               mem_.read(obj + object::kPosZ, xyz[2]);
    }

    // Emula queryInterface(50373) + getWeapon(slot) identificando a classe pela vtable e subindo
    // pelos pais (object + 0x28), como o HUD faz. A lista de classes vem das 4 implementacoes de
    // queryInterface que testam 50373; subclasses com outra vtable nao sao reconhecidas.
    addr_t activeWeapon(addr_t obj, int slot = kPrimaryWeaponSlot, int depth = 0) const {
        for (int i = 0; obj && i < 8; ++i) {
            addr_t vt = ptr(obj + object::kVtable);
            if (vt == vtable::kSoldier)
                return ptr(obj + soldier::kWeaponHandler + soldier_weapon_handler::kWeapons + slot * 4);
            if (vt == vtable::kNoWeaponsObject) return 0;
            if (isVehicleVtable(vt)) {
                addr_t wh = obj + vehicle::kWeaponHandler;
                addr_t owner = resolveHandle(ptr(wh + vehicle_weapon_handler::kOwnerHandle));
                bool delegates = ptr(wh + vehicle_weapon_handler::kDelegateBlock) == 0 &&
                                 resolveHandle(ptr(wh + vehicle_weapon_handler::kHandleA)) && owner &&
                                 ptr(owner + object::kVtable) == vtable::kSoldier;
                if (delegates && depth < 2) return activeWeapon(owner, slot, depth + 1);
                return ptr(wh + vehicle_weapon_handler::kWeapons + slot * 4);
            }
            obj = ptr(obj + object::kParent);
        }
        return 0;
    }

    bool ammoOf(addr_t weaponObj, AmmoInfo& out) const {
        addr_t a = weaponObj ? ptr(weaponObj + weapon::kAmmo) : 0;
        if (!a) return false;
        addr_t begin = ptr(a + ammo::kMagsBegin);
        addr_t end = ptr(a + ammo::kMagsEnd);
        if (!begin || end < begin || !mem_.read(begin, out.inMag)) return false;
        out.magsLeft = static_cast<int>((end - begin) / 4);
        if (addr_t t = ptr(a + ammo::kTemplate)) {
            mem_.read(t + ammo_template::kMagSize, out.magSize);
            mem_.read(t + ammo_template::kNrOfMags, out.nrOfMags);
        }
        return true;
    }

    int tickets(int teamId) const {
        addr_t gl = ptr(exe::kGameLogic);
        int v = 0;
        if (gl) mem_.read(gl + game_logic::Tickets(teamId), v);
        return v;
    }

    // Le um std::string do VS2003 que esta no endereco s (vazio se nao der para ler)
    std::string readString(addr_t s, addr_t maxLen = 64) const {
        if (!s) return {};
        addr_t len = ptr(s + msvc_string::kLength);
        addr_t cap = ptr(s + msvc_string::kCapacity);
        addr_t chars = cap < 16 ? s + msvc_string::kBuffer : ptr(s + msvc_string::kBuffer);
        if (len == 0 || !chars) return {};
        if (len > maxLen) len = maxLen;
        std::string out(len, '\0');
        if (!mem_.readRaw(chars, &out[0], len)) return {};
        return out;
    }

    // Nome de osso pela tabela global (cacheado em memoria local para evitar milhares de RPM)
    std::string boneName(std::int16_t nameId) const {
        if (nameId < 0) return {};
        static std::unordered_map<std::int16_t, std::string> s_BoneNameCache;
        auto it = s_BoneNameCache.find(nameId);
        if (it != s_BoneNameCache.end()) return it->second;

        addr_t begin = ptr(bone_names::kTable + bone_names::kBegin);
        addr_t end = ptr(bone_names::kTable + bone_names::kEnd);
        if (!begin) return {};
        addr_t s = begin + static_cast<addr_t>(nameId) * msvc_string::kSize;
        if (s + msvc_string::kSize > end) return {};
        std::string res = readString(s);
        if (!res.empty()) s_BoneNameCache[nameId] = res;
        return res;
    }

    // Nome do template de um objeto (kit, veiculo, arma...) com cache local
    std::string templateName(addr_t obj) const {
        addr_t tmpl = obj ? ptr(obj + object::kTemplate) : 0;
        if (!tmpl) return std::string();
        static std::unordered_map<addr_t, std::string> s_TemplateCache;
        auto it = s_TemplateCache.find(tmpl);
        if (it != s_TemplateCache.end()) return it->second;
        std::string res = readString(tmpl + object_template::kName);
        if (!res.empty()) s_TemplateCache[tmpl] = res;
        return res;
    }

    addr_t kitOf(addr_t soldierObj) const { return soldierObj ? ptr(soldierObj + soldier::kKit) : 0; }

#pragma pack(push, 1)
    struct RawPlayerFields {
        uint8_t ai;              // 0xD4
        uint8_t alive;           // 0xD5
        uint8_t suicide;         // 0xD6
        uint8_t padD7;           // 0xD7
        int32_t team;            // 0xD8
        uint8_t padDC[12];       // 0xDC..0xE7
        uint8_t manDown;         // 0xE8
        uint8_t padE9[15];       // 0xE9..0xF7
        int32_t ping;            // 0xF8
        uint8_t padFC[16];       // 0xFC..0x10B
        int32_t squadId;         // 0x10C
        uint8_t commander;       // 0x110
        uint8_t squadLeader;     // 0x111
    };
#pragma pack(pop)

    PlayerInfo playerInfo(addr_t pl) const {
        PlayerInfo info;
        if (!pl) return info;

        info.name = readString(pl + player::kName);

        RawPlayerFields raw = {};
        if (mem_.readRaw(pl + player::kAI, &raw, sizeof(raw))) {
            info.ai = (raw.ai != 0);
            info.alive = (raw.alive != 0);
            info.team = raw.team;
            info.manDown = (raw.manDown != 0);
            info.ping = raw.ping;
            info.squad = raw.squadId;
            info.commander = (raw.commander != 0);
            info.squadLeader = (raw.squadLeader != 0);
        } else {
            mem_.read(pl + player::kTeam, info.team);
            mem_.read(pl + player::kSquadId, info.squad);
            mem_.read(pl + player::kPing, info.ping);
            uint8_t b = 0;
            if (mem_.read(pl + player::kAI, b)) info.ai = (b != 0);
            if (mem_.read(pl + player::kAlive, b)) info.alive = (b != 0);
            if (mem_.read(pl + player::kManDown, b)) info.manDown = (b != 0);
        }
        if (info.team == 0) {
            mem_.read(pl + player::kTeam, info.team);
        }

        if (addr_t sc = ptr(pl + player::kScore)) {
            int scBuf[4] = {};
            if (mem_.readRaw(sc + score_block::kScore, scBuf, sizeof(scBuf))) {
                info.score = scBuf[0];
                info.kills = scBuf[1];
                info.deaths = scBuf[2];
                info.teamKills = scBuf[3];
            }
        }

        addr_t sol = soldierOf(pl);
        if (sol) {
            info.kit = templateName(kitOf(sol));
            health(sol, info.hp, info.maxHp);

            uint8_t entDead = 0, entDown = 0;
            if (mem_.read(sol + 0xEC, entDead) && entDead == 1) {
                info.alive = false;
                info.hp = 0.0f;
            } else if (mem_.read(sol + 0xED, entDown) && entDown == 1) {
                info.manDown = true;
            }
        }

        return info;
    }

    // Camera ativa do renderer: *(*(RendDX9 + 0x23B970) + 0xF8). rendBase = base da RendDX9.dll no processo
    addr_t camera(addr_t rendBase) const {
        addr_t owner = rendBase ? ptr(rendBase + rend::kDeviceOwner) : 0;
        return owner ? ptr(owner + rend::device_owner::kCamera) : 0;
    }

    // Projeta um ponto do mundo com a ViewProj da camera (vetor-linha * matriz, convencao D3D).
    // ndc fica em [-1, 1] quando o ponto esta na tela; false se nao der para ler ou se estiver atras da camera.
    // Pixel: x = (ndc.x + 1) / 2 * largura, y = (1 - ndc.y) / 2 * altura
    bool worldToNdc(addr_t rendBase, const float p[3], float ndc[2]) const {
        addr_t cam = camera(rendBase);
        float m[16];
        if (!cam || !mem_.read(cam + rend::camera::kViewProj, m)) return false;
        float x = p[0] * m[0] + p[1] * m[4] + p[2] * m[8] + m[12];
        float y = p[0] * m[1] + p[1] * m[5] + p[2] * m[9] + m[13];
        float wc = p[0] * m[3] + p[1] * m[7] + p[2] * m[11] + m[15];
        if (wc < 0.001f) return false;
        ndc[0] = x / wc;
        ndc[1] = y / wc;
        return true;
    }

    // Postura do soldado (soldier::kPose); -1 se nao der para ler
    int pose(addr_t soldierObj) const {
        int v = -1;
        if (!soldierObj || !mem_.read(soldierObj + soldier::kPose, v)) return -1;
        return v;
    }

    // Velocidade (m/s) pela fisica. Entende as duas classes de fisica conhecidas (getVelocity 0x6EFF90 e
    // 0x6EB390); para outras devolve false em vez de chutar um offset
    bool velocity(addr_t obj, float v[3]) const {
        addr_t phys = obj ? ptr(obj + object::kPhysics) : 0;
        addr_t vt = phys ? ptr(phys) : 0;
        addr_t fn = vt ? ptr(vt + object::physics_vfn::kGetVelocity) : 0;
        addr_t src = 0;
        if (fn == object::soldier_physics::kGetVelocityFn) {
            addr_t owner = ptr(phys + object::soldier_physics::kOwner);
            if (owner && ptr(owner + object::kParent)) {  // preso a um assento: o jogo devolve zero
                v[0] = v[1] = v[2] = 0.0f;
                return true;
            }
            src = phys + object::soldier_physics::kVelocity;
        } else if (fn == object::composite_physics::kGetVelocityFn) {
            src = ptr(phys + object::composite_physics::kVelocityPtr);  // pode estar um quadro atrasado
        }
        return src && mem_.read(src, v[0]) && mem_.read(src + 4, v[1]) && mem_.read(src + 8, v[2]);
    }

    // Direcao horizontal para onde o objeto aponta, em graus (0 = +Z, 90 = +X), pela linha Z da matriz de mundo
    bool yawDegrees(addr_t obj, float& yaw) const {
        float m[16];
        if (!obj || !mem_.read(obj + object::kWorldMatrix, m)) return false;
        yaw = std::atan2(m[8], m[10]) * 57.29578f;
        return true;
    }

    // Vida de qualquer objeto com Armor (soldado, veiculo...): atual e maxima
    bool health(addr_t obj, float& hp, float& maxHp) const {
        hp = maxHp = 0.0f;
        addr_t a = obj ? ptr(obj + object::kArmor) : 0;
        addr_t t = a ? ptr(a + armor::kTemplate) : 0;
        return a && t && mem_.read(a + armor::kHitPoints, hp) && mem_.read(t + armor_template::kMaxHitPoints, maxHp);
    }

    // Qual esqueleto o jogo esta animando neste soldado (0 = 1P, 1 = 3P; -1 se nao der para ler)
    int activeSkeleton(addr_t soldierObj) const {
        int v = -1;
        if (!soldierObj || !mem_.read(soldierObj + soldier::kActiveSkeleton, v) || v < 0 || v > 1) return -1;
        return v;
    }

    // Ossos do esqueleto com posicao no mundo. which: 0 = 1P, 1 = 3P (ver soldier::kSkeletons).
    // posed (opcional) = false quando todas as translacoes sao 0, ou seja, o jogo nao calculou a pose
    // (soldados longe); nesse caso todos os ossos ficam na origem do soldado.
    bool bones(addr_t soldierObj, std::vector<BoneInfo>& out, int which = 1, bool* posed = nullptr) const {
        out.clear();
        if (posed) *posed = false;
        addr_t skel = soldierObj ? ptr(soldierObj + soldier::kSkeletons + which * soldier::kSkeletonStride) : 0;
        if (!skel) return false;
        int count = 0;
        addr_t defs = ptr(skel + skeleton::kBoneDefs);
        addr_t mats = ptr(skel + skeleton::kMatrices);
        if (!mem_.read(skel + skeleton::kBoneCount, count) || count <= 0 || count > 256 || !defs || !mats)
            return false;
        float w[16];
        if (!mem_.read(soldierObj + object::kWorldMatrix, w)) return false;

        // Bulk RPM read for all definitions and matrices (2 syscalls instead of 240)
        std::vector<uint8_t> defsBuf(count * skeleton::kBoneDefSize);
        if (!mem_.readRaw(defs, defsBuf.data(), defsBuf.size())) return false;

        std::vector<uint8_t> matsBuf(count * skeleton::kMatrixSize);
        if (!mem_.readRaw(mats, matsBuf.data(), matsBuf.size())) return false;

        out.reserve(count);
        for (int i = 0; i < count; ++i) {
            const uint8_t* defPtr = &defsBuf[i * skeleton::kBoneDefSize];
            std::int16_t nameId = *reinterpret_cast<const std::int16_t*>(defPtr + skeleton::kDefNameId);
            std::int16_t parent = *reinterpret_cast<const std::int16_t*>(defPtr + skeleton::kDefParent);

            const uint8_t* matPtr = &matsBuf[i * skeleton::kMatrixSize];
            const float* t = reinterpret_cast<const float*>(matPtr + skeleton::kMatrixTranslation);

            BoneInfo b;
            b.name = boneName(nameId);
            b.parent = parent;
            if (t[0] != 0.0f || t[1] != 0.0f || t[2] != 0.0f) {
                if (posed) *posed = true;
            }
            if (t[0] * t[0] + t[1] * t[1] + t[2] * t[2] >
                skeleton::kMaxModelSpaceDistance * skeleton::kMaxModelSpaceDistance) {
                for (int k = 0; k < 3; ++k) b.world[k] = t[k];  // osso auxiliar que ja guarda posicao de mundo
            } else {
                // vetor-linha * matriz (convencao D3D): t.x*linha0 + t.y*linha1 + t.z*linha2 + linha3
                for (int k = 0; k < 3; ++k) b.world[k] = t[0] * w[k] + t[1] * w[4 + k] + t[2] * w[8 + k] + w[12 + k];
            }
            out.push_back(std::move(b));
        }
        return true;
    }

    // Indice do osso pelo nome ("head", "neck", "torso"...); -1 se nao existir
    static int findBone(const std::vector<BoneInfo>& list, const char* name) {
        for (std::size_t i = 0; i < list.size(); ++i)
            if (list[i].name == name) return static_cast<int>(i);
        return -1;
    }

private:
    bool isNode(addr_t node, addr_t head) const {
        std::uint8_t nil = 1;
        return node && node != head && mem_.read(node + map_node::kIsNil, nil) && nil == 0;
    }

    static bool isVehicleVtable(addr_t vt) {
        for (addr_t v : vtable::kVehicles)
            if (v == vt) return true;
        return false;
    }

    const Mem& mem_;
};

}  // namespace prbf2

