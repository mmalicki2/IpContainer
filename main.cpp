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
#include <iostream>
#include <vector>
#include <algorithm>
#include <cassert>
#include <functional>
#include <stdexcept>
    

#include <arpa/inet.h>

#include "IpContainer.hpp"

class IpContainerTest : public IpContainer {

public:
    typedef std::function<void(const data_type&)> list_visitor_type;

    void add(const std::string& ip, char mask);
    void del(const std::string& ip, char mask);
    char check(const std::string& ip);
    void list(list_visitor_type& visitor);
private:
    void list(pointer node, list_visitor_type& visitor);
    unsigned int getBase(const std::string& ip);
};

using namespace std;


void IpContainerTest::add(const std::string& ip, char mask)
{
    cerr << "add: " << ip << "/" << (int)mask << " " << flush;
    if (IpContainer::add(getBase(ip), mask) == -1) {
        cerr << "not added" << endl;
        throw std::runtime_error("Error");
    }
    cerr << "added" << endl;
}

void IpContainerTest::del(const std::string& ip, char mask)
{
    cerr << "del: " << ip << "/" << (int)mask << " " << flush;
    if (IpContainer::del(getBase(ip), mask) == -1) {
        cerr << "not removed" << endl;
        throw std::runtime_error("Error");
    }
    cerr << "removed" << endl;
}
    

char IpContainerTest::check(const std::string& ip)
{
    return IpContainer::check(getBase(ip));
}

void IpContainerTest::list(pointer node, list_visitor_type& visitor)
{
    if (node->isInner()) {
        assert(node->inner.zero->getParent() == node);
        assert(node->inner.one->getParent() == node);

        list(node->inner.zero, visitor);
        list(node->inner.one, visitor);
    } else {
        visitor(*node->leaf.data);
    }
}

void IpContainerTest::list(list_visitor_type& visitor)
{
    if (root->root.child == pointer()) {
        return; 
    } 
    list(root->root.child, visitor);
}

unsigned int IpContainerTest::getBase(const std::string& ip)
{
    char binip[4];
    inet_pton(AF_INET, ip.c_str(), &binip);

    uint32_t ret = 0;
    for (int i = 0; i < 4; ++i) {
        ret <<= 8;
        ret |= 0xff & binip[i];
    }
    return ret;
}

#define CHECK(v)                                  \
    if (v) {                                      \
        std::cerr << "ERROR " << #v << std::endl; \
    } else {                                      \
        std::cerr << "OK: " << #v << std::endl;   \
    }                                             \
 
#define CHECK_EQUAL(v, val) do {                      \
        auto i_ = v;                                  \
        if (i_ != val) {                              \
            std::cerr << "ERROR " << #v               \
                      << "(" << std::dec << (int)i_   \
                      << " != " <<  val << ")"        \
                      << std::endl;                   \
        } else {                                      \
            std::cerr << "Ok: " << #v << std::endl;   \
        }                                             \
    } while(0)

#define REQUIRE_THROW(v) do {                                    \
        try {                                                    \
            v;                                                   \
        } catch (...) {                                          \
            break;                                               \
        }                                                        \
        throw std::runtime_error("Exception not being thrown");  \
    } while(0)
    

std::function<void(const IpContainerTest::data_type&)> printVisitor = [](const IpContainerTest::data_type& data)
{
    cerr << dec <<        ((data.ip & (0xFF000000)) >> 24)  
                << "." << ((data.ip & (0x00FF0000)) >> 16) 
                << "." << ((data.ip & (0x0000FF00)) >>  8)
                << "." << ((data.ip & (0x000000FF)) >>  0) << ": ";
    for (int i = 0; i < data.prefixes.size(); ++i) {
        cerr << (i != 0 ? ", " : "") << (unsigned)data.prefixes[i];
    }
    cerr << ";" << endl;
};   


void test_add() {
    IpContainerTest container;
    container.add("0.0.0.128", 25);
    CHECK_EQUAL(container.check("0.0.0.128"), 25);
    container.add("0.0.0.128", 26);
    CHECK_EQUAL(container.check("0.0.0.128"), 26);
    container.add("0.0.0.128", 27);
    CHECK_EQUAL(container.check("0.0.0.128"), 27);
    container.add("0.0.0.130", 31);
    CHECK_EQUAL(container.check("0.0.0.130"), 31);
    container.add("1.0.0.130", 31);
    CHECK_EQUAL(container.check("1.0.0.130"), 31);
    container.add("1.0.1.130", 31);
    CHECK_EQUAL(container.check("1.0.1.130"), 31);
    container.add("0.0.0.128", 26);
    container.add("0.0.0.128", 26);
    container.add("0.0.0.128", 26);
    CHECK_EQUAL(container.check("0.0.0.130"), 31);
}

void test_del()
{
    IpContainerTest container;
    container.add("0.0.0.128", 25);
    container.add("0.0.0.128", 26);
    container.add("0.0.0.128", 27);
    container.add("0.0.0.130", 31);
    container.add("1.0.0.130", 31);
    container.add("1.0.1.130", 31);
    container.add("0.0.0.128", 26);
    container.list(printVisitor);
    
    CHECK_EQUAL(container.check("0.0.0.128"), 27);
    CHECK_EQUAL(container.check("0.0.0.130"), 31);
    CHECK_EQUAL(container.check("1.0.0.130"), 31);
    CHECK_EQUAL(container.check("1.0.1.130"), 31);
    CHECK_EQUAL(container.check("0.0.0.130"), 31);
    CHECK_EQUAL(container.check("0.0.0.128"), 27);

    container.del("0.0.0.128", 27);
    container.list(printVisitor);
    CHECK_EQUAL(container.check("0.0.0.128"), 26);
    container.del("0.0.0.128", 25);
    container.list(printVisitor);
    CHECK_EQUAL(container.check("0.0.0.128"), 26);
    container.del("0.0.0.128", 26);
    container.list(printVisitor);
    CHECK_EQUAL(container.check("0.0.0.128"), -1);
    REQUIRE_THROW(container.del("0.0.0.128", 26));

    container.list(printVisitor);
    container.del("0.0.0.130", 31);
    container.list(printVisitor);
    container.del("1.0.0.130", 31);
    container.list(printVisitor);
    CHECK_EQUAL(container.check("1.0.1.130"), 31);
    container.list(printVisitor);
    container.del("1.0.1.130", 31);
    container.list(printVisitor);
    REQUIRE_THROW(container.del("0.0.0.128", 26));
    
    CHECK_EQUAL(container.check("0.0.0.128"), -1);
    CHECK_EQUAL(container.check("0.0.0.128"), -1);
    CHECK_EQUAL(container.check("0.0.0.128"), -1);
    CHECK_EQUAL(container.check("0.0.0.130"), -1);
    CHECK_EQUAL(container.check("1.0.0.130"), -1);
    CHECK_EQUAL(container.check("1.0.1.130"), -1);
    CHECK_EQUAL(container.check("0.0.0.130"), -1);
}


int main(int argc, const char** argv)
{
    cerr << "\nTest add" << endl;
    test_add();

    cerr << "\nTest del" << endl;
    test_del();

    return 0;
}
