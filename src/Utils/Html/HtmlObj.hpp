#pragma once


class CHtmlObj final
{
public:
    class exception final : public std::exception
    {
    public:
        exception(const std::string& Func, const std::string& Msg);
        virtual ~exception(void);
        virtual char const* what(void) const override;

    private:
        std::string m_Msg;
    };

    struct find_attribute
    {
        std::string name;
        std::string value;
    };

public:
    static void Initialize(void);
    static void Terminate(void);

private:
    CHtmlObj(void* pNode);

public:
    CHtmlObj(const char* DocBuffer, uint32 DocBufferSize);
    ~CHtmlObj(void);
    CHtmlObj parent(void);
    CHtmlObj operator[](const std::string& Label);
    CHtmlObj operator[](int Index);
    int32 count(void);
    std::string text(void);
    bool is_text(void);
    bool has_attribute(const std::string& AttributeName);
    bool has_attribute_value(const std::string& AttributeName, const std::string& AttributeValue);
    bool has_attributes_values(const std::vector<find_attribute>& attr_and_values);
    int32 attribute_count(void);
    std::string attribute(int32 Index);
    std::string attribute(const std::string& AttributeName);
    CHtmlObj find(const std::string& Label, const std::vector<find_attribute>& attributes = {});

private:
    std::string tag(void);

private:
    void* m_pOutput;
    void* m_pNode;
};