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

#ifdef _MSC_VER
#   pragma warning( push )
#   pragma warning( disable : 4996 )
#endif

#include <kademlia/session.hpp>

#include <list>
#include <map>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <random>
#include <boost/asio/io_service.hpp>

#include <kademlia/error.hpp>

#include "message_socket.hpp"
#include "message.hpp"
#include "routing_table.hpp"
#include "subnet.hpp"
#include "response_dispatcher.hpp"

namespace kademlia {

namespace {

CXX11_CONSTEXPR std::chrono::milliseconds INITIAL_CONTACT_RECEIVE_TIMEOUT{ 1000 };

} // anonymous namespace

/**
 *
 */
class session::impl final
{
public:
    /**
     *
     */
    explicit 
    impl
        ( endpoint const& initial_peer
        , endpoint const& listen_on_ipv4
        , endpoint const& listen_on_ipv6 )
            : random_engine_{ std::random_device{}() }
            , my_id_( random_engine_ )
            , io_service_{}
            , initial_peer_{ initial_peer }
            , ipv4_subnet_{ create_ipv4_subnet( io_service_, listen_on_ipv4 ) }
            , ipv6_subnet_{ create_ipv6_subnet( io_service_, listen_on_ipv6 ) }
            , routing_table_{ my_id_ }
            , response_dispatcher_{ io_service_ }
            , tasks_{}
            , main_failure_{}
    { }
    
    /**
     *
     */
    void
    async_save
        ( key_type const& key 
        , data_type const& data
        , save_handler_type handler )
    { throw std::system_error{ make_error_code( UNIMPLEMENTED ) }; }

    /**
     *
     */
    void
    async_load
        ( key_type const& key
        , load_handler_type handler )
    { throw std::system_error{ make_error_code( UNIMPLEMENTED ) }; }

    /**
     *
     */
    std::error_code
    run
        ( void ) 
    {
        main_failure_.clear();
        init();
        
        while ( ! main_failure_ && io_service_.run_one() )
        {
            io_service_.poll();
            destroy_finished_tasks();
        }

        return main_failure_;
    }

    /**
     *
     */
    void
    abort
        ( void )
    { 
        io_service_.stop();

        main_failure_ = make_error_code( RUN_ABORTED );
    }

private:
    ///
    using task_ptr = std::shared_ptr< detail::task_base >;

    ///
    using tasks = std::list< task_ptr >;

private:
    /**
     *
     */
    void
    init
        ( void )
    {
        io_service_.reset();

        start_receive_on_each_subnet();
        contact_initial_peer();
    }

    /**
     *
     */
    static detail::subnet
    create_ipv4_subnet
        ( boost::asio::io_service & io_service
        , endpoint const& ipv4_endpoint )
    {
        auto endpoints = detail::resolve_endpoint( io_service, ipv4_endpoint );

        for ( auto const & i : endpoints )
            if ( i.address().is_v4() )
                return detail::subnet{ detail::create_socket( io_service, i ) };

        throw std::system_error{ make_error_code( INVALID_IPV4_ADDRESS ) };
    }

    /**
     *
     */
    static detail::subnet
    create_ipv6_subnet
        ( boost::asio::io_service & io_service
        , endpoint const& ipv6_endpoint )
    {
        auto endpoints = detail::resolve_endpoint( io_service, ipv6_endpoint );

        for ( auto const& i : endpoints )
            if ( i.address().is_v6() )
                return detail::subnet{ detail::create_socket( io_service, i ) };

        throw std::system_error{ make_error_code( INVALID_IPV6_ADDRESS ) };
    }

    /**
     *
     */
    void
    contact_initial_peer
        ( void )
    {
#if 0
        auto last_request_sending_time = std::chrono::steady_clock::now();
        auto current_subnet = subnets_.begin();
        auto endpoints_to_try = detail::resolve_endpoint( io_service_, initial_peer_ );
        auto new_task = [ this, last_request_sending_time, current_subnet, endpoints_to_try ] 
            ( void ) mutable -> std::error_code
        { 
            // If the routing_table has been filled, we can leave this method.
            if ( routing_table_.peer_count() != 0 )
                return std::error_code{};

            // If timeout is not yet elasped, return without trying another endpoint/subnet.
            auto const timeout = std::chrono::steady_clock::now() - last_request_sending_time; 
            if ( timeout <= INITIAL_CONTACT_RECEIVE_TIMEOUT )
                return make_error_code( std::errc::operation_in_progress );

            // Current endpoint didn't respond after timeout,
            // hence try with another one.
            for (
                ; ! endpoints_to_try.empty()
                ; endpoints_to_try.pop_back() )
                // Try first compatible subnet.
                for ( 
                    ; current_subnet != subnets_.end()
                    ; ++ current_subnet )
                {
                    // Try to send the request, returning a non zero std::error_code on error.
                    if ( send_initial_request( endpoints_to_try.back(), *current_subnet ) )
                        continue;

                    last_request_sending_time = std::chrono::steady_clock::now();

                    // And exit.
                    return make_error_code( std::errc::operation_in_progress );
                }
                
                current_subnet = subnets_.begin();

            // No responsive endpoint has been found.
            main_failure_ = make_error_code( INITIAL_PEER_FAILED_TO_RESPOND );
            return make_error_code( std::errc::timed_out ); 
        };
#endif
    }

    /**
     *
     */
    void
    start_receive_on_each_subnet
        ( void )
    {
        schedule_receive_on_subnet( ipv4_subnet_ );
        schedule_receive_on_subnet( ipv6_subnet_ );
    }

    /**
     *
     */
    void
    schedule_receive_on_subnet
        ( detail::subnet & current_subnet )
    {
        auto on_new_message = [ this, &current_subnet ]
            ( std::error_code const& failure
            , detail::message_socket::endpoint_type const& sender
            , detail::buffer const& message )
        {
            if ( failure )
                main_failure_ = failure;
            else
            {
                handle_new_message( current_subnet, sender, message );
                schedule_receive_on_subnet( current_subnet );
            }
        };

        current_subnet.async_receive( on_new_message );
    }

    /**
     *
     *  @note This method do not own the task.
     */
    std::error_code 
    send_initial_request
        ( detail::message_socket::endpoint_type const& endpoint_to_try
        , detail::subnet & current_subnet
        , detail::task_base * initial_request_task )
    {
        // Ensure we can access to the current peer address
        // on the current subnet (i.e. we don't try
        // to reach an IPv4 peer from an IPv6 socket).
        if ( endpoint_to_try.protocol() != current_subnet.local_endpoint().protocol() )
            make_error_code( std::errc::address_family_not_supported );

        detail::id const request_id{ random_engine_ };
        auto message = generate_initial_request( request_id );

        // The purpose of this object is to ensure
        // the message buffer lives long enought.
        auto on_message_sent = [ message ]
            ( std::error_code const& /* failure */ )
        { };

        associate_response_with_task( request_id, initial_request_task );
        current_subnet.async_send( *message, endpoint_to_try, on_message_sent );

        return std::error_code{};
    } 

    /**
     *
     */
    std::shared_ptr< detail::buffer >
    generate_initial_request
        ( detail::id const& request_id )
    {
        auto new_message = std::make_shared< detail::buffer >();

        detail::header const find_node_header
        {
            detail::header::V1,
            detail::header::FIND_NODE_REQUEST,
            my_id_,
            request_id
        };

        detail::find_node_request_body const find_node_request_body{ my_id_ };

        serialize( find_node_header, *new_message );
        serialize( find_node_request_body, *new_message );

        return new_message;
    }

    /**
     *
     */
    void
    handle_new_message
        ( detail::subnet & source_subnet
        , detail::message_socket::endpoint_type const& sender
        , detail::buffer const& message )
    {
        auto i = message.begin(), e = message.end();

        detail::header h;
        // Try to deserialize header.
        if ( deserialize( i, e, h ) )
            return;

        switch ( h.type_ )
        {
            case detail::header::PING_REQUEST: 
                handle_ping_request( source_subnet, sender, h );
                break;
            case detail::header::STORE_REQUEST: 
                handle_store_request( source_subnet, sender, h, i, e );
                break;
            case detail::header::FIND_NODE_REQUEST: 
                handle_find_node_request( source_subnet, sender, h, i, e );
                break;
            case detail::header::FIND_VALUE_REQUEST:
                handle_find_value_request( source_subnet, sender, h, i, e );
                break;
            default:
                handle_response( h, i, e );
        }
    }

    /**
     *
     */
    void
    handle_response 
        ( detail::header const& h
        , detail::buffer::const_iterator i
        , detail::buffer::const_iterator e )
    { response_dispatcher_.dispatch_message( h, i, e ); }

    /**
     *
     */
    void
    handle_ping_request
        ( detail::subnet & source_subnet
        , detail::message_socket::endpoint_type const& sender
        , detail::header const& h )
    {

    }

    /**
     *
     */
    void
    handle_store_request
        ( detail::subnet & source_subnet
        , detail::message_socket::endpoint_type const& sender
        , detail::header const& h
        , detail::buffer::const_iterator i
        , detail::buffer::const_iterator e )
    {

    }

    /**
     *
     */
    void
    handle_find_node_request
        ( detail::subnet & source_subnet
        , detail::message_socket::endpoint_type const& sender
        , detail::header const& h
        , detail::buffer::const_iterator i
        , detail::buffer::const_iterator e )
    {

    }


    /**
     *
     */
    void
    handle_find_value_request
        ( detail::subnet & source_subnet
        , detail::message_socket::endpoint_type const& sender
        , detail::header const& h
        , detail::buffer::const_iterator i
        , detail::buffer::const_iterator e )
    {

    }

    /**
     *
     */
    void
    associate_response_with_task
        ( detail::id const& request_id
        , detail::task_base * task )
    { 
        response_dispatcher_.associate_response_with_task_for( request_id
                                                             , task
                                                             , INITIAL_CONTACT_RECEIVE_TIMEOUT );
    }

    /**
     *
     */
    template< typename Task, typename ... Args >
    void 
    create_new_task
        ( Args ... args )
    {
        auto t = std::make_shared< Task >( std::forward< Args >( args )... ); 
        tasks_.push_back( t );
    }

    /**
     *
     */
    void
    destroy_finished_tasks
        ( void )
    {
        auto is_task_finished = []( task_ptr const& t )

        { return t->is_finished(); };
        tasks_.remove_if( is_task_finished );
    }

private:
    std::default_random_engine random_engine_;
    detail::id my_id_;
    boost::asio::io_service io_service_;
    endpoint initial_peer_;
    detail::subnet ipv4_subnet_;
    detail::subnet ipv6_subnet_;
    detail::routing_table routing_table_;
    detail::response_dispatcher response_dispatcher_;
    tasks tasks_;
    std::error_code main_failure_;
};

session::session
    ( endpoint const& initial_peer
    , endpoint const& listen_on_ipv4
    , endpoint const& listen_on_ipv6 )
    : impl_{ new impl{ initial_peer, listen_on_ipv4, listen_on_ipv6 } }
{ }

session::~session
    ( void )
{ }

void
session::async_save
    ( key_type const& key 
    , data_type const& data
    , save_handler_type handler )
{ impl_->async_save( key, data, handler ); }

void
session::async_load
    ( key_type const& key
    , load_handler_type handler )
{ impl_->async_load( key, handler ); }

std::error_code
session::run
        ( void )
{ return impl_->run(); }

void
session::abort
        ( void )
{ impl_->abort(); }

} // namespace kademlia

#ifdef _MSC_VER
#   pragma warning( pop )
#endif
