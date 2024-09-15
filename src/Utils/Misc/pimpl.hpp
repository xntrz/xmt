#pragma once


/* stolen from here https://www.youtube.com/watch?v=_AkF8SpUV3k 10:30 */
template<class T, std::size_t size, std::size_t align>
struct pimpl
{
private:    
	template<std::size_t real_size, std::size_t real_align>
    inline static void validate() {
        static_assert(size == real_size, "change size");
        static_assert(align == real_align, "change align");
	};

public:
    template<class ...args>
    inline pimpl(args&&... a) {
        new (ptr()) T(std::forward<args>(a)...);
    };

    inline ~pimpl() {
		validate<sizeof(T), alignof(T)>();
        ptr()->~T();
    };

    inline pimpl& operator=(pimpl&& r) {
        *ptr() = std::move(*r);
        return *this;
    };

    inline T* operator->() {
        return ptr();
    };
    
    inline const T* operator->() const {
        return ptr();
    };
    
    inline T& operator*() {
        return *ptr();
    };
    
    inline const T& operator*() const {
        return *ptr();
    };
    
    inline T* ptr() {
        return reinterpret_cast<T*>(&m_storage);
    };
    
    inline const T* ptr() const {
        return reinterpret_cast<const T*>(&m_storage);
    };

private:
    std::aligned_storage_t<size, align> m_storage;
};