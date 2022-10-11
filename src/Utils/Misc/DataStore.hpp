#pragma once


class CDataStore final
{
public:
	void Clear(void);
	void Remove(const char* key);
	void Set(const char* key, const char* value);
	void Set(const char* key, const std::string& str);
	std::string Get(const char* key) const;
	int GetCount(const char* key) const;
	std::vector<std::string> GetVector(const char* key) const;

private:
	std::unordered_multimap<std::string, std::string> m_mapData;
};

