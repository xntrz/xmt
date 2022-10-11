#include "DataStore.hpp"


void CDataStore::Clear(void)
{
	m_mapData.clear();
};


void CDataStore::Set(const char* key, const char* value)
{
	m_mapData.insert(std::pair<std::string, std::string>(key, value));
};


void CDataStore::Set(const char* key, const std::string& str)
{
	m_mapData.insert(std::pair<std::string, std::string>(key, str));
};


void CDataStore::Remove(const char* key)
{
	m_mapData.erase(key);
};


int CDataStore::GetCount(const char* key) const
{
	auto itPair = m_mapData.equal_range(key);
	return std::distance(itPair.first, itPair.second);
};


std::string CDataStore::Get(const char* key) const
{
	auto it = m_mapData.find(key);
	if (it != m_mapData.end())
		return it->second;
	else
		return{};
};


std::vector<std::string> CDataStore::GetVector(const char* key) const
{
	auto itPair = m_mapData.equal_range(key);
	if (std::distance(itPair.first, itPair.second))
	{
		std::vector<std::string> values;
		std::for_each(itPair.first, itPair.second, [&values](std::pair<std::string, std::string> mapPair)
		{
			values.push_back(mapPair.second);
		});
		return values;
	};

	return{};
};
