#include "AppModule.hpp"

#include "Shared/Common/Configuration.hpp"


inline std::string extractModuleName(const std::string& path)
{
	std::size_t posBegin = path.find_last_of('\\');
	if (posBegin == std::string::npos)
		posBegin = 0;

	std::size_t posEnd = path.find_first_of('.', ++posBegin);
	if (posEnd == std::string::npos)
		posEnd = path.length();

	return path.substr(posBegin, posEnd - posBegin);
};


class CAppModuleContainer
{
public:
	class CModule
	{
	public:
		CModule(const std::string& path);
		bool startup();
		void shutdown();
		void attach();
		void detach();
		void info(HINSTANCE& hInstance, std::string& strTag);
		void info_ex(HINSTANCE& hInstance, std::string& strTitle, std::string& strDescription, int32& rcIcoId, bool(*&ui_proc)(struct nkgdi_window*, struct nk_context*, bool));
		
	private:
		std::string m_path;
		HMODULE m_handle;
		MODULE_EVT_PROC m_pfnEvtProc;
	};

	static const std::size_t id_termination_request = std::numeric_limits<std::size_t>::max();

public:
	CAppModuleContainer();
	~CAppModuleContainer();
	void SetCurrentModule(std::size_t id);
	void LoadModules(const std::vector<std::string>& pathModules);
	void UnloadModules();
	std::vector<std::string> PreLoadModules(const std::string& pathRoot) const;
	std::vector<std::string> FindModules(const std::string& pathRoot) const;
	bool ValidateModule(const std::string& pathModule) const;
	bool IsPathDLL(const std::string& pathModule) const;
	
	inline std::shared_ptr<CModule> GetModule(std::size_t id) const { return m_moduleArray[id]; };
	inline std::size_t GetModuleCnt() const { return m_moduleArray.size(); };
	
private:
	std::shared_ptr<CModule> m_pCurrentModule;
	std::vector<std::shared_ptr<CModule>> m_moduleArray;
	bool m_bRecursivePreload;
};


CAppModuleContainer::CModule::CModule(const std::string& path)
: m_path(path)
, m_handle(NULL)
, m_pfnEvtProc(nullptr)
{
	;
};


bool CAppModuleContainer::CModule::startup()
{
	m_handle = LoadLibraryA(m_path.c_str());
	ASSERT(m_handle != NULL);
	
	if (m_handle)
	{
		m_pfnEvtProc = MODULE_EVT_PROC(GetProcAddress(m_handle, MODULE_ENTRY_NAME));

#ifdef _DEBUG
		std::string tag = extractModuleName(m_path);
		DbgRegistModule(tag.c_str(), m_handle);
#endif  

		MODULE_EVT evt = { 0 };
		evt.id = MODULE_EVT_STARTUP;
		evt.param.startup.hInstance = m_handle;
		m_pfnEvtProc(evt);
	};

	return (m_handle != NULL);
};


void CAppModuleContainer::CModule::shutdown()
{
	MODULE_EVT evt = { 0 };
	evt.id = MODULE_EVT_SHUTDOWN;
	evt.param.shutdown.hInstance = m_handle;
	m_pfnEvtProc(evt);

#ifdef _DEBUG
	DbgRemoveModule(m_handle);
#endif     

	FreeLibrary(m_handle);
	m_handle = NULL;
};


void CAppModuleContainer::CModule::attach()
{
	MODULE_EVT evt = { 0 };
	evt.id = MODULE_EVT_ATTACH;
	m_pfnEvtProc(evt);
};


void CAppModuleContainer::CModule::detach()
{
	MODULE_EVT evt = { 0 };
	evt.id = MODULE_EVT_DETACH;
	m_pfnEvtProc(evt);
};


void CAppModuleContainer::CModule::info(HINSTANCE& hInstance, std::string& strTag)
{
	MODULE_EVT evt = { 0 };
	evt.id = MODULE_EVT_INFO;
	m_pfnEvtProc(evt);

	hInstance 	= m_handle;
	strTag 		= (evt.param.info.tag ? evt.param.info.tag : "MODULE_TAG_PLACEHOLDER");
};


void CAppModuleContainer::CModule::info_ex(
	HINSTANCE& 		hInstance,
	std::string& 	strTitle,
	std::string& 	strDescription,
	int32& 			rcIcoId,
	bool(*&ui_proc)(struct nkgdi_window*, struct nk_context*, bool)
)
{
	MODULE_EVT evt = { 0 };
	evt.id = MODULE_EVT_INFO;
	m_pfnEvtProc(evt);

	hInstance 		= m_handle;
	strTitle 		= (evt.param.info.title ? evt.param.info.title : "MODULE_TITLE_PLACEHOLDER");
	strDescription 	= (evt.param.info.description ? evt.param.info.description : "MODULE_DESC_PLACEHOLDER");
	rcIcoId 		= evt.param.info.icoid;
	ui_proc 		= evt.param.info.ui_proc;
};


CAppModuleContainer::CAppModuleContainer()
: m_pCurrentModule(nullptr)
, m_moduleArray()
, m_bRecursivePreload(false)
{
	std::vector<std::string> modulePaths = PreLoadModules(CfgGetCurrentDir());
	LoadModules(modulePaths);
};


CAppModuleContainer::~CAppModuleContainer()
{
	UnloadModules();
};


void CAppModuleContainer::SetCurrentModule(std::size_t id)
{
	if (id == id_termination_request)
	{
		if (m_pCurrentModule)
			m_pCurrentModule->detach();
		m_pCurrentModule = nullptr;
	}
	else
	{
		if (m_pCurrentModule == m_moduleArray[id])
			return;

		if (m_pCurrentModule)
			m_pCurrentModule->detach();
		
		m_pCurrentModule = m_moduleArray[id];
		ASSERT(m_pCurrentModule);
		
		m_pCurrentModule->attach();
	};
};


void CAppModuleContainer::LoadModules(const std::vector<std::string>& pathModules)
{
	std::for_each(pathModules.begin(), pathModules.end(), [this](const std::string& path) {
		std::shared_ptr<CModule> pModule = std::make_shared<CModule>(path);
		if (pModule->startup())
			m_moduleArray.push_back(pModule);
	});
};


void CAppModuleContainer::UnloadModules()
{
	std::for_each(m_moduleArray.begin(), m_moduleArray.end(), std::mem_fn(&CModule::shutdown));
};


std::vector<std::string> CAppModuleContainer::PreLoadModules(const std::string& pathRoot) const
{
	std::vector<std::string> paths;
	std::vector<std::string> modPaths = FindModules(pathRoot);

	std::for_each(modPaths.begin(), modPaths.end(), [&](const std::string& modPath) {
		if (ValidateModule(modPath))
			paths.push_back(std::move(modPath));
	});

	return paths;
};


std::vector<std::string> CAppModuleContainer::FindModules(const std::string& pathRoot) const
{
	std::vector<std::string> result;
	result.reserve(MODULE_MAX);

	std::string fdPath(pathRoot + "\\*");
	WIN32_FIND_DATAA w32fd;
	std::memset(&w32fd, 0, sizeof(w32fd));

	HANDLE hFind = FindFirstFileA(fdPath.c_str(), &w32fd);
	if (hFind)
	{
		do
		{
			if (w32fd.cFileName[0] == '.')
				continue;

			std::string filePath = pathRoot + '\\' + w32fd.cFileName;
			if (w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (m_bRecursivePreload)
				{
					std::vector<std::string> sub_result = FindModules(filePath);
					result.insert(result.end(), sub_result.begin(), sub_result.end());
				};
			}
			else
			{
				if (IsPathDLL(filePath))
					result.push_back(std::move(filePath));
			};
		} while (FindNextFileA(hFind, &w32fd));

		FindClose(hFind);
		hFind = NULL;
	};

	return result;
};


bool CAppModuleContainer::ValidateModule(const std::string& pathModule) const
{
	bool bValid = false;
	
	HMODULE handle = LoadLibraryExA(pathModule.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
	if (handle != NULL)
	{
		bValid = GetProcAddress(handle, MODULE_ENTRY_NAME) ? true : false;
		FreeLibrary(handle);
	};

#ifdef _DEBUG
	if(!bValid)
		OUTPUTLN("Attempt to load invalid module \"%s\"", pathModule.c_str());
#endif	

	return bValid;
};


bool CAppModuleContainer::IsPathDLL(const std::string& pathModule) const
{
	std::size_t fmtBegin = pathModule.find_last_of('.');
	if (fmtBegin == std::string::npos)
		return false;

	std::string fmt = pathModule.substr(++fmtBegin);
	if (fmt.empty())
		return false;

	std::transform(fmt.begin(), fmt.end(), fmt.begin(), std::tolower);
	if (fmt != "dll")
		return false;

	return true;
};


static std::shared_ptr<CAppModuleContainer> s_pAppModContainer= nullptr;


static inline std::shared_ptr<CAppModuleContainer> AppModContainer()
{
	return s_pAppModContainer;
};


void AppModInitialize()
{
	s_pAppModContainer = std::make_shared<CAppModuleContainer>();
};


void AppModTerminate()
{
	s_pAppModContainer = nullptr;
};


void AppModSetCurrent(std::size_t id)
{
	AppModContainer()->SetCurrentModule(id);
};


std::size_t AppModGetCount()
{
	return AppModContainer()->GetModuleCnt();
};


bool
AppModGetInfo(
    std::size_t     id,
    HINSTANCE&      hInstamce,
    int32&          rcIcoId,
    std::string&    strTitle,
    std::string&    strDescription,
    bool            (*&ui_proc)(struct nkgdi_window*, struct nk_context*, bool)
)
{
	ASSERT(id < AppModContainer()->GetModuleCnt());
	
	if (id < AppModContainer()->GetModuleCnt())
		AppModContainer()->GetModule(id)->info_ex(hInstamce, strTitle, strDescription, rcIcoId, ui_proc);

	return (hInstamce != NULL);
};