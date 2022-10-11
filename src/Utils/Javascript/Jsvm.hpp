#pragma once

class CJsvm final
{
public:
    class exception final : public std::exception
    {
    public:
        exception(const std::string& msg);
        virtual ~exception(void) {};
        virtual char const* what() const override;

    private:
        std::string m_MyMsg;
    };
    
public:
    static void Initialize(void);
    static void Terminate(void);

    CJsvm(void);
    ~CJsvm(void);
    std::string Eval(const std::string& Line);

private:
    void* m_opaque;
};