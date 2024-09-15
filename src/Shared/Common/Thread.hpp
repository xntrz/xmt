#pragma once


namespace thread
{
    void initialize();
    void terminate();
    DLLSHARED void regist_current(const char* pszName);
    DLLSHARED void remove_current();
    DLLSHARED const char* current_name();
    DLLSHARED uint32 current_id();
    DLLSHARED uint32 current_app_id();
    

    template<class Function, class ...Args>
    inline void __main(const std::string& name, Function&& f, Args&&... args)
    {
        regist_current(name.c_str());
        f(std::forward<Args>(args) ...);
        remove_current();
    };


    template<class Function, class... Args>
    inline std::thread spawn(const std::string& name, Function&& f, Args&&... args)
    {
        return std::thread(&__main<Function&&, Args&&...>, name, std::forward<Function>(f), std::forward<Args>(args)...);
    };
};