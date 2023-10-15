#pragma once
#include <stdio.h>
#include <assert.h>


template <typename T>
class Singleton
{
protected:
    Singleton()
    {
        assert(!m_kInstance);
        long long offset = (long long)(T*)1 - (long long)(Singleton<T> *)(T*)1;
        m_kInstance = (T*)((long long)this + offset);
    }
    ~Singleton()
    {
        assert(m_kInstance);
        m_kInstance = 0;
    }

public:
    static T* instance()
    {
        if (m_kInstance == NULL)
            m_kInstance = new T;
        return m_kInstance;
    };
    static void destroyInstance()
    {
        if (m_kInstance) {
            delete m_kInstance;
            m_kInstance = NULL;
        }
    };

private:
    static T* m_kInstance;
};

template <typename T> T* Singleton<T>::m_kInstance = 0;