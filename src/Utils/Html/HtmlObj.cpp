#include "HtmlObj.hpp"

#include "gumbo/gumbo.h"


#ifdef _DEBUG
#define HTMLOBJ_EXCEPTION(Msg) CHtmlObj::exception(__FUNCTION__, Msg)
#else
#define HTMLOBJ_EXCEPTION(Msg) CHtmlObj::exception("", Msg)
#endif


static void* HtmlAlloc(void* Param, uint32 Size)
{
#ifdef _DEBUG	
    return MemAlloc(Size, __FILE__, __LINE__);
#else
    return MemAlloc(Size);
#endif
};


static void HtmlFree(void* Param, void* Ptr)
{
#ifdef _DEBUG	
    MemFree(Ptr, __FILE__, __LINE__);
#else
    MemFree(Ptr);
#endif
};


const GumboOptions MyGumboOptions =
{
    /*allocator             */  &HtmlAlloc,
    /*deallocator           */  &HtmlFree,
    /*userdata              */  NULL,       // alloc/free param
    /*tab_stop              */  8,
    /*stop_on_first_error   */  false,
    /*max_errors            */  -1,
    /*fragment_context      */  GUMBO_TAG_LAST,
    /*fragment_namespace    */  GUMBO_NAMESPACE_HTML
};


static inline GumboOutput* ConvOutput(void* pOutput)
{
    ASSERT(pOutput);
    if (!pOutput)
        throw HTMLOBJ_EXCEPTION("error output is NULL");
    
    return (GumboOutput*)pOutput;
};


static inline GumboNode* ConvNode(void* pNode)
{
    ASSERT(pNode);
    if (!pNode)
        throw HTMLOBJ_EXCEPTION("error node is NULL");
    
    return (GumboNode*)pNode;
};


/*static*/ void CHtmlObj::Initialize(void)
{
    ;
};


/*static*/ void CHtmlObj::Terminate(void)
{
    ;
};


CHtmlObj::exception::exception(const std::string& Func, const std::string& Msg)
: m_Msg(Func + "-->" + Msg)
{
    ;
};


CHtmlObj::exception::~exception(void)
{
    ;
};


char const* CHtmlObj::exception::what(void) const
{
    return m_Msg.c_str();
};


CHtmlObj::CHtmlObj(void* pNode)
: m_pOutput(nullptr)
, m_pNode(pNode)
{
    if (!pNode)
        throw HTMLOBJ_EXCEPTION("construct failed - node is NULL");
};


CHtmlObj::CHtmlObj(const char* DocBuffer, uint32 DocBufferSize)
{
    if (!DocBuffer)
        throw HTMLOBJ_EXCEPTION("construct failed - doc buffer invalid");

    if (DocBufferSize == 0)
        throw HTMLOBJ_EXCEPTION("construct failed - doc buffer empty");

    m_pOutput = gumbo_parse_with_options(&MyGumboOptions, DocBuffer, DocBufferSize);
    if (m_pOutput)
        m_pNode = ConvOutput(m_pOutput)->root;
    else
        throw HTMLOBJ_EXCEPTION("construct failed - output is NULL");
};


CHtmlObj::~CHtmlObj(void)
{
    if (m_pOutput)
    {        
        gumbo_destroy_output(&MyGumboOptions, ConvOutput(m_pOutput));
        m_pOutput = nullptr;
        m_pNode = nullptr;
    };
};


CHtmlObj CHtmlObj::parent(void)
{    
    GumboNode* pNode = ConvNode(m_pNode);
    if (!pNode->parent)
        throw HTMLOBJ_EXCEPTION("parent is null!");

    return CHtmlObj(pNode->parent);
};


CHtmlObj CHtmlObj::operator[](const std::string& Label)
{
    GumboNode* pNode = ConvNode(m_pNode);
    GumboTag TagId = gumbo_tag_enum(Label.c_str());

    const GumboVector* pRootChildren = &pNode->v.element.children;
    for (int32 i = 0; i < int32(pRootChildren->length); ++i)
    {
        GumboNode* pChild = (GumboNode*)pRootChildren->data[i];
        if (pChild->type != GUMBO_NODE_ELEMENT)
            continue;

        if (TagId != GUMBO_TAG_UNKNOWN)
        {
            if (pChild->v.element.tag == TagId)
                return CHtmlObj(pChild);
        }
        else
        {
            std::string ElementTag(
                pChild->v.element.original_tag.data,
                pChild->v.element.original_tag.data + pChild->v.element.original_tag.length
            );

            std::size_t Pos = ElementTag.find(Label);
            if ((Pos != std::string::npos) && (Pos == 1))
                return CHtmlObj(pChild);
        };
    };

    throw HTMLOBJ_EXCEPTION("not found " + Label);

    return CHtmlObj(nullptr);
};


CHtmlObj CHtmlObj::operator[](int32 Index)
{
    GumboNode* pNode = ConvNode(m_pNode);

    if (pNode->type != GUMBO_NODE_ELEMENT)
        throw HTMLOBJ_EXCEPTION("not a element node at index " + std::to_string(Index));

    if ((Index < 0) || (Index >= int32(pNode->v.element.children.length)))
        throw HTMLOBJ_EXCEPTION("index \"" + std::to_string(Index) + "\" is out of bounds");

    return CHtmlObj(pNode->v.element.children.data[Index]);
};


int32 CHtmlObj::count(void)
{
    GumboNode* pNode = ConvNode(m_pNode);

    if (pNode->type != GUMBO_NODE_ELEMENT)
        throw HTMLOBJ_EXCEPTION("not a element node");

	return int32(pNode->v.element.children.length);
};


std::string CHtmlObj::text(void)
{
    GumboNode* pNode = ConvNode(m_pNode);

    if (pNode->type != GUMBO_NODE_TEXT)
        throw HTMLOBJ_EXCEPTION("not a text node");

    return std::string(pNode->v.text.text);
};


bool CHtmlObj::is_text(void)
{
    GumboNode* pNode = ConvNode(m_pNode);
    
    return (pNode->type == GUMBO_NODE_TEXT);
};


bool CHtmlObj::has_attribute(const std::string& AttributeName)
{
    bool bResult = false;

    try
    {
        attribute(AttributeName);
        bResult = true;
    }
    catch (exception& e)
    {
		REF(e);
    };

    return bResult;
};


bool CHtmlObj::has_attribute_value(const std::string& AttributeName, const std::string& AttributeValue)
{
    bool bResult = false;

    try
    {
        std::string AttrVal = attribute(AttributeName);
        if (AttrVal.compare(AttributeValue) == 0)
            bResult = true;
    }
    catch (exception& e)
    {
		REF(e);
    };

    return bResult;
};


bool CHtmlObj::has_attributes_values(const std::vector<find_attribute>& attr_and_values)
{
    int32 NumOk = 0;

    for (int32 i = 0; i < int32(attr_and_values.size()); ++i)
    {
        if (has_attribute_value(attr_and_values[i].name, attr_and_values[i].value))
            ++NumOk;
    };

    return (NumOk == attr_and_values.size());
};


int32 CHtmlObj::attribute_count(void)
{
    GumboNode* pNode = ConvNode(m_pNode);

    if (pNode->type != GUMBO_NODE_ELEMENT)
        throw HTMLOBJ_EXCEPTION("not a element node");

    return pNode->v.element.attributes.length;
};


std::string CHtmlObj::attribute(int32 Index)
{
    GumboNode* pNode = ConvNode(m_pNode);

    if (pNode->type != GUMBO_NODE_ELEMENT)
        throw HTMLOBJ_EXCEPTION("not a element node at index " + std::to_string(Index));

    if ((Index < 0) || (Index >= int32(pNode->v.element.attributes.length)))
        throw HTMLOBJ_EXCEPTION("index \"" + std::to_string(Index) + "\" is out of bounds");

    GumboAttribute* pAttribute = (GumboAttribute*)pNode->v.element.attributes.data[Index];
    ASSERT(pAttribute);

    return std::string(pAttribute->value);
};


std::string CHtmlObj::attribute(const std::string& AttributeName)
{
    GumboNode* pNode = ConvNode(m_pNode);

    if (pNode->type != GUMBO_NODE_ELEMENT)
        throw HTMLOBJ_EXCEPTION("not a element node " + AttributeName);
    
    GumboAttribute* pAttribute = gumbo_get_attribute(&pNode->v.element.attributes, AttributeName.c_str());
    if (!pAttribute)
        throw HTMLOBJ_EXCEPTION("attribute not found " + AttributeName);

    return std::string(pAttribute->value);
};


CHtmlObj CHtmlObj::find(const std::string& Label, const std::vector<find_attribute>& attributes)
{
    GumboNode* pNode = ConvNode(m_pNode);

    //
    //  Recursive walk through doc tree and search tag
    //
    const auto FnFindNode = [](const std::string& Label, const std::vector<find_attribute>& attributes, GumboNode* pNode, auto& Fn) -> GumboNode*
    {
        GumboTag TagId = gumbo_tag_enum(Label.c_str());
        const GumboVector* pRootChildren = &pNode->v.element.children;
        
        for (int32 i = 0; i < int32(pRootChildren->length); ++i)
        {
            GumboNode* pChild = (GumboNode*)pRootChildren->data[i];
            if (pChild->type != GUMBO_NODE_ELEMENT)
                continue;
            
            if (TagId != GUMBO_TAG_UNKNOWN)
            {
                //
                //  If tag valid, check it and check attributes for this node
                //
                if (pChild->v.element.tag == TagId)
                {
                    CHtmlObj obj(pChild);
                    if (obj.has_attributes_values(attributes))
                        return pChild;
                };
            }
            else
            {
                //
                //  Tag is invalid, check by tag name at pos 1
                //
                std::string ElementTag(
                    pChild->v.element.original_tag.data,
                    pChild->v.element.original_tag.data + pChild->v.element.original_tag.length
                );

                std::size_t Pos = ElementTag.find(Label);
                if ((Pos != std::string::npos) && (Pos == 1))
                {
                    CHtmlObj obj(pChild);
                    if (obj.has_attributes_values(attributes))
                        return pChild;
                };
            };

            GumboNode* pRet = Fn(Label, attributes, pChild, Fn);
            if (pRet)
                return pRet;
        };

        return nullptr;
    };

    pNode = FnFindNode(Label, attributes, pNode, FnFindNode);
    if (!pNode)
        throw HTMLOBJ_EXCEPTION("not found " + Label);
	
	return CHtmlObj(pNode);
};


std::string CHtmlObj::tag(void)
{
    GumboNode* pNode = ConvNode(m_pNode);

    switch (pNode->type)
    {
    case GUMBO_NODE_TEXT:
        return std::string(pNode->v.text.text);

    case GUMBO_NODE_ELEMENT:
        return std::string(
            pNode->v.element.original_tag.data ? pNode->v.element.original_tag.data : "unknown tag"
        );

    default:
        return "unknown tag";
    };
};