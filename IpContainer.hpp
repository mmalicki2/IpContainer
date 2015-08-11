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
#ifndef IPCONTAINER_HPP
#define IPCONTAINER_HPP

#include <vector>
#include <functional>
#include <cassert>

#include "ChunkAllocator.hpp"

struct DataNode
{
    typedef std::vector<char> vector_type;

    uint32_t ip;
    vector_type prefixes;

    bool contain(char prefix);
    void addPrefix(char prefix);
    int removePrefix(char prefix);
    char getMaxPrefixForIp(uint32_t ip_);
};

template<class Pointer>
struct RootNode {
    //TODO: Root node should contain pointer to variable that is used to tree access.
    //      This will enable the root node updating.
    typedef Pointer pointer;

    uint32_t flag;
    pointer parent;
    pointer child;
};

template<class Pointer>
struct InnerNode {
    typedef Pointer pointer;

    uint32_t branchMask;
    pointer parent;
    pointer one;
    pointer zero;
};

template<class NodePointer, class DataPointer>
struct LeafNode {
    typedef NodePointer node_pointer;
    typedef DataPointer data_pointer;

    uint32_t     flag;
    node_pointer parent;
    data_pointer data;
};

template<class NodePointer, class Data>
union Node {
    typedef Data                                   data_type;
    typedef std::allocator<data_type>              data_allocator_type;
    typedef typename data_allocator_type::pointer  data_pointer;

    typedef Node<NodePointer, data_type>            node_type;
    typedef ChunkAllocator<node_type, NodePointer>  node_allocator_type;
    typedef typename node_allocator_type::pointer   node_pointer;
   
    struct RootNode<node_pointer>               root; 
    struct InnerNode<node_pointer>              inner;
    struct LeafNode<node_pointer, data_pointer> leaf;

    Node();
    Node(const node_type& node);
    bool isLeaf() const;
    bool isInner() const;
    bool isRoot() const;
    void setRoot();
    void setLeaf();
    void setInner();
    node_pointer getParent() const;
    void setParent(node_pointer node);

    //ChunkAllocator concept
    void UpdateChunk(node_pointer newPointer, node_pointer oldPointer);
};

class IpContainer {
public:
    typedef DataNode                                  data_type;

    IpContainer();
    ~IpContainer();
    int add(unsigned int base, char mask);
    int del(unsigned int base, char mask);
    char check(unsigned int ip);

protected:
    typedef Node<uint32_t, DataNode>                  node_type;
    typedef typename node_type::node_allocator_type   node_allocator_type;
    typedef typename node_type::data_allocator_type   data_allocator_type;
    typedef typename node_allocator_type::pointer     pointer;
    typedef std::function<void(const pointer&)>       node_visitor_type;
    
    node_allocator_type node_alloc;
    data_allocator_type data_alloc;
    pointer root;

    bool validate(unsigned int base, char mask);
    char getDiffBit(uint32_t v1, uint32_t v2);
    pointer createLeafNode(uint32_t ip, char mask);
    pointer createInnerNode();
    pointer createParentNode(pointer newNode, pointer siblingNode, char diffBit);
    void deleteNode(pointer node);
    void disconnectNode(pointer node);
    pointer findNode(uint32_t ip) const;
    pointer findAny() const;
    bool empty() const;
};


/** Node implementation **/

template<class N, class D>
Node<N,D>::Node()
{
    setRoot();
}

template<class N, class D>
Node<N,D>::Node(const node_type& node)
{
    if (node.isLeaf()) {
        leaf = node.leaf;
    } else if (node.isRoot()) {
        root = node.root;
    } else {
        inner = node.inner;
    }
}

template<class N, class D>
bool Node<N,D>::isRoot() const {
    return root.flag == static_cast<uint32_t>(-2);
}

template<class N, class D>
bool Node<N,D>::isLeaf() const {
    return leaf.flag == static_cast<uint32_t>(-1);
}

template<class N, class D>
bool Node<N,D>::isInner() const {
    return !isLeaf() && !isRoot();
}

template<class N, class D>
void Node<N,D>::setRoot() {
    root.flag = static_cast<uint32_t>(-2);
}

template<class N, class D>
void Node<N,D>::setLeaf() {
    leaf.flag = static_cast<uint32_t>(-1);
}

template<class N, class D>
void Node<N,D>::setInner() {
    leaf.flag = 0;
}

template<class N, class D>
typename Node<N,D>::node_pointer Node<N,D>::getParent() const
{
    if (isLeaf()) {
        return leaf.parent; 
    } else if (isInner()) {
        return inner.parent;
    } else {
        assert(isRoot());
        return node_pointer();
    }
}

template<class N, class D>
void Node<N,D>::setParent(node_pointer node)
{
    if (isLeaf()) {
        leaf.parent = node; 
    } else {
        assert(isInner()); 
        inner.parent = node;
    }   
}

template<class N, class D>
void Node<N,D>::UpdateChunk(node_pointer newPointer, node_pointer oldPointer)
{
    assert(!isRoot() && "Root can't be upated");

    node_pointer node = getParent();
    if (node == node_pointer()) {
        //this node is disconnected or it is root
        return;
    }
    
    if (node->isRoot()) {
        assert(node->root.child == oldPointer);
        node->root.child = newPointer;
        assert(node->root.child->getParent() == node);
    } else {   
        assert(node->isInner());
        if (node->inner.zero == oldPointer) {
            node->inner.zero = newPointer;
        } else {
            assert(node->inner.one == oldPointer);
            node->inner.one = newPointer;
        }
        assert(node->inner.zero->getParent() == node);
        assert(node->inner.one->getParent() == node);
    }

    if (newPointer->isInner() && newPointer->inner.zero != node_pointer()) {
        assert(newPointer->inner.one != node_pointer());
        newPointer->inner.zero->setParent(newPointer);
        newPointer->inner.one->setParent(newPointer);
    }
}

#endif /* IPCONTAINER_HPP */
