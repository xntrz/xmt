#pragma once


class CDataStore final
{
public:
	inline CDataStore()
		: m_mapData() {};

	inline void clear(void) {
		m_mapData.clear();
	};

	inline void set(const std::string& name, const std::string& value) {
		m_mapData.insert(std::pair<std::string, std::string>(name, value));
	};

	inline void remove(const std::string& name) {
		m_mapData.erase(name);
	};

	inline std::size_t get_count(const std::string& name) const {
		auto itPair = m_mapData.equal_range(name);
		return std::distance(itPair.first, itPair.second);
	};

	inline std::string get(const std::string& name) const {
		auto it = m_mapData.find(name);
		return (it != m_mapData.end() ? it->second : std::string());
	};

	inline std::vector<std::string> get_all(const std::string& name) const {
		auto itPair = m_mapData.equal_range(name);
		if (std::distance(itPair.first, itPair.second)) {
			std::vector<std::string> values;
			std::for_each(itPair.first, itPair.second, [&values](std::pair<std::string, std::string> mapPair) {
				values.push_back(mapPair.second);
			});
			return values;
		};

		return{};
	};

private:
	std::unordered_multimap<std::string, std::string> m_mapData;
};

