#pragma once


class CJson final
{
public:
	class exception final : public std::exception
	{
	public:
		exception(const char* msg) : std::exception(msg) {};
		virtual ~exception(void) {};
	};

public:
	static void Initialize(void);
	static void Terminate(void);

private:	
	CJson(void* opaque);

public:
	CJson(const char* buffer, int bufferSize);
	~CJson(void);
	CJson operator[](const char* name);
	CJson operator[](int idx);
	std::string data(void);
	bool is_object(void) const;
	bool is_array(void) const;
	int array_size(void) const;

private:
	void* m_opaque;
	bool m_bParent;
};