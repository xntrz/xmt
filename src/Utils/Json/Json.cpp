#include "Json.hpp"

#include "cjson/cJSON.h"


static const char* JSON_TRUE	= "true";
static const char* JSON_FALSE	= "false";
static const char* JSON_NULL	= "null";


static void* cjson_malloc(uint32 size)
{
#ifdef _DEBUG	
	return MemAlloc(size, __FILE__, __LINE__);
#else
	return MemAlloc(size);
#endif
};


static void cjson_free(void* mem)
{
#ifdef _DEBUG	
	MemFree(mem, __FILE__, __LINE__);
#else
	MemFree(mem);
#endif
};


/*static*/ void CJson::Initialize(void)
{
	cJSON_Hooks CJsonMemFunc = {};
	CJsonMemFunc.malloc_fn = cjson_malloc;
	CJsonMemFunc.free_fn = cjson_free;

	cJSON_InitHooks(&CJsonMemFunc);
};


/*static*/ void CJson::Terminate(void)
{
	;
};


CJson::CJson(void* opaque)
: m_opaque(opaque)
, m_bParent(false)
{
	;
};


CJson::CJson(const char* buffer, int bufferSize)
: m_opaque(nullptr)
, m_bParent(true)
{
	if(!buffer)
		throw exception("invalid buffer");

	if (!bufferSize)
		throw exception("empty buffer");

	m_opaque = cJSON_ParseWithLength(buffer, bufferSize);
	if (!m_opaque)
		throw exception("parse failed");
};


CJson::~CJson(void)
{
	if (m_opaque && m_bParent)
		cJSON_Delete((cJSON*)m_opaque);
};


CJson CJson::operator[](const char* name)
{
	cJSON* childObj = cJSON_GetObjectItem((cJSON*)m_opaque, name);
	if (!childObj)
		throw exception(name);
	return CJson(childObj);
};


CJson CJson::operator[](int idx)
{
	ASSERT(is_array());	
	cJSON* childObj = cJSON_GetArrayItem((cJSON*)m_opaque, idx);
	if (!childObj)
		throw exception("invalid array index");
	return CJson(childObj);
};


std::string CJson::data(void)
{
	cJSON* obj = (cJSON*)m_opaque;
	if (cJSON_IsString(obj))
	{
		return cJSON_GetStringValue(obj);
	}
	else if (cJSON_IsNumber(obj))
	{
		return std::to_string(obj->valueint);
	}
	else if (cJSON_IsBool(obj))
	{
		return cJSON_IsTrue(obj) ? JSON_TRUE : JSON_FALSE;
	}
	else if (cJSON_IsNull(obj))
	{
		return JSON_NULL;
	}
	else
	{
		throw exception("unknown json data type");
	};
};


bool CJson::is_object(void) const
{
	return (cJSON_IsObject((cJSON*)m_opaque) > 0);
};


bool CJson::is_array(void) const
{
	return (cJSON_IsArray((cJSON*)m_opaque) > 0);
};


int CJson::array_size(void) const
{
	return cJSON_GetArraySize((cJSON*)m_opaque);
};