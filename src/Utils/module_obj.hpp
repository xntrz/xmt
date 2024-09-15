#pragma once


struct module_obj : public CListNode<module_obj>
{
#ifdef _DEBUG    
    /* avoid alloc */
    static const std::size_t TAG_MAX_LEN = 8;
    
    inline module_obj()
        : m_tag("unk") { ASSERT(m_tag.length() < TAG_MAX_LEN); };
    
    inline module_obj(const std::string& tag)
        : m_tag(tag) { ASSERT(m_tag.length() < TAG_MAX_LEN); };

    std::string m_tag;
#else
    inline module_obj() {};
    inline module_obj(const std::string& tag) {};
#endif    
};


void module_obj_init();
void module_obj_term();
void module_obj_wait_removes();
void module_obj_regist(module_obj* obj);
void module_obj_remove(module_obj* obj);