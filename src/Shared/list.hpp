#pragma once


class CListDefaultTag {};


template<class T, class tag = CListDefaultTag>
class CListNode
{
public:
    CListNode(const CListNode&) = delete;
    const CListNode& operator=(const CListNode&) = delete;

    inline CListNode() : next(nullptr), prev(nullptr), data((T*)this) {};
    inline bool is_linked() const { return (next && prev); };

    inline void unlink()
    {
        prev->next = next;
        next->prev = prev;
        next = prev = nullptr;
    };

    CListNode<T, tag>* next;
    CListNode<T, tag>* prev;
    T* data;
};


template<class T, class tag = CListDefaultTag>
class CList : public CListNode<T, tag>
{
public:
    typedef T value_type;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T* pointer;
    typedef const T* const_pointer;


    template<class Ty>
    class iterator_base
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = T;
        using pointer = T*;
        using reference = T&;

        inline iterator_base() : m_list(nullptr), m_node(nullptr) {};
        inline iterator_base(Ty* list, Ty* node) : m_list(list), m_node(node) {};
        inline iterator_base(const iterator_base& it) : m_list(it.m_list), m_node(it.m_node) {};

        inline bool is_end()
        {
            return (m_node == m_list);
        };

        inline Ty* node()
        {
            return m_node;
        };

        inline iterator_base prev()
        {
            return iterator_base(m_list, m_node->prev);
        };

        inline iterator_base next()
        {
            return iterator_base(m_list, m_node->next);
        };

        inline bool operator==(const iterator_base& it) const
        {
            return ((m_list == it.m_list) && (m_node == it.m_node));
        };

        inline bool operator!=(const iterator_base& it) const
        {
            return ((m_list == it.m_list) && (m_node != it.m_node));
        };

        inline iterator_base operator++(int)
        {
            auto self = *this;
            *this = next();
            return self;
        };

        inline iterator_base operator--(int)
        {
            auto self = *this;
            *this = prev();
            return self;
        };

        inline iterator_base& operator++()
        {
            *this = next();
            return *this;
        };

        inline iterator_base& operator--()
        {
            *this = prev();
            return *this;
        };

        inline explicit operator bool()
        {
            return (!is_end());
        };

        inline reference operator*() const
        {
            return *m_node->data;
        };

        inline pointer operator->() const
        {
            return m_node->data;
        };

    protected:
        Ty* m_node;
        Ty* m_list;
    };

    typedef iterator_base<CListNode<T, tag>> iterator;
    typedef iterator_base<const CListNode<T, tag>> const_iterator;

public:
    inline CList()
    {
        clear();
    };

    inline CList(const CList<T, tag>& other)
    {
        *this = other;
    };

    inline const CList<T, tag>& operator=(const CList<T, tag>& other)
    {
        this->next = other.next;
        this->prev = other.prev;
        this->data = other.data;
        return *this;
    };

    inline void clear()
    {
        this->data = nullptr;
        this->next = this->prev = this;
    };

    inline bool empty() const
    {
        return (this->prev == this);
    };

    inline pointer front()
    {
        if (empty())
            return nullptr;
        else
            return static_cast<T*>(this->next);
    };

    inline pointer front() const
    {
        if (empty())
            return nullptr;
        else
            return static_cast<T*>(this->next);
    };

    inline pointer back()
    {
        if (empty())
            return nullptr;
        else
            return static_cast<T*>(this->prev);
    };

    inline pointer back() const
    {
        if (empty())
            return nullptr;
        else
            return static_cast<T*>(this->prev);
    };

    inline iterator begin()
    {
        return iterator(this, this->next);
    };

    inline iterator end()
    {
        return iterator(this, this);
    };

    inline const_iterator begin() const
    {
        return const_iterator(this, this->next);
    };

    inline const_iterator end() const
    {
        return const_iterator(this, this);
    };

    inline void push_front(iterator it)
    {
        push_front(it.node());
    };

    inline void push_front(CListNode<T, tag>* node)
    {
        insert(iterator(this, this->next), node);
    };

    inline void push_back(iterator it)
    {
        push_back(it.node());
    };

    inline void push_back(CListNode<T, tag>* node)
    {
        insert(iterator(this, this->prev->next), node);
    };

    inline iterator insert(iterator pos, CListNode<T, tag>* node)
    {
        CListNode<T, tag>* pos_node = pos.node();
        iterator result(this, node);

        node->prev = pos_node->prev;
        node->next = pos_node;

        pos_node->prev->next = node;
        pos_node->prev = node;

        return result;
    };

    inline void pop_front()
    {
        erase(this->next);
    };

    inline void pop_back()
    {
        erase(this->prev);
    };

    inline iterator erase(CListNode<T, tag>* node)
    {
        return erase(iterator(this, node));
    };

    inline iterator erase(iterator it)
    {
        if (empty() || !it)
            return it;

        CListNode<T, tag>* node = it.node();
        iterator result = ++it;

        node->prev->next = node->next;
        node->next->prev = node->prev;

        node->prev = node->next = nullptr;

        return result;
    };

    inline void merge(CList<T, tag>* list)
    {
        if (list->empty())
            return;

        this->prev->next = list->next;
        list->next->prev = this->prev;

        list->prev->next = this;
        this->prev = list->prev;

        list->clear();
    };

    inline void swap(CList<T, tag>* list)
    {
        if (empty() || list->empty())
            return;

        this->next->prev = list;
        this->prev->next = list;

        list->next->prev = this;
        list->prev->next = this;

        CList<T, tag> temp = *list;
        *list = *this;
        *this = temp;
    };

    inline bool contains(const CListNode<T, tag>* node) const
    {
        return std::any_of(begin(), end(), [ & ](CListNode<T, tag>& n) { return (node == &n); });
    };

    inline pointer search(std::size_t no)
    {
        ASSERT(no >= 0 && no < std::distance(begin(), end()));

        iterator it = begin();
        std::advance(it, no);
        return &(*it);
    };
};