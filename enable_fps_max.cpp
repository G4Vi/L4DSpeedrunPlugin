#include <cstdlib>
#include "eiface.h"
#include "icvar.h"
#include "tier1/iconvar.h"
#include "tier1/convar.h"
#include <Windows.h>

#pragma comment(lib, "legacy_stdio_definitions.lib")

// memdbgon must be the last include file in a .cpp file!!!
//#include "tier0/memdbgon.h"

class EnableFPSMax: public IServerPluginCallbacks
{
public:
	EnableFPSMax();
	// IServerPluginCallbacks methods
	virtual bool			Load(	CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory );
	virtual void			Unload( void );
	virtual void			Pause( void );
	virtual void			UnPause( void );
	virtual const char     *GetPluginDescription( void );      
	virtual void			LevelInit( char const *pMapName );
	virtual void			ServerActivate( edict_t *pEdictList, int edictCount, int clientMax );
	virtual void			GameFrame( bool simulating );
	virtual void			LevelShutdown( void );
	virtual void			ClientActive( edict_t *pEntity );
	virtual void			ClientDisconnect( edict_t *pEntity );
	virtual void			ClientPutInServer( edict_t *pEntity, char const *playername );
	virtual void			SetCommandClient( int index );
	virtual void			ClientSettingsChanged( edict_t *pEdict );
	virtual PLUGIN_RESULT	ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen );
	virtual PLUGIN_RESULT	ClientCommand( edict_t *pEntity, const CCommand &args );
	virtual PLUGIN_RESULT	NetworkIDValidated( const char *pszUserName, const char *pszNetworkID );
	virtual void			OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue );

	// added with version 3 of the interface.
	virtual void			OnEdictAllocated( edict_t *edict );
	virtual void			OnEdictFreed( const edict_t *edict  );	
	private:
		ConVar * fps_max_cvar;
};


EnableFPSMax g_EnableFPSMaxPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(EnableFPSMax, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_EnableFPSMaxPlugin );

EnableFPSMax::EnableFPSMax() :  fps_max_cvar(NULL)
{

}



typedef enum GameTimerGTS {
	GTS_NOTRUNNING,
	GTS_NOTRUNNING_LOADING,
	GTS_NOTRUNNING_LOADED,
	GTS_RUNNING,
	GTS_RUNNING_SCOREBOARD,
	GTS_RUNNING_LEVELLOADING,
	GTS_RUNNING_NOCAMPAIGN,
	GTS_RUNNING_CAMPAIGNLOADING,
	GTS_RUNNING_LOADED,
}GameTimerGTS;

static const bool *GameLoading;
//static const bool *HasControl;
static const bool *ScoreboardLoad;
const unsigned char *Client;

GameTimerGTS GTS;

static const char *GetGTS(void)
{
	switch (GTS) 
	{
	case GTS_NOTRUNNING: return "GTS_NOTRUNNING";
	case GTS_NOTRUNNING_LOADING: return "GTS_NOTRUNNING_LOADING";
	case GTS_NOTRUNNING_LOADED: return "GTS_NOTRUNNING_LOADED";
	case GTS_RUNNING: return "GTS_RUNNING";	
	case GTS_RUNNING_SCOREBOARD: return "GTS_RUNNING_SCOREBOARD";
	case GTS_RUNNING_LEVELLOADING: return "GTS_RUNNING_LEVELLOADING";
	case GTS_RUNNING_NOCAMPAIGN: return "GTS_RUNNING_NOCAMPAIGN";
	case GTS_RUNNING_CAMPAIGNLOADING: return "GTS_RUNNING_LOADED";
	case GTS_RUNNING_LOADED: return "GTS_RUNNING_LOADED";
	};
	return "GTS_UNKNOWN";
}


DWORD StartTime = 0;
DWORD SumLoads = 0;
DWORD CurrentCampaign;

DWORD SplitStartTime;
DWORD LoadStartTime;

DWORD CampaignStartTime;
DWORD CampaignSumLoads;

unsigned NumCampaigns = 1;

static const bool HasControl(void) 
{
	/*const unsigned char *clientTemp = (Client + 0x574950);
	const unsigned char *client2 = *(unsigned char **)clientTemp;
	DevMsg("clientTemp %p\n", clientTemp);
	DevMsg("client2 %p\n", client2);
	return *(const bool*)(client2 + 0xC);*/


	return  *(const bool*)((*(unsigned char**)(Client + 0x574950)) + 0xC);
}

void Setme_Dump_Status(const CCommand &args)
{
	Msg("GameLoading %u HasControl %u, ScoreboardLoad %u\n", *GameLoading, HasControl(), *ScoreboardLoad);
	Msg("%s StartTime %u SumLoads %u CurrentCampaign %u SplitStartTime %u LoadStartTime %u CampaignStartTime %u CampaignSumLoads %u NumCampaigns %u\n", GetGTS(), StartTime, SumLoads, CurrentCampaign, SplitStartTime, LoadStartTime, CampaignStartTime, CampaignSumLoads, NumCampaigns);
}
static ConCommand setme_dump_status("setme_dump_status", Setme_Dump_Status, "dumps status", 0);

void Setme_Reset(const CCommand &args)
{
	Msg("Resetting Run\n");
	GTS = GTS_NOTRUNNING;
	SumLoads = 0;
	CurrentCampaign = 0;

}
static ConCommand setme_reset("setme_reset", Setme_Reset, "resets run", 0);


typedef void*(__thiscall *FINDENTITYBYCLASSNAME)(void*, void *, const char*);
FINDENTITYBYCLASSNAME FindEntityByClassname;
unsigned char *gEntList;

typedef string_t(__thiscall *CBASEENTITYGETMODELNAME)(void*);
CBASEENTITYGETMODELNAME CBaseEntityGetModelName;

typedef const char*(__thiscall *CBASEENTITYGETSERVERCLASS)(void*);
CBASEENTITYGETSERVERCLASS CBaseEntityGetServerClass;

typedef void(__thiscall *CBASEENTITYTHINK)(void*);
CBASEENTITYTHINK oldCBaseEntityThink;
CBASEENTITYTHINK CBaseEntityThink;
struct Dumb
{
	void Think(void)
	{
		Msg("Thinking hard\n");
		oldCBaseEntityThink(this);
	}
	
};





/*
CBASEENTITYTHINKA CBaseEntityThink;
typedef void(__thiscall Dumb::*CBASEENTITYTHINKA)(void*);
*/

#include<stdint.h>
void Setme_FindEntityByClassname(const CCommand &args)
{
	if (args.ArgC() < 2)
	{
		Warning("bad febc");
		return;
	}
	Msg("Finding entity with classname %s\n",  args[1]);
	void *last = FindEntityByClassname(gEntList, NULL, args[1]);
	while (last)
	{
		Msg("Entity: 0x%p\n", last);
		if (strcmp(args[1], "point_viewcontrol_survivor") == 0)
		{
			DWORD vtable = *(DWORD*)last;
			Msg("CSurvivorCamera vtable at %p\n", vtable);
			//1.0 offsets
			/*CBaseEntityGetModelName = *(CBASEENTITYGETMODELNAME*)(vtable + (0x7 * sizeof(DWORD)));
			Msg("GetModelName at %p\n", CBaseEntityGetModelName);
			string_t val = CBaseEntityGetModelName(last);
			// CBaseEntityGetModelName(last));*/

			/*
			CBaseEntityGetServerClass = *(CBASEENTITYGETSERVERCLASS*)(vtable + (0x9 * sizeof(DWORD)));
			Msg("serverclass at %p\n", CBaseEntityGetServerClass(last));
			*/
			static bool done = false;
			if (!done)
			{
				oldCBaseEntityThink = *(CBASEENTITYTHINK*)(vtable + (47 * sizeof(DWORD)));
				Msg("Think Func at %p\n", oldCBaseEntityThink);				
				auto func = &Dumb::Think;
				CBaseEntityThink = (CBASEENTITYTHINK&)func;
				SIZE_T dumb;
				DWORD oldProtect = 0;
				LPVOID addr = (LPVOID)(vtable + (47 * sizeof(DWORD))); 
				VirtualProtectEx(GetCurrentProcess(), addr, 256, PAGE_EXECUTE_READWRITE, &oldProtect);
				if (WriteProcessMemory(GetCurrentProcess(), addr, &CBaseEntityThink, 4, &dumb) == 0)
				{
					Msg("WPM failed %d\n", GetLastError());
				}
				else
				{
					Msg("WPM success %d\n", GetLastError());
				}
				VirtualProtectEx(GetCurrentProcess(), addr, 256, oldProtect, NULL);
				done = true;
			}
			


		}
		last = FindEntityByClassname(gEntList, last, args[1]);
	}	
}
static ConCommand setme_fbec("setme_fbec", Setme_FindEntityByClassname, "FindEntityByClassname", 0);




static void OnCampaignStart(DWORD curtime)
{
	SplitStartTime = curtime;
	CampaignStartTime = curtime;
	GTS = GTS_RUNNING;
}

//---------------------------------------------------------------------------------
// Purpose: called once per server frame, do recurring work here (like checking for timeouts)
//---------------------------------------------------------------------------------
void EnableFPSMax::GameFrame(bool simulating)
{
	if (GTS == GTS_NOTRUNNING)
	{
		if (*GameLoading) GTS = GTS_NOTRUNNING_LOADING;
	}
	else if (GTS == GTS_NOTRUNNING_LOADING)
	{
		if (! *GameLoading) GTS = GTS_NOTRUNNING_LOADED;
	}
	else if (GTS == GTS_NOTRUNNING_LOADED)
	{
		if (HasControl())
		{
			StartTime = GetTickCount();
			OnCampaignStart(StartTime);
			Msg("Started Campaign at %u\n", StartTime);
		}
	}
	else if (GTS == GTS_RUNNING)
	{
		if (*ScoreboardLoad)
		{
			DWORD curtime = GetTickCount();
			Msg("Split time %u\n", curtime - SplitStartTime);
			SplitStartTime = curtime;
			GTS = GTS_RUNNING_SCOREBOARD;
		}
		else if (!HasControl())
		{
DWORD curtime = GetTickCount();
SumLoads += CampaignSumLoads;
Msg("Split time %u\n", curtime - SplitStartTime);
Msg("Campaign %u took %u\n", CurrentCampaign, (curtime - CampaignStartTime) - CampaignSumLoads);
if ((CurrentCampaign + 1) == NumCampaigns)
{
	Msg("Run took %u\n", curtime - StartTime - SumLoads);
	SumLoads = 0;
	CurrentCampaign = 0;
	GTS = GTS_NOTRUNNING;
	return;
}
SplitStartTime = curtime;
GTS = GTS_RUNNING_NOCAMPAIGN;
		}
	}
	else if (GTS == GTS_RUNNING_SCOREBOARD)
	{
	if (*GameLoading)
	{
		LoadStartTime = GetTickCount();
		GTS = GTS_RUNNING_LEVELLOADING;
	}
	}
	else if (GTS == GTS_RUNNING_LEVELLOADING)
	{
	if (HasControl())
	{
		CampaignSumLoads += (GetTickCount() - LoadStartTime);
		GTS = GTS_RUNNING;
	}
	}
	else if (GTS == GTS_RUNNING_NOCAMPAIGN)
	{
	if (*GameLoading)
	{
		LoadStartTime = GetTickCount();
		GTS = GTS_RUNNING_CAMPAIGNLOADING;
	}
	}
	else if (GTS == GTS_RUNNING_CAMPAIGNLOADING)
	{
	if (!*GameLoading)
	{
		uint64 curtime = GetTickCount();
		SumLoads += curtime - LoadStartTime;
		CurrentCampaign++;
		CampaignSumLoads = 0;
		GTS = GTS_RUNNING_LOADED;
	}
	}
	else if (GTS == GTS_RUNNING_LOADED)
	{
	if (HasControl())
	{
		OnCampaignStart(GetTickCount());
	}
	}
}


unsigned  char* hex_decode(const char *in, size_t len, unsigned char *out)
{
	unsigned int i, t, hn, ln;

	for (t = 0, i = 0; i < len; i += 2, ++t) {

		hn = in[i] > '9' ? in[i] - 'A' + 10 : in[i] - '0';
		ln = in[i + 1] > '9' ? in[i + 1] - 'A' + 10 : in[i + 1] - '0';

		out[t] = (hn << 4) | ln;
	}

	return out;
}

bool isPageOK(void *address)
{
	MEMORY_BASIC_INFORMATION mbi = { 0 };
	unsigned char *pAddress = NULL, *pEndRegion = NULL;
	DWORD dwProtectionMask = PAGE_READONLY | PAGE_EXECUTE_WRITECOPY | PAGE_READWRITE | PAGE_WRITECOMBINE;
	while (sizeof(mbi) == VirtualQuery(pEndRegion, &mbi, sizeof(mbi))) {
		pAddress = pEndRegion;
		pEndRegion += mbi.RegionSize;
		if ((mbi.AllocationProtect & dwProtectionMask) && (mbi.State & MEM_COMMIT))
		{
			return true;
		}
	}
	return false;
}
#include <Psapi.h>
unsigned char *findSig(void *start, const char *bytes)
{
	MODULEINFO mi;
	BOOL res = GetModuleInformation(GetCurrentProcess(), (HMODULE)start, &mi, sizeof(mi));
	if (!res) return NULL;
	unsigned char *sbytes = (unsigned char*)start;
	unsigned numbytes = strlen(bytes) / 2;
	unsigned i;

DO_IT_AGAIN:
	for (i = 0; i < (numbytes * 2); i += 2)
	{
		if ((((DWORD)&sbytes[i]) - (DWORD)start) >=( mi.SizeOfImage))
		{
			DevMsg("sbytes %p i %u\n", sbytes, i);
			return NULL;
		}
		if ((bytes[i] == '?') && (bytes[i + 1] == '?')) continue;		
		unsigned char out;
		hex_decode(&bytes[i], 2, &out);		
		if (out == sbytes[i/2]) continue;	
		sbytes += 1;
		goto DO_IT_AGAIN;
	}
	if (i != 0) return sbytes;
	return NULL;
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is loaded, load the interface we need from the engine
//---------------------------------------------------------------------------------
bool EnableFPSMax::Load( CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory )
{
	
	ICvar * pCvars = reinterpret_cast<ICvar *>(interfaceFactory(CVAR_INTERFACE_VERSION,NULL));	
	if(pCvars == NULL)
	{
		Warning("FPS Max Enabler: Failed to get Cvar interface.\n");
		return false;
	}
	DevMsg("FPS Max Enabler: Found CVar interface %08x\n", pCvars);

	char *serverdll = (char*)GetModuleHandle("server.dll");

	if (serverdll == NULL)
	{
		Warning("FPS Max Enabler: Failed to find serverdll\n");
		return false;
	}
	const unsigned char *LevelShutdown = findSig(serverdll, "E8????????E8????????B9????????E8????????E8");
	if (LevelShutdown == NULL)
	{
		Warning("FPS Max Enabler: Failed to find LevelShutdown\n");
		return false;
	}
	DevMsg("FPS Max Enabler: Found LevelShutdown 0x%p\n", LevelShutdown);	
	
	memcpy(&gEntList, LevelShutdown + 11, 4);
	DevMsg("FPS Max Enabler: Found gEntList at 0x%p\n", gEntList);

    FindEntityByClassname = (FINDENTITYBYCLASSNAME)findSig(serverdll, "5355568BF18B4C241085C95774198B018B5008FFD28B0025FF0F000083C001C1E0048B3C30EB068BBE????????85FF74398B5C24188B2D????????EB03");
	if (FindEntityByClassname == NULL)
	{
		Warning("FPS Max Enabler: Failed to find FindEntityByClassname\n");
		return false;
	}
	DevMsg("FPS Max Enabler: Found FindEntityByClassname at 0x%p\n", FindEntityByClassname);

	/*
	fps_max_cvar = pCvars->FindVar("fps_max");
	if(fps_max_cvar == NULL)
	{
		Warning("FPS Max Enabler: Failed to find fps_max command");
		return false;
	}
	DevMsg("FPS Max Enabler: Found fps_max cvar %08x\n", fps_max_cvar);
	
	fps_max_cvar->RemoveFlags(FCVAR_DEVELOPMENTONLY);
	DevMsg("FPS Max Enabler : FCVAR_REPLICATED %u\n", fps_max_cvar->IsFlagSet(FCVAR_REPLICATED));
	*/

	GTS = GTS_NOTRUNNING;

	const HANDLE engine = GetModuleHandle("engine");
	Client = (const unsigned char*)GetModuleHandle("client");
	DevMsg("Engine at 0x%p, Client at 0x%p\n", engine, Client);
	if ((engine == NULL) || (Client == NULL)) return false;

	//1.0
	GameLoading = (const bool*)(((const unsigned char*)engine) + 0x5D1E6C);	
	ScoreboardLoad = (const bool*)(Client + 0x5900E9);

	
	

	// register our console commands
	pCvars->RegisterConCommand(&setme_dump_status);
	pCvars->RegisterConCommand(&setme_reset);
	pCvars->RegisterConCommand(&setme_fbec);

	return true;
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unloaded (turned off)
//---------------------------------------------------------------------------------
void EnableFPSMax::Unload( void )
{
    
	//fps_max_cvar->AddFlags(FCVAR_DEVELOPMENTONLY);
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is paused (i.e should stop running but isn't unloaded)
//---------------------------------------------------------------------------------
void EnableFPSMax::Pause( void )
{
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unpaused (i.e should start executing again)
//---------------------------------------------------------------------------------
void EnableFPSMax::UnPause( void )
{
}

//---------------------------------------------------------------------------------
// Purpose: the name of this plugin, returned in "plugin_print" command
//---------------------------------------------------------------------------------
const char *EnableFPSMax::GetPluginDescription( void )
{
	

	//return "fps_max Enabler 1.0, ProdigySim";
	return "SET ME 1.0, G4Vi";
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void EnableFPSMax::LevelInit( char const *pMapName )
{
	DevMsg("GameLoading %u HasControl %u, ScoreboardLoad %u\n", *GameLoading, HasControl(), *ScoreboardLoad);
	//DevMsg("FPS Max Enabler : LevelInit\n");
}

//---------------------------------------------------------------------------------
// Purpose: called on level start, when the server is ready to accept client connections
//		edictCount is the number of entities in the level, clientMax is the max client count
//---------------------------------------------------------------------------------
void EnableFPSMax::ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level end (as the server is shutting down or going to a new map)
//---------------------------------------------------------------------------------
void EnableFPSMax::LevelShutdown( void ) // !!!!this can get called multiple times per map change
{
	DevMsg("SET ME : LevelShutdown\n");
}

//---------------------------------------------------------------------------------
// Purpose: called when a client spawns into a server (i.e as they begin to play)
//---------------------------------------------------------------------------------
void EnableFPSMax::ClientActive( edict_t *pEntity )
{
	DevMsg("SET ME : ClientActive\n");
}

//---------------------------------------------------------------------------------
// Purpose: called when a client leaves a server (or is timed out)
//---------------------------------------------------------------------------------
void EnableFPSMax::ClientDisconnect( edict_t *pEntity )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on 
//---------------------------------------------------------------------------------
void EnableFPSMax::ClientPutInServer( edict_t *pEntity, char const *playername )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void EnableFPSMax::SetCommandClient( int index )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void EnableFPSMax::ClientSettingsChanged( edict_t *pEdict )
{
}

//---------------------------------------------------------------------------------
// Purpose: called when a client joins a server
//---------------------------------------------------------------------------------
PLUGIN_RESULT EnableFPSMax::ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a client types in a command (only a subset of commands however, not CON_COMMAND's)
//---------------------------------------------------------------------------------
PLUGIN_RESULT EnableFPSMax::ClientCommand( edict_t *pEntity, const CCommand &args )
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a client is authenticated
//---------------------------------------------------------------------------------
PLUGIN_RESULT EnableFPSMax::NetworkIDValidated( const char *pszUserName, const char *pszNetworkID )
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a cvar value query is finished
//---------------------------------------------------------------------------------
void EnableFPSMax::OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue )
{
}
void EnableFPSMax::OnEdictAllocated( edict_t *edict )
{
}
void EnableFPSMax::OnEdictFreed( const edict_t *edict  )
{
}
