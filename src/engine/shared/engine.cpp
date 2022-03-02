/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/system.h>

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <engine/storage.h>

#include <engine/shared/assertion_logger.h>

CHostLookup::CHostLookup() = default;

CHostLookup::CHostLookup(const char *pHostname, int Nettype)
{
	str_copy(m_aHostname, pHostname, sizeof(m_aHostname));
	m_Nettype = Nettype;
}

void CHostLookup::Run()
{
	m_Result = net_host_lookup(m_aHostname, &m_Addr, m_Nettype);
}

class CEngine : public IEngine
{
public:
	IConsole *m_pConsole;
	IStorage *m_pStorage;
	bool m_Logging;

	CAssertionLogger m_AssertionLogger;

	char m_aAppName[256];

	static void Con_DbgLognetwork(IConsole::IResult *pResult, void *pUserData)
	{
		CEngine *pEngine = static_cast<CEngine *>(pUserData);

		if(pEngine->m_Logging)
		{
			CNetBase::CloseLog();
			pEngine->m_Logging = false;
		}
		else
		{
			char aBuf[32];
			str_timestamp(aBuf, sizeof(aBuf));
			char aFilenameSent[IO_MAX_PATH_LENGTH], aFilenameRecv[IO_MAX_PATH_LENGTH];
			str_format(aFilenameSent, sizeof(aFilenameSent), "dumps/network_sent_%s.txt", aBuf);
			str_format(aFilenameRecv, sizeof(aFilenameRecv), "dumps/network_recv_%s.txt", aBuf);
			CNetBase::OpenLog(pEngine->m_pStorage->OpenFile(aFilenameSent, IOFLAG_WRITE, IStorage::TYPE_SAVE),
				pEngine->m_pStorage->OpenFile(aFilenameRecv, IOFLAG_WRITE, IStorage::TYPE_SAVE));
			pEngine->m_Logging = true;
		}
	}

	CEngine(bool Test, const char *pAppname, bool Silent, int Jobs)
	{
		str_copy(m_aAppName, pAppname, std::size(m_aAppName));
		if(!Test)
		{
			if(!Silent)
				dbg_logger_stdout();
			dbg_logger_debugger();

			//
			dbg_msg("engine", "running on %s-%s-%s", CONF_FAMILY_STRING, CONF_PLATFORM_STRING, CONF_ARCH_STRING);
#ifdef CONF_ARCH_ENDIAN_LITTLE
			dbg_msg("engine", "arch is little endian");
#elif defined(CONF_ARCH_ENDIAN_BIG)
			dbg_msg("engine", "arch is big endian");
#else
			dbg_msg("engine", "unknown endian");
#endif

			// init the network
			net_init();
			CNetBase::Init();
		}

		m_JobPool.Init(Jobs);

		m_Logging = false;
	}

	void Init()
	{
		m_pConsole = Kernel()->RequestInterface<IConsole>();
		m_pStorage = Kernel()->RequestInterface<IStorage>();

		if(!m_pConsole || !m_pStorage)
			return;

		char aFullPath[IO_MAX_PATH_LENGTH];
		m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, "dumps/", aFullPath, sizeof(aFullPath));
		m_AssertionLogger.Init(aFullPath, m_aAppName);

		m_pConsole->Register("dbg_lognetwork", "", CFGFLAG_SERVER | CFGFLAG_CLIENT, Con_DbgLognetwork, this, "Log the network");
	}

	void InitLogfile()
	{
		// open logfile if needed
		if(g_Config.m_Logfile[0])
			dbg_logger_file(g_Config.m_Logfile);
	}

	void AddJob(std::shared_ptr<IJob> pJob)
	{
		if(g_Config.m_Debug)
			dbg_msg("engine", "job added");
		m_JobPool.Add(std::move(pJob));
	}
};

void IEngine::RunJobBlocking(IJob *pJob)
{
	CJobPool::RunBlocking(pJob);
}

IEngine *CreateEngine(const char *pAppname, bool Silent, int Jobs) { return new CEngine(false, pAppname, Silent, Jobs); }
IEngine *CreateTestEngine(const char *pAppname, int Jobs) { return new CEngine(true, pAppname, true, Jobs); }
