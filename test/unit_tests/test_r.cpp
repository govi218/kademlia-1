// Copyright (c) 2013-2014, David Keller
// All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the University of California, Berkeley nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY DAVID KELLER AND CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <iostream>

#include "helpers/common.hpp"
#include "kademlia/r.hpp"

namespace k = kademlia;
namespace kd = k::detail;

namespace {

struct test_type
{
    test_type
        ( int a
        , int b )
        : a_{ a }
        , b_{ b }
        , moved_{ false }
    { }

    test_type
        ( test_type && other )
        : a_{ other.a_ }
        , b_{ other.b_ }
        , moved_{ false }
    { other.moved_ = true; }

    test_type
        ( test_type const & other )
        : a_{ other.a_ }
        , b_{ other.b_ }
        , moved_{ false }
    { }

    int a_;
    int b_;
    bool moved_;
};

bool
operator==
    ( test_type const & a
    , test_type const & b )
{ return a.a_ == b.a_ && a.b_ == b.b_; }

std::ostream &
operator<<
    ( std::ostream & out
    , test_type const & a )
{ return out << a.a_ << " " << a.b_; }


} // namespace

BOOST_AUTO_TEST_SUITE( test_construction )

BOOST_AUTO_TEST_CASE( can_be_constructed_from_a_value )
{
    using type = kd::r< int >;

    type::value_type value{ 42 };
    static type::error_type const NO_ERROR;

    {
        type r{ value };
        BOOST_REQUIRE( r );
        BOOST_REQUIRE_EQUAL( value, r.v() );
        BOOST_REQUIRE_EQUAL( NO_ERROR, r.e() );
    }

    {
        const type r{ value };
        BOOST_REQUIRE( r );
        BOOST_REQUIRE_EQUAL( value, r.v() );
        BOOST_REQUIRE_EQUAL( NO_ERROR, r.e() );
    }

    {
        type r{ 42 };
        BOOST_REQUIRE( r );
        BOOST_REQUIRE_EQUAL( 42, r.v() );
        BOOST_REQUIRE_EQUAL( NO_ERROR, r.e() );
    }
}

BOOST_AUTO_TEST_CASE( can_be_constructed_emplace )
{
    kd::r< test_type > r{ 42, 69 };

    BOOST_REQUIRE( r );
    BOOST_REQUIRE_EQUAL( 42, r.v().a_ );
    BOOST_REQUIRE_EQUAL( 69, r.v().b_ );
}

BOOST_AUTO_TEST_CASE( can_be_constructed_from_an_error )
{
    using type = kd::r< int >;

    auto error = make_error_code( std::errc::address_in_use );
    {
        type r{ error };
        BOOST_REQUIRE( ! r );
        BOOST_REQUIRE_THROW( r.v(), type::exception_type );
        BOOST_REQUIRE_EQUAL( error, r.e() );
    }
}

BOOST_AUTO_TEST_CASE( can_be_copy_constructed )
{
    using type = kd::r< test_type >;

    {
        type r1{ 42, 69 };
        type r2{ r1 };
        BOOST_REQUIRE( r2 );
        BOOST_REQUIRE_EQUAL( r1.v(), r2.v() );
        BOOST_REQUIRE_EQUAL( r1.e(), r2.e() );
    }

    {
        const type r1{ 42, 69 };
        type r2{ r1 };
        BOOST_REQUIRE( r2 );
        BOOST_REQUIRE_EQUAL( r1.v(), r2.v() );
        BOOST_REQUIRE_EQUAL( r1.e(), r2.e() );
    }

    {
        type r1{ make_error_code( std::errc::address_in_use ) };
        type r2{ r1 };
        BOOST_REQUIRE( ! r2 );
        BOOST_REQUIRE_EQUAL( r1.e(), r2.e() );
    }

    {
        const type r1{ make_error_code( std::errc::address_in_use ) };
        type r2{ r1 };
        BOOST_REQUIRE( ! r2 );
        BOOST_REQUIRE_EQUAL( r1.e(), r2.e() );
    }
}

BOOST_AUTO_TEST_CASE( can_be_move_constructed )
{
    using type = kd::r< test_type >;

    {
        type r1{ 42, 69 };
        type r2{ std::move( r1 ) };
        BOOST_REQUIRE( r1.v().moved_ );
        BOOST_REQUIRE( r2 );
        BOOST_REQUIRE_EQUAL( r1.v(), r2.v() );
        BOOST_REQUIRE_EQUAL( r1.e(), r2.e() );
    }
    {
        type r1{ make_error_code( std::errc::address_in_use ) };
        type r2{ std::move( r1 ) };
        BOOST_REQUIRE( ! r2 );
        BOOST_REQUIRE_EQUAL( r1.e(), r2.e() );
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( test_assignment )

BOOST_AUTO_TEST_CASE( can_be_assigned )
{
    using type = kd::r< test_type >;

    {
        type r1{ 0, 0 };
        test_type v1{ 42, 69 };
        r1 = v1;
        BOOST_REQUIRE( r1 );
        BOOST_REQUIRE_EQUAL( v1, r1.v() );
    }
    {
        type r1{ 0, 0 };
        const test_type v1{ 42, 69 };
        r1 = v1;
        BOOST_REQUIRE( r1 );
        BOOST_REQUIRE_EQUAL( v1, r1.v() );
    }
    {
        type r1{ 0, 0 };
        auto e1 = make_error_code( std::errc::address_in_use );
        r1 = e1;
        BOOST_REQUIRE( ! r1 );
        BOOST_REQUIRE_EQUAL( e1, r1.e() );
    }
    {
        type r1{ 0, 0 };
        auto const e1 = make_error_code( std::errc::address_in_use );
        r1 = e1;
        BOOST_REQUIRE( ! r1 );
        BOOST_REQUIRE_EQUAL( e1, r1.e() );
    }
    {
        type r1{ make_error_code( std::errc::address_family_not_supported ) };
        auto e1 = make_error_code( std::errc::address_in_use );
        r1 = e1;
        BOOST_REQUIRE( ! r1 );
        BOOST_REQUIRE_EQUAL( e1, r1.e() );
    }
    {
        type r1{ make_error_code( std::errc::address_family_not_supported ) };
        auto const e1 = make_error_code( std::errc::address_in_use );
        r1 = e1;
        BOOST_REQUIRE( ! r1 );
        BOOST_REQUIRE_EQUAL( e1, r1.e() );
    }
}

BOOST_AUTO_TEST_SUITE_END()

