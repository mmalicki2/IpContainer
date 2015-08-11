/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Mateusz Malicki (malicki.mateusz@gmail.com)
 *   
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef CHUNKALLOCATOR_HPP
#define CHUNKALLOCATOR_HPP

#include <iostream>
#include <vector>
#include <algorithm>
#include <memory>
#include <cassert>

#include <arpa/inet.h>


const int MIN_CAPACITY = 8;

template<class T, class UnderlyingPointerType>
class ChunkAllocator;

template<class T, class I>
class ChunkBuff {
public:
    typedef T value_type;
    typedef I index_type;

    ChunkBuff() {
        capacity = MIN_CAPACITY; 
        size = 1;
        buf = (value_type*)malloc(sizeof(value_type) * capacity);
    }
    ~ChunkBuff() {
        free(buf);
        buf = 0;
        capacity = 0;
        size = 0;
    }

    value_type& operator[](const index_type& index) { return buf[index]; }
    index_type allocate() {
        if (capacity <= size) {
            capacity *= 2;
            buf = (value_type*)realloc(buf, sizeof(value_type) * capacity);
        }
        assert(!capacity <= size);
        ::new (&buf[size]) value_type();
        return size++;
    }

    void deallocate(index_type index) {
            if (index != size - 1) {
                buf[index] = buf[size - 1];
                //TODO: Check if T has UpdateChunk method (SFINAE)
                buf[index].UpdateChunk(index, size - 1);
            }
            size--;
            if (capacity / 3 >= MIN_CAPACITY && size < capacity / 3) {
                capacity /= 3;
                buf = (value_type*)realloc(buf, sizeof(value_type) * capacity);
            }
    }
    
    private:
        index_type capacity;
        index_type size;
        value_type* buf; 
};


template<class MemoryAllocator>
class ChunkPointer 
{
private:
    typedef ChunkPointer<MemoryAllocator> this_type;
    typedef typename MemoryAllocator::underlying_pointer_type underlying_pointer_type;

    underlying_pointer_type index;
    ChunkPointer(underlying_pointer_type x) :index(x) {}
public:
    typedef typename MemoryAllocator::difference_type  difference_type;
    typedef typename MemoryAllocator::value_type       value_type;
    typedef typename MemoryAllocator::pointer          pointer;
    typedef typename MemoryAllocator::reference        reference;
    typedef std::random_access_iterator_tag            iterator_category;
    
    ChunkPointer() : index(-1) {}
    ChunkPointer(const this_type& other) : index(other.index) {}

    this_type& operator+=(difference_type rhs) {index += rhs; return *this;}
    this_type& operator-=(difference_type rhs) {index -= rhs; return *this;}
    value_type& operator*() const {return MemoryAllocator::buf[index];}
    value_type* operator->() const {return &MemoryAllocator::buf[index];}
    value_type& operator[](difference_type rhs) const {return MemoryAllocator::buf[index + rhs];}

    this_type& operator++() {++index; return *this;}
    this_type& operator--() {--index; return *this;}
    this_type operator++(int) const {this_type tmp(*this); ++index; return tmp;}
    this_type operator--(int) const {this_type tmp(*this); --index; return tmp;}
    difference_type operator-(const this_type& rhs) const {return this_type(index - rhs.index);}
    this_type operator+(difference_type rhs) const {return this_type(index + rhs);}
    this_type operator-(difference_type rhs) const {return this_type(index - rhs);}

    bool operator==(const this_type& rhs) const {return index == rhs.index;}
    bool operator!=(const this_type& rhs) const {return index != rhs.index;}
    bool operator>(const this_type& rhs) const {return index > rhs.index;}
    bool operator<(const this_type& rhs) const {return index < rhs.index;}
    bool operator>=(const this_type& rhs) const {return index >= rhs.index;}
    bool operator<=(const this_type& rhs) const {return index <= rhs.index;}

    friend inline this_type operator+(difference_type lhs, const this_type& rhs) {return this_type(lhs + rhs.index);}
    friend inline this_type operator-(difference_type lhs, const this_type& rhs) {return this_type(lhs - rhs.index);}
    
    friend class ChunkAllocator<value_type, underlying_pointer_type>;
    friend class ChunkBuff<value_type, underlying_pointer_type>;
};

template<class T, class UnderlyingPointerType = uint32_t>
class ChunkAllocator
{
    public:
        typedef ChunkAllocator<T, UnderlyingPointerType>  this_type;
        typedef T                                         value_type;
        typedef UnderlyingPointerType                     underlying_pointer_type;
        typedef UnderlyingPointerType                     size_type;
        typedef UnderlyingPointerType                     difference_type;
        typedef ChunkPointer<this_type>                   pointer;
        typedef const ChunkPointer<this_type>             const_pointer;
        typedef T&                                        reference;
        typedef const T&                                  const_reference;
 
        ChunkAllocator() throw() {}
        ChunkAllocator(const this_type&) throw() {}
        template <class U, class P = UnderlyingPointerType>
        ChunkAllocator(const ChunkAllocator<U, P>&) throw() {}
        ~ChunkAllocator() throw() {}

        pointer address(reference x) const { return &x; }
        const_pointer address(const_reference x) const { return &x; }

        pointer allocate(size_type n,
                         std::allocator<void>::const_pointer hint = 0) {
            assert(n == 1);
            return buf.allocate();
        }
        void deallocate(pointer p, size_type n) {
            assert(n == 1);
            assert(p > 0);
            buf.deallocate(p.index);
        }
        size_type max_size() const throw() { return 1; }
 
        void construct(pointer p, const T& val) { ::new (&buf[p.index]) T(val); }
        void destroy(pointer p) { buf[p.index].~T(); }

        template <class U>
        struct rebind { typedef std::allocator<U> other; };

    private:
        typedef ChunkBuff<value_type, size_type> buff_type;
        static buff_type buf;

        friend class ChunkPointer<this_type>;
};

template<class T, class U>
typename ChunkAllocator<T, U>::buff_type ChunkAllocator<T, U>::buf;


#endif /* CHUNKALLOCATOR_HPP */
