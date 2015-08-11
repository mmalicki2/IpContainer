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
#include <vector>
#include <algorithm>
#include <cassert>

#include "IpContainer.hpp"

namespace {

template<class T>
void swap(T& v1, T& v2)
{
    T t = v1;
    v1 = v2;
    v2 = t;
}

} // namespace


/** DataNode implementation **/

bool DataNode::contain(char prefix)
{
    return std::binary_search(prefixes.begin(), prefixes.end(), prefix);
}

void DataNode::addPrefix(char prefix)
{
    if (contain(prefix)) {
        return;
    }

    prefixes.push_back(prefix);
    for (int i = prefixes.size() - 1; i > 0 ; --i) {
        if (!(prefixes[i-1] < prefixes[i])) {
            swap(prefixes[i-1], prefixes[i]);
        }
    }       
}

int DataNode::removePrefix(char prefix)
{
    vector_type::iterator it = std::lower_bound(prefixes.begin(), prefixes.end(), prefix); 
    if (it != prefixes.end() && !(prefix < *it)) { 
        prefixes.erase(it);
        return 0;
    }
    return -1;
}

char DataNode::getMaxPrefixForIp(uint32_t ip_)
{
    for (int i = prefixes.size() - 1; i >= 0; --i) {
        uint32_t mask = static_cast<uint32_t>(-1) << (32 - prefixes[i]);
        if ((ip & mask) == (ip_ & mask)) {
            return prefixes[i];
        }
    }
    return -1;
}


/** IpContainer implementation **/
IpContainer::IpContainer()
{
    //XXX: Because root cannot be updated IpContainer should be singleton
    root = node_alloc.allocate(1);
    node_alloc.construct(root, node_type());
    root->setRoot();
    root->root.parent = pointer();
    root->root.child = pointer();
}

IpContainer::~IpContainer()
{
    while (!empty()) {
        pointer p = findAny();
        del(p->leaf.data->ip, *p->leaf.data->prefixes.rbegin());
    }

    disconnectNode(root);
    deleteNode(root);
}

bool IpContainer::validate(unsigned int base, char mask)
{
    bool v1 = mask >= 0 && mask <= 32 && base <= (1 << 31);
    return v1 && ((base & (static_cast<uint32_t>(-1) >> mask)) == 0);
}
    
char IpContainer::getDiffBit(uint32_t v1, uint32_t v2)
{
    char diffBit = 31;
    while(((v1 ^ v2) & (static_cast<uint32_t>(1) << diffBit)) == 0) {
        diffBit--;
    }
    
    return diffBit;
}

IpContainer::pointer IpContainer::createLeafNode(uint32_t ip, char mask)
{
    pointer node = node_alloc.allocate(1);
    node_alloc.construct(node, node_type());
    node->setLeaf();
    node->leaf.data = data_alloc.allocate(1);
    data_alloc.construct(node->leaf.data, data_type());
    node->leaf.data->ip = ip;
    node->leaf.data->addPrefix(mask);
    node->leaf.parent = pointer();
    return node;
}

IpContainer::pointer IpContainer::createInnerNode()
{
    pointer node = node_alloc.allocate(1);
    node_alloc.construct(node, node_type());
    node->setInner();
    node->inner.parent = pointer();
    return node;
}
    
IpContainer::pointer IpContainer::createParentNode(pointer newNode, pointer siblingNode, char diffBit)
{
    
    pointer oneNode = siblingNode;
    pointer zeroNode = newNode;
    if (zeroNode->leaf.data->ip & (1 << diffBit)) {
        swap(zeroNode, oneNode);
    }

    pointer parent = createInnerNode();
    parent->inner.zero = zeroNode;
    parent->inner.one = oneNode;
    parent->inner.branchMask = diffBit;

    if (zeroNode->isLeaf()) {
        zeroNode->leaf.parent = parent;
        assert((zeroNode->leaf.data->ip & (1 << diffBit)) == 0);
    } else {
        assert(zeroNode->isInner());
        zeroNode->inner.parent = parent;
    }
    if (oneNode->isLeaf()) {
        oneNode->leaf.parent = parent;
        assert((oneNode->leaf.data->ip & (1 << diffBit)) != 0);
    } else {
        assert(oneNode->isInner());
        oneNode->inner.parent = parent;
    }
    return parent;
}
    
void IpContainer::deleteNode(pointer node)
{
    assert(node->getParent() == pointer());
    if (node->isLeaf()) {
        data_alloc.destroy(node->leaf.data);
        data_alloc.deallocate(node->leaf.data, 1);
    } else if (node->isInner()) {
        assert(node->inner.zero == pointer());
        assert(node->inner.one == pointer());
    } else {
        assert(node->isRoot());
        assert(node->root.child == pointer());
    }
    node_alloc.destroy(node);

    //NOTE: Local pointers that are greater than deallocate node can be invalid
    node_alloc.deallocate(node, 1);
}

void IpContainer::disconnectNode(pointer node)
{
    node_type& n = *node;
    if (node->isInner()) {
        node->inner.parent = pointer();
        node->inner.zero = pointer();
        node->inner.one = pointer();
    } else if (node->isLeaf()) {
        node->leaf.parent = pointer();
    } else {
        assert(node->isRoot());
        assert(node->root.parent == pointer());
        node->root.child = pointer();
    }
}
    
IpContainer::pointer IpContainer::findNode(uint32_t ip) const
{
    pointer node = root->root.child;
    assert(node != pointer());
    while (node->isInner()) {
        if ((1 << node->inner.branchMask) & ip) {
            node = node->inner.one;
        } else {
            node = node->inner.zero;
        }
    }
    assert(node->isLeaf());
    return node;
}
    
IpContainer::pointer IpContainer::findAny() const
{
    pointer node = root->root.child;
    while (node->isInner()) {
            node = node->inner.one;
    }
    assert(node->isLeaf());
    return node;
}

bool IpContainer::empty() const
{
    return root->root.child == pointer();
}

int IpContainer::add(unsigned int base, char mask)
{
    if (!validate(base, mask)) {
        return -1;
    }

    if (root->root.child == pointer()) {
        root->root.child = createLeafNode(base, mask);
        root->root.child->leaf.parent = root;
        return 0;
    }

    pointer node = findNode(base);
    if (node->leaf.data->ip == base) {
        node->leaf.data->addPrefix(mask);
        return 0;
    }
    
    char diffBit = getDiffBit(node->leaf.data->ip, base);

    if (node == root->root.child) {
        node = createLeafNode(base, mask);
        root->root.child = createParentNode(node, root->root.child, diffBit);   
        root->root.child->inner.parent = root;
        return 0;
    }

    assert(root->root.child->isInner());
    if (root->root.child->inner.branchMask < diffBit) {
        node = createLeafNode(base, mask);
        root->root.child = createParentNode(node, root->root.child, diffBit);
        root->root.child->inner.parent = root;
        return 0;
    }
    
    pointer parentNode = node->leaf.parent;
    while (parentNode->inner.branchMask < diffBit) {
        node = parentNode;
        parentNode = parentNode->inner.parent;
    }
    assert(parentNode->inner.branchMask > diffBit);
    
    pointer newLeafNode = createLeafNode(base, mask);
    pointer newInnerNode = createParentNode(newLeafNode, node, diffBit);
    newInnerNode->inner.parent = parentNode;
    if (parentNode->inner.zero == node) {
        parentNode->inner.zero = newInnerNode;
    } else {
        assert(parentNode->inner.one == node);
        parentNode->inner.one = newInnerNode;
    }
    return 0; 
}

char IpContainer::check(unsigned int ip)
{
    if (root->root.child == pointer()) {
        return -1; 
    } 
    pointer node = findNode(ip);
    return node->leaf.data->getMaxPrefixForIp(ip);
}

int IpContainer::del(unsigned int base, char mask)
{
    if (root->root.child == pointer()) {
        return -1; 
    } 
    pointer node = findNode(base);
    char ret = node->leaf.data->removePrefix(mask);
    if (ret == -1) {
        return -1;
    }
    if (!node->leaf.data->prefixes.empty()) {
        return 0;
    }
    if (node == root->root.child) {
        disconnectNode(root->root.child);

        deleteNode(root->root.child);
        root->root.child = pointer();
        return 0;
    }    

    pointer child;
    pointer oldParent = node->leaf.parent;
    pointer newParent = oldParent->inner.parent;
    if (oldParent->inner.zero == node) {
        child = oldParent->inner.one;
    } else {
        assert(oldParent->inner.one == node);
        child = oldParent->inner.zero;
    }
    if (child->isLeaf()) {
        child->leaf.parent = newParent;
    } else {
        child->inner.parent = newParent;
    }
    if (!newParent->isRoot()) {
        if (newParent->inner.zero == oldParent) {
            newParent->inner.zero = child;
        } else {
            assert(newParent->inner.one == oldParent);
            newParent->inner.one = child;
        }
        assert(newParent->inner.one->getParent() == newParent);
        assert(newParent->inner.zero->getParent() == newParent);
    } else {
        assert(newParent == root);
        root->root.child = child;
        assert(root->root.child->getParent() == root);
    }
    assert(child->getParent() == newParent);

    //XXX: Local pointers that are greater than deleted node can be invalid
    if (node < oldParent) {
        swap(node, oldParent);
    }

    disconnectNode(node);
    disconnectNode(oldParent);

    deleteNode(node);
    deleteNode(oldParent);
}
