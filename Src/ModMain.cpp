#include <Prey/CrySystem/File/ICryPak.h>
#include <Prey/GameDll/ark/player/ArkPlayerStatusComponent.h>
#include <Prey/GameDll/ark/player/ArkPlayerComponent.h>
#include <Prey/GameDll/ark/player/trauma/ArkStatusGoodDrunk.h>
#include <Prey/GameDll/ark/player/trauma/ArkStatusWellFed.h>
#include <Prey/GameDll/ark/player/trauma/ArkTraumaBase.h>
#include <Prey/GameDll/ark/player/trauma/ArkTraumaBleeding.h>
#include <Prey/GameDll/ark/player/trauma/ArkTraumaBurns.h>
#include <Prey/GameDll/ark/player/trauma/ArkTraumaConcussion.h>
#include <Prey/GameDll/ark/player/trauma/ArkTraumaDisruption.h>
#include <Prey/GameDll/ark/player/trauma/ArkTraumaDrunk.h>
#include <Prey/GameDll/ark/player/trauma/ArkTraumaFear.h>
#include <Prey/GameDll/ark/player/trauma/ArkTraumaHobbled.h>
#include <Prey/GameDll/ark/player/trauma/ArkTraumaPsychoshock.h>
#include <Prey/GameDll/ark/player/trauma/ArkTraumaRadiation.h>
#include <Prey/GameDll/ark/player/trauma/ArkTraumaSuitIntegrity.h>
#include <Chairloader/IChairXmlUtils.h>
#include "ModMain.h"

ModMain* gMod = nullptr;

//! Trauma class info loaded from XML.
struct TraumaClass
{
    uint64_t id = 0;
    string name;
    string className;
};

static auto s_ArkPlayerComponent_LoadConfig_Hook = ArkPlayerComponent::FLoadConfig.MakeHook();

//! Really hacky code to call a constructor from Prey without rewriting it.
template <typename T, typename TFunc, typename... TArgs>
static std::unique_ptr<T> HackyCode(TFunc& pfn, TArgs&& ...args)
{
    T* ptr = std::launder(reinterpret_cast<T*>(new char[sizeof(T)]));
    pfn(ptr, std::forward<TArgs>(args)...);

    // Default deleter will call the virtual destructor.
    // vtable is set by Prey so it should just work (tm).
    return std::unique_ptr<T>(ptr);
}

static void ArkPlayerComponent_LoadConfig_Hook(ArkPlayerComponent* const _this, const XmlNodeRef& _node)
{
    gMod->InstantiateTraumas(_this->m_pStatusComponent.get());
    s_ArkPlayerComponent_LoadConfig_Hook.InvokeOrig(_this, _node);
}

//! Factory function to create a trauma instance dynamically.
static std::unique_ptr<ArkTraumaBase> InstantiateTrauma(const TraumaClass& trauma, EArkPlayerStatus status)
{
    std::unique_ptr<ArkTraumaBase> result;

    if (trauma.className == "ArkStatusGoodDrunk")
        result = HackyCode<ArkStatusGoodDrunk>(ArkStatusGoodDrunk::FArkStatusGoodDrunkOv1);
    else if (trauma.className == "ArkStatusWellFed")
        result = HackyCode<ArkStatusWellFed>(ArkStatusWellFed::FArkStatusWellFedOv2);
    else if (trauma.className == "ArkTraumaBase")
        result = HackyCode<ArkTraumaBase>(ArkTraumaBase::FArkTraumaBaseOv1, trauma.id, status);
    else if (trauma.className == "ArkTraumaBleeding")
        result = HackyCode<ArkTraumaBleeding>(ArkTraumaBleeding::FArkTraumaBleedingOv1);
    else if (trauma.className == "ArkTraumaBurns")
        result = HackyCode<ArkTraumaBurns>(ArkTraumaBurns::FArkTraumaBurnsOv2);
    else if (trauma.className == "ArkTraumaConcussion")
        result = HackyCode<ArkTraumaConcussion>(ArkTraumaConcussion::FArkTraumaConcussionOv2);
    else if (trauma.className == "ArkTraumaDisruption")
        result = HackyCode<ArkTraumaDisruption>(ArkTraumaDisruption::FArkTraumaDisruptionOv2);
    else if (trauma.className == "ArkTraumaDrunk")
        result = HackyCode<ArkTraumaDrunk>(ArkTraumaDrunk::FArkTraumaDrunkOv1);
    else if (trauma.className == "ArkTraumaFear")
        result = HackyCode<ArkTraumaFear>(ArkTraumaFear::FArkTraumaFearOv1);
    else if (trauma.className == "ArkTraumaHobbled")
        result = HackyCode<ArkTraumaHobbled>(ArkTraumaHobbled::FArkTraumaHobbledOv1);
    else if (trauma.className == "ArkTraumaPsychoshock")
        result = HackyCode<ArkTraumaPsychoshock>(ArkTraumaPsychoshock::FArkTraumaPsychoshockOv2);
    else if (trauma.className == "ArkTraumaRadiation")
        result = HackyCode<ArkTraumaRadiation>(ArkTraumaRadiation::FArkTraumaRadiationOv2);
    else if (trauma.className == "ArkTraumaSuitIntegrity")
        result = HackyCode<ArkTraumaSuitIntegrity>(ArkTraumaSuitIntegrity::FArkTraumaSuitIntegrityOv2);
    
    if (result)
    {
        const_cast<uint64_t&>(result->m_id) = trauma.id;
        const_cast<EArkPlayerStatus&>(result->m_status) = status;
    }
    else
    {
        CryError("Unknown class {}", trauma.className);
    }

    return result;
}

void ModMain::InstantiateTraumas(ArkPlayerStatusComponent* _this)
{
    std::vector<TraumaClass> traumas = LoadTraumaList();

    // Process traumas that already been instantiated by the game
    for (auto& i : _this->m_statuses)
    {
        auto existingIt = std::find_if(traumas.begin(), traumas.end(), [&](const TraumaClass& x) { return x.id == i->m_id; });

        if (existingIt == traumas.end())
            continue;

        if (gEnv->pSystem->IsDevMode())
            CryLog("Trauma {} [{}]: already instantiated by the base game", existingIt->name, existingIt->id);
        traumas.erase(existingIt);
    }

    // Instantiate new traumas
    for (int i = 0; i < (int)traumas.size(); i++)
    {
        EArkPlayerStatus status = (EArkPlayerStatus)(100 + i);

        if (gEnv->pSystem->IsDevMode())
            CryLog("Trauma {} [{}]: instantiating class {} (enum = {})", traumas[i].name, traumas[i].id, traumas[i].className, (int)status);

        std::unique_ptr<ArkTraumaBase> instance = InstantiateTrauma(traumas[i], status);

        if (!instance)
        {
            CryError("Failed to instantiate {} [{}]", traumas[i].name, traumas[i].id);
            continue;
        }

        _this->m_statuses.push_back(std::move(instance));
    }
}

//---------------------------------------------------------------------------------
// Mod Initialization
//---------------------------------------------------------------------------------
void ModMain::FillModInfo(ModDllInfoEx& info)
{
    info.modName = "tmp64.CustomTraumas";
    info.logTag = "CustomTraumas";
    info.supportsHotReload = false;
}

void ModMain::InitHooks()
{
    s_ArkPlayerComponent_LoadConfig_Hook.SetHookFunc(ArkPlayerComponent_LoadConfig_Hook);
}

void ModMain::InitSystem(const ModInitInfo& initInfo, ModDllInfo& dllInfo)
{
    BaseClass::InitSystem(initInfo, dllInfo);
    // Your code goes here
}

void ModMain::InitGame(bool isHotReloading)
{
    BaseClass::InitGame(isHotReloading);
    // Your code goes here
}

//---------------------------------------------------------------------------------
// Mod Shutdown
//---------------------------------------------------------------------------------
void ModMain::ShutdownGame(bool isHotUnloading)
{
    // Your code goes here
    BaseClass::ShutdownGame(isHotUnloading);
}

void ModMain::ShutdownSystem(bool isHotUnloading)
{
    // Your code goes here
    BaseClass::ShutdownSystem(isHotUnloading);
}

//---------------------------------------------------------------------------------
// GUI
//---------------------------------------------------------------------------------
void ModMain::Draw()
{
}

//---------------------------------------------------------------------------------
// Main Update Loop
//---------------------------------------------------------------------------------
void ModMain::MainUpdate(unsigned updateFlags)
{
    // Your code goes here
}

std::vector<TraumaClass> ModMain::LoadTraumaList()
{
    std::vector<TraumaClass> list;
    _finddata_t c_file;
    intptr_t hFile;
    std::string baseDir = "Libs/CustomTraumas";

    if ((hFile = gEnv->pCryPak->FindFirst((baseDir + "/*.xml").c_str(), &c_file)) == -1L)
        return list;

    do
    {
        if (_stricmp(c_file.name, "..") == 0)
            continue;
        if (_stricmp(c_file.name, ".") == 0)
            continue;
        if (c_file.attrib & _A_SUBDIR)
            continue;

        std::string filePath = baseDir + "/" + c_file.name;
        pugi::xml_document doc = gCL->pXmlUtils->LoadXmlFromFile(filePath.c_str());

        for (pugi::xml_node node : doc.first_child().children())
        {
            TraumaClass item;
            item.id = node.attribute("ID").as_ullong();
            item.name = node.attribute("Name").as_string();
            item.className = node.attribute("Class").as_string();
            CryLog("Trauma {}/{} [{}] = {}", c_file.name, item.name, item.id, item.className);

            if (item.id == 0)
            {
                CryError("Trauma {}/{}: ID is invalid", c_file.name, item.name);
                continue;
            }

            if (item.name.empty())
            {
                CryError("Trauma {}/{}: Name is invalid", c_file.name, item.name);
                continue;
            }

            if (item.className.empty())
            {
                CryError("Trauma {}/{}: Class is invalid", c_file.name, item.name);
                continue;
            }

            list.emplace_back(item);
        }
    } while (gEnv->pCryPak->FindNext(hFile, &c_file) == 0);

    return list;
}

//---------------------------------------------------------------------------------
// Mod Interfacing
//---------------------------------------------------------------------------------
#ifdef EXAMPLE
void *ModMain::QueryInterface(const char *ifaceName)
{
    // This function is called by other mods to get interfaces.
    // Check if you implement the interface. If so, return a pointer to it.
    // This example assumes ModMain inherits from the interface class. You may choose a different object if you want.
    if (!strcmp(ifaceName, IModName::IFACE_NAME))
        return static_cast<IModName*>(this);

    return nullptr;
}

void ModMain::Connect(const std::vector<IChairloaderMod *> &mods)
{
    IModName* pModName = nullptr; // Add this to ModMain

    // You can use one of these helper functions. They will handle finding and version checking.
    pModName = GetInterfaceFromModList<IModName>(mods, "Other Mod Name");
    pModName = GetRequiredInterfaceFromModList<IModName>(mods, "Other Mod Name");
}
#endif

//---------------------------------------------------------------------------------
// Exported Functions
//---------------------------------------------------------------------------------
extern "C" DLL_EXPORT IChairloaderMod* ClMod_Initialize()
{
    CRY_ASSERT(!gMod);
    gMod = new ModMain();
    return gMod;
}

extern "C" DLL_EXPORT void ClMod_Shutdown()
{
    CRY_ASSERT(gMod);
    delete gMod;
    gMod = nullptr;
}

// Validate that declarations haven't changed
static_assert(std::is_same_v<decltype(ClMod_Initialize), IChairloaderMod::ProcInitialize>);
static_assert(std::is_same_v<decltype(ClMod_Shutdown), IChairloaderMod::ProcShutdown>);
