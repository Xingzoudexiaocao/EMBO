/*
 * CTU/EMBO - EMBedded Oscilloscope <github.com/parezj/EMBO>
 * Author: Jakub Parez <parez.jakub@gmail.com>
 */

#include <vector>

#ifndef MOVEMEAN_H
#define MOVEMEAN_H

template <class T>
class MoveMean
{
public:
    MoveMean() { }
    bool setSize(int size);
    void addVal(T const& val);
    double getMean();
    double getMean(T const& val);
    void reset();

private:
    std::vector<T> m_buff;
    int m_it = 0;
    int m_cnt = 0;
    int m_sz = 1;
};

template <class T>
bool MoveMean<T>::setSize(int size)
{
    if (size < 1)
        return false;

   m_buff.resize(size);
   m_sz = size;

   return true;
}

template <class T>
void MoveMean<T>::addVal(T const& val)
{
   m_buff[m_it++] = val;

   if (m_it >= m_sz)
       m_it = 0;

   if (m_cnt < m_sz)
       m_cnt++;
}

template <class T>
double MoveMean<T>::getMean()
{
    double ret = 0;
    for (int i = 0; i < m_cnt; i++)
        ret += m_buff[i];

    return ret / m_cnt;
}

template <class T>
double MoveMean<T>::getMean(T const& val)
{
    addVal(val);
    return getMean();
}

template <class T>
void MoveMean<T>::reset()
{
    m_it = 0;
    m_cnt = 0;
    m_buff.clear();
}

#endif // MOVEMEAN_H
