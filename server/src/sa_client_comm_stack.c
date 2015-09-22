/*******************************************************************************
Copyright (C) 2015 OLogN Technologies AG

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*******************************************************************************/


#include <simpleiot/siot_common.h>
#include "commstack_commlayer.h"
#include <hal_time_provider.h>
#include <simpleiot/siot_oud_protocol.h>
#include <simpleiot/siot_s_protocol.h>
#include <simpleiot/siot_gd_protocol.h>
#include <simpleiot/siot_m_protocol.h>
//#include "saccp_protocol_client_side.h"
//#include "test_generator.h"
#include <stdio.h>
//#include "sa_config.h"
#include "hal_commstack_persistent_storage.h"

#include <stdlib.h>     /* atoi */

//DECLARE_AES_ENCRYPTION_KEY

#define MAX_INSTANCES_SUPPORTED 10
typedef struct _DEVICE_CONTEXT
{
	uint16_t device_id;
	SASP_DATA sasp_data;
	uint8_t AES_ENCRYPTION_KEY[16];
	SAGDP_DATA sagdp_context_app;
	SAGDP_DATA sagdp_context_ctr;
} DEVICE_CONTEXT;

DEVICE_CONTEXT devices[ MAX_INSTANCES_SUPPORTED ];

void FAKE_INITIALIZE_DEVICES() // NOTE: here we do a quick jump ove pairing (or alike), and loading respective values from a DB; eventualy this code will be completely replaced by a valid one.
{
	uint16_t i, j;
	uint8_t base_key[16] = { 	0x00, 0x01,	0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, };
	for ( i=0; i<MAX_INSTANCES_SUPPORTED; i++ )
	{
		devices[i].device_id = i + 1;
		for ( j=0; j<16; j++ )
			devices[i].AES_ENCRYPTION_KEY[j] = base_key[j] + ( (i+1) << 4 );
	}
}


int main_loop()
{
	uint8_t pid[ SASP_NONCE_SIZE ];
	sa_uint48_t nonce;

#ifdef ENABLE_COUNTER_SYSTEM
	INIT_COUNTER_SYSTEM
#endif // ENABLE_COUNTER_SYSTEM


	ZEPTO_DEBUG_PRINTF_1("starting CLIENT's COMMM STACK...\n");
	ZEPTO_DEBUG_PRINTF_1("================================\n\n");

	// TODO: actual key loading, etc
//	uint8_t AES_ENCRYPTION_KEY[16];
//	ZEPTO_MEMCPY( AES_ENCRYPTION_KEY, "16-byte fake key", 16 );
//	memset( AES_ENCRYPTION_KEY, 0xab, 16 );

//	timeout_action tact;
//	tact.action = 0;
	sa_time_val currt;
	waiting_for wait_for;
	ZEPTO_MEMSET( &wait_for, 0, sizeof( waiting_for ) );
	wait_for.wait_packet = 1;
	TIME_MILLISECONDS16_TO_TIMEVAL( 1000, wait_for.wait_time ) //+++TODO: actual processing throughout the code

	uint8_t timer_val = 0x1;
	// TODO: revise time/timer management

	uint8_t ret_code;

	uint16_t dev_in_use;

	// do necessary initialization
	for ( dev_in_use=0; dev_in_use<MAX_INSTANCES_SUPPORTED; dev_in_use++ )
	{
		sasp_restore_from_backup( &(devices[dev_in_use].sasp_data), devices[dev_in_use].device_id );
		sagdp_init( &(devices[dev_in_use].sagdp_context_app) );
		sagdp_init( &(devices[dev_in_use].sagdp_context_ctr) );
	}
	siot_mesh_init_tables();

	// Try to initialize connection
	if ( !communication_initialize() )
		return -1;

	// prepare fake return address
	zepto_write_uint8( MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, 0 );
	zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR );
	zepto_write_uint8( MEMORY_HANDLE_MAIN_LOOP_2_SAOUDP_ADDR, 0 );
	zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_2_SAOUDP_ADDR );

	// MAIN LOOP
	for (;;)
	{
wait_for_comm_event:
		// [[QUICK CHECK FOR UNITS POTENTIALLY WAITING FOR TIMEOUT start]]
		// we ask each potential unit; if it reports activity, let it continue; otherwise, ask a next one
		// IMPORTANT: once an order of units is selected and tested, do not change it without extreme necessity

		sa_get_time( &currt );

//		for ( dev_in_use=0; dev_in_use<MAX_INSTANCES_SUPPORTED; dev_in_use++ )
		dev_in_use = 0;
		{
			// 1.1. test GDP-ctr
			ret_code = handler_sagdp_timer( &currt, &wait_for, NULL, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_CTR, MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_ctr) + dev_in_use );
			if ( ret_code == SAGDP_RET_NEED_NONCE )
			{
				// NOTE: here we assume that, if GDP has something to re-send by timer, working_handle is not in use (say, by CCP)
				ret_code = handler_sasp_get_packet_id( nonce, &(devices[dev_in_use].sasp_data), devices[dev_in_use].device_id );
				ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
				ret_code = handler_sagdp_timer( &currt, &wait_for, nonce, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_CTR, MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_ctr) );
				ZEPTO_DEBUG_ASSERT( ret_code != SAGDP_RET_NEED_NONCE && ret_code != SAGDP_RET_OK );
				zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
				zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR );
				goto saspsend;
			}

			// 1.2. test GDP-app
			ret_code = handler_sagdp_timer( &currt, &wait_for, NULL, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_APP, MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_app) );
			if ( ret_code == SAGDP_RET_NEED_NONCE )
			{
				// NOTE: here we assume that, if GDP has something to re-send by timer, working_handle is not in use (say, by CCP)
				ret_code = handler_sasp_get_packet_id( nonce, &(devices[dev_in_use].sasp_data), devices[dev_in_use].device_id );
				ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
				ret_code = handler_sagdp_timer( &currt, &wait_for, nonce, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_APP, MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_app) );
				ZEPTO_DEBUG_ASSERT( ret_code != SAGDP_RET_NEED_NONCE && ret_code != SAGDP_RET_OK );
				zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
				zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR );
				goto saspsend;
			}
		}

		// 2. MESH
		ret_code = handler_siot_mesh_timer( &currt, &wait_for, MEMORY_HANDLE_MAIN_LOOP_1 );
		switch ( ret_code )
		{
			case SIOT_MESH_RET_PASS_TO_CCP:
			{
				// quite dirty and temporary solution
				zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
				ZEPTO_DEBUG_PRINTF_1( "         ############  about to jump to sagdp with route update reply  ###########\n" );
//				goto sagdpsend;
				zepto_parser_free_memory( MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR );
				sa_get_time( &currt );
				ret_code = handler_sagdp_receive_hlp( &currt, &wait_for, NULL, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_CTR, MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_ctr) );
				if ( ret_code == SAGDP_RET_NEED_NONCE )
				{
					ret_code = handler_sasp_get_packet_id( nonce, &(devices[dev_in_use].sasp_data), devices[dev_in_use].device_id );
					ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
					sa_get_time( &(currt) );
					ret_code = handler_sagdp_receive_hlp( &currt, &wait_for, nonce, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_CTR, MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_ctr) );
					ZEPTO_DEBUG_ASSERT( ret_code != SAGDP_RET_NEED_NONCE );
				}
				zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
				ZEPTO_DEBUG_PRINTF_4( "SAGDP2: ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );
		
				switch ( ret_code )
				{
					case SAGDP_RET_SYS_CORRUPTED:
					{
						// TODO: think about proper error processsing
//						send_error_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1 );
						sagdp_init( &(devices[dev_in_use].sagdp_context_ctr) );
						ZEPTO_DEBUG_PRINTF_1( "Internal error. System is to be reinitialized\n" );
						goto wait_for_comm_event;
						break;
					}
					case SAGDP_RET_TO_LOWER_NEW:
					{
						// regular processing will be done below in the next block
						goto saspsend;
						break;
					}
					default:
					{
						// unexpected ret_code
						ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
						ZEPTO_DEBUG_ASSERT( 0 );
						break;
					}
				}
				break;
			}
		}

		// 3. (next candidate)

		// [[QUICK CHECK FOR UNITS POTENTIALLY WAITING FOR TIMEOUT end]]

		ret_code = wait_for_communication_event( &wait_for );
		SA_TIME_SET_INFINITE_TIME( wait_for.wait_time );
//		ZEPTO_DEBUG_PRINTF_4( "=============================================Msg wait event; ret = %d, rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );

		switch ( ret_code )
		{
			case COMMLAYER_RET_FAILED:
			{
				// fatal communication error
				return 0;
				break;
			}
			case COMMLAYER_RET_FROM_CENTRAL_UNIT:
			{
				// regular processing will be done below in the next block
				ret_code = try_get_message_within_master( MEMORY_HANDLE_MAIN_LOOP_1 );
				if ( ret_code == COMMLAYER_RET_FAILED )
					return 0;
				if ( ret_code == COMMLAYER_RET_OK_AS_CU )
				{
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					ZEPTO_DEBUG_PRINTF_3( "\'ret_code == COMMLAYER_RET_OK_AS_CU\': rq_size: %d, rsp_size: %d\n", ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );
					parser_obj po, po1;
					zepto_parser_init( &po, MEMORY_HANDLE_MAIN_LOOP_1 );
					dev_in_use = zepto_parse_encoded_uint16( &po );
					ZEPTO_DEBUG_ASSERT( dev_in_use > 0 );
					dev_in_use --;
					zepto_parser_init_by_parser( &po1, &po );
					zepto_parse_skip_block( &po1, zepto_parsing_remaining_bytes( &po ) );
					zepto_convert_part_of_request_to_response( MEMORY_HANDLE_MAIN_LOOP_1, &po, &po1 );

					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					goto client_received;
					break;
				}
				else if ( ret_code == COMMLAYER_RET_OK_AS_SLAVE )
				{
					// regular processing will be done below in the next block
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					ZEPTO_DEBUG_PRINTF_3( "\'ret_code == COMMLAYER_RET_OK_AS_SLAVE\': rq_size: %d, rsp_size: %d\n", ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );
					goto siotmp_rec;
					break;
				}
				else
				{
					ZEPTO_DEBUG_ASSERT( 0 );
				}
			}
			case COMMLAYER_RET_TIMEOUT:
			{
#if 0
				// ZEPTO_DEBUG_PRINTF_1( "no reply received; the last message (if any) will be resent by timer\n" );
				sa_get_time( &currt );
				gdp_context = SAGDP_CONTEXT_UNKNOWN;
				ret_code = handler_sagdp_timer( &gdp_context, &currt, &wait_for, NULL, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
				if ( ret_code == SAGDP_RET_OK )
				{
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					goto wait_for_comm_event;
				}
				else if ( ret_code == SAGDP_RET_NEED_NONCE )
				{
					ret_code = handler_sasp_get_packet_id( nonce, &(devices[dev_in_use].sasp_data), devices[dev_in_use].device_id );
					ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
					sa_get_time( &currt );
					ret_code = handler_sagdp_timer( &gdp_context, &currt, &wait_for, nonce, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR/*, &sagdp_data*/ );
					ZEPTO_DEBUG_ASSERT( ret_code == SAGDP_RET_TO_LOWER_REPEATED );
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					goto saspsend;
					break;
				}
				else
				{
					ZEPTO_DEBUG_PRINTF_2( "ret_code = %d\n", ret_code );
					ZEPTO_DEBUG_ASSERT( 0 );
				}
#endif // 0
					
				goto wait_for_comm_event;
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}


		ZEPTO_DEBUG_PRINTF_1("Message from server received\n");
		ZEPTO_DEBUG_PRINTF_4( "ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );


		// 2.0. Pass to siot/mesh
	siotmp_rec:
#if SIOT_MESH_IMPLEMENTATION_WORKS
		ret_code = handler_siot_mesh_receive_packet( MEMORY_HANDLE_MAIN_LOOP_1, &dev_in_use, 0 ); // TODO: add actual connection quality
		ZEPTO_DEBUG_ASSERT( dev_in_use > 0 );
		dev_in_use--;
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_6( "handler_siot_mesh_receive_packet(): ret: %d; rq_size: %d, rsp_size: %d, dev_in_use = %d, device_id = %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ), dev_in_use, devices[dev_in_use].device_id );

		switch ( ret_code )
		{
			case SIOT_MESH_RET_PASS_TO_PROCESS:
			{
				// regular processing will be done below in the next block
				break;
			}
			case SIOT_MESH_RET_PASS_TO_SEND:
			{
				goto hal_send;
				break;
			}
			case SIOT_MESH_RET_GARBAGE_RECEIVED:
			{
				goto wait_for_comm_event;
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}
#endif

		// 2.1. Pass to SAoUDP
//saoudp_in:
		ret_code = handler_saoudp_receive( MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );

		switch ( ret_code )
		{
			case SAOUDP_RET_OK:
			{
				// regular processing will be done below in the next block
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

		// 2.2. Pass to SASP
		ret_code = handler_sasp_receive( devices[dev_in_use].AES_ENCRYPTION_KEY, pid, MEMORY_HANDLE_MAIN_LOOP_1, &(devices[dev_in_use].sasp_data), devices[dev_in_use].device_id );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_6( "handler_sasp_receive(): ret: %d; rq_size: %d, rsp_size: %d, dev_in_use = %d, device_id = %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ), dev_in_use, devices[dev_in_use].device_id );

		switch ( ret_code )
		{
			case SASP_RET_IGNORE_PACKET_BROKEN:
			case SASP_RET_IGNORE_PACKET_LAST_REPEATED:
			case SASP_RET_IGNORE_PACKET_NONCE_LS_NOT_APPLIED:
			{
				ZEPTO_DEBUG_PRINTF_1( "SASP: ignoring packet\n" );
				goto wait_for_comm_event;
				break;
			}
			case SASP_RET_TO_LOWER_ERROR:
			{
				goto saoudp_send;
				break;
			}
			case SASP_RET_TO_HIGHER_NEW:
			{
				// regular processing will be done below in the next block
				break;
			}
			case SASP_RET_TO_HIGHER_LAST_SEND_FAILED:
			{
				bool use_ctr = true;
				ZEPTO_DEBUG_PRINTF_1( "NONCE_LAST_SENT has been reset; the last message (if any) will be resent\n" );
				sa_get_time( &(currt) );
				ret_code = handler_sagdp_receive_request_resend_lsp( &currt, &wait_for, NULL, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_CTR, MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_ctr) );
				if ( ret_code == SAGDP_RET_TO_LOWER_NONE )
				{
					use_ctr = false;
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR );
					ret_code = handler_sagdp_receive_request_resend_lsp( &currt, &wait_for, NULL, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_APP, MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_app) );
				}
				if ( ret_code == SAGDP_RET_TO_LOWER_NONE )
				{
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
					zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR );
					goto wait_for_comm_event;
					break;
				}
				if ( ret_code == SAGDP_RET_NEED_NONCE )
				{
					ret_code = handler_sasp_get_packet_id( nonce, &(devices[dev_in_use].sasp_data), devices[dev_in_use].device_id );
					ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
					sa_get_time( &(currt) );
					if ( use_ctr )
						ret_code = handler_sagdp_receive_request_resend_lsp( &currt, &wait_for, nonce, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_CTR, MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_ctr) );
					else
						ret_code = handler_sagdp_receive_request_resend_lsp( &currt, &wait_for, nonce, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_APP, MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_app) );
					ZEPTO_DEBUG_ASSERT( ret_code != SAGDP_RET_NEED_NONCE && ret_code != SAGDP_RET_TO_LOWER_NONE );
				}
				zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
				zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR );
				goto saspsend;
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

		// 3. pass to SAGDP a new packet
		sa_get_time( &currt );
		bool for_ctr = handler_sagdp_is_up_packet_ctr( MEMORY_HANDLE_MAIN_LOOP_1 );

		if ( for_ctr )
		{
			ret_code = handler_sagdp_receive_up( &currt, &wait_for, NULL, pid, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_CTR, MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_ctr) );
			if ( ret_code == SAGDP_RET_NEED_NONCE )
			{
				ret_code = handler_sasp_get_packet_id( nonce, &(devices[dev_in_use].sasp_data), devices[dev_in_use].device_id );
				ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
				sa_get_time( &(currt) );
				ret_code = handler_sagdp_receive_up( &currt, &wait_for, nonce, pid, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_CTR, MEMORY_HANDLE_SAGDP_LSM_CTR_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_ctr) );
				ZEPTO_DEBUG_ASSERT( ret_code != SAGDP_RET_NEED_NONCE );
			}
			zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
			ZEPTO_DEBUG_PRINTF_4( "SAGDP1 (ctr): ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );

			switch ( ret_code )
			{
				case SAGDP_RET_SYS_CORRUPTED:
				{
					// TODO: think about error processing in case of control packets
//					send_error_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1 );
					sagdp_init( &(devices[dev_in_use].sagdp_context_ctr) );
					ZEPTO_DEBUG_PRINTF_1( "Internal error. System is to be reinitialized\n" );
					goto wait_for_comm_event;
					break;
				}
				case SAGDP_RET_TO_HIGHER:
				{
					// we add a quick jump here to intercept packets for Mesh Protocol itself
					// TODO: full CCP processing must be done here
					parser_obj po, po1;
					zepto_parser_init( &po, MEMORY_HANDLE_MAIN_LOOP_1 );

					uint8_t first_byte = zepto_parse_uint8( &po );
					uint16_t packet_head = zepto_parse_encoded_uint16( &po );
					uint8_t packet_type = packet_head & 0x7; // TODO: use bit field processing instead

					if ( packet_type == 0x5 /*SACCP_PHY_AND_ROUTING_DATA*/ )
					{
						uint8_t additional_bits = (packet_head >> 3) & 0x7; // "additional bits" passed alongside with PHY-AND-ROUTING-DATA-REQUEST-BODY
						ZEPTO_DEBUG_ASSERT( additional_bits == 0 ); // Route-Update-Request is always accompanied with SACCP "additional bits" equal to 0x0; bits [6..7] reserved (MUST be zeros)
						zepto_parser_init_by_parser( &po1, &po );
						zepto_parse_skip_block( &po1, zepto_parsing_remaining_bytes( &po ) );
						zepto_convert_part_of_request_to_response( MEMORY_HANDLE_MAIN_LOOP_1, &po, &po1 );
						zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
						uint16_t source_dev_id = 1; // TODO: here we know in context of which device we actually work; use actual data!!! 
				ZEPTO_DEBUG_PRINTF_1( "         ############  route update reply received  ###########\n" );
						handler_siot_mesh_process_route_update_response( source_dev_id, MEMORY_HANDLE_MAIN_LOOP_1 );
						goto wait_for_comm_event;
						break;
					}
					else
					{
						ZEPTO_DEBUG_ASSERT( 0 == "Unknown packet type" );
						goto wait_for_comm_event;
						break;
					}
				}
				case SAGDP_RET_TO_LOWER_REPEATED:
				{
					goto saspsend;
				}
				case SAGDP_RET_OK:
				{
					goto wait_for_comm_event;
				}
				default:
				{
					// unexpected ret_code
					ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
					ZEPTO_DEBUG_ASSERT( 0 );
					break;
				}
			}
		}

		// LIKELY BRANCH: PAACKET IS FOR APP

		ZEPTO_DEBUG_ASSERT( !for_ctr ); // we are not supposed to go through the above code
		ret_code = handler_sagdp_receive_up( &currt, &wait_for, NULL, pid, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_APP, MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_app) );
		if ( ret_code == SAGDP_RET_NEED_NONCE )
		{
			ret_code = handler_sasp_get_packet_id( nonce, &(devices[dev_in_use].sasp_data), devices[dev_in_use].device_id );
			ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
			sa_get_time( &(currt) );
			ret_code = handler_sagdp_receive_up( &currt, &wait_for, nonce, pid, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_APP, MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_app) );
			ZEPTO_DEBUG_ASSERT( ret_code != SAGDP_RET_NEED_NONCE );
		}
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_4( "SAGDP1: ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );

		switch ( ret_code )
		{
			case SAGDP_RET_OK:
			{
				ZEPTO_DEBUG_PRINTF_1( "master received unexpected or repeated packet. ignored\n" );
				goto wait_for_comm_event;
				break;
			}
			case SAGDP_RET_TO_HIGHER:
			{
				break;
			}
			case SAGDP_RET_SYS_CORRUPTED:
			{
				send_error_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1, devices[dev_in_use].device_id ); // TODO: what's about errors of Comm.Stack itself?
				sagdp_init( &(devices[dev_in_use].sagdp_context_app) );
				ZEPTO_DEBUG_PRINTF_1( "Internal error. System is to be reinitialized\n" );
				goto wait_for_comm_event;
				break;
			}
#if 0
			case SAGDP_RET_TO_HIGHER_ERROR:
			{
				sagdp_init( &sagdp_data );
				// TODO: reinit the rest of stack (where applicable)
				ret_code = send_to_central_unit_error( MEMORY_HANDLE_MAIN_LOOP_1 );
				//+++TODO: where to go?
				goto wait_for_comm_event;
				break;
			}
#endif // 0
			case SAGDP_RET_TO_LOWER_REPEATED:
			{
				goto saspsend;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}




#ifdef MASTER_ENABLE_ALT_TEST_MODE
		ret_code = send_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1 );
		goto wait_for_comm_event;
#else

		// 4. pass to SACCP a new packet
#if 0 // we cannot do any essential processing here in comm stack...
		ret_code = handler_saccp_receive( MEMORY_HANDLE_MAIN_LOOP_1/*, sasp_nonce_type chain_id*/ ); //master_process( &wait_to_continue_processing, MEMORY_HANDLE_MAIN_LOOP_1 );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_4( "SACCP1: ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );
		switch ( ret_code )
		{
			case SACCP_RET_PASS_TO_CENTRAL_UNIT:
			{
				ret_code = send_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1 );
				// TODO: check ret_code
				goto wait_for_comm_event;
				break;
			}
			case SACCP_RET_FAILED:
			{
				ZEPTO_DEBUG_PRINTF_1( "Failure in SACCP. handling is not implemented. Aborting\n" );
				return 0;
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

#else	// ...instead we just send whatever we have received  to the Central Unit.
		// Note: we may need to add some data (such as chain ID) or to somehow restructure the packet data;
		//       in this case this is a right place to do that

		ret_code = send_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1, devices[dev_in_use].device_id );
		// TODO: check ret_code
		goto wait_for_comm_event;

#endif // 0

#endif




		ZEPTO_DEBUG_PRINTF_1( "<NOT jumping to client_received (consequtive flow)>\n" );
client_received:
#if 0 // this functionality is trivial and will be done on a Central Unit side
		// 4. SACCP (prepare packet)
		ret_code = handler_saccp_prepare_to_send( MEMORY_HANDLE_MAIN_LOOP_1 );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		gdp_context = SAGDP_CONTEXT_APPLICATION; // TODO: context selection based on caller
		ZEPTO_DEBUG_PRINTF_4( "SACCP2: ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );
		// TODO: analyze and process ret_code
#endif

		// 5. SAGDP
		ZEPTO_DEBUG_PRINTF_5( "@client_received: rq_size: %d, rsp_size: %d, dev_in_use: %d, for-device: %d\n", ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ), dev_in_use, devices[dev_in_use].device_id );
		sa_get_time( &currt );
//		gdp_context = SAGDP_CONTEXT_APPLICATION; // TODO: context selection based on caller
		ret_code = handler_sagdp_receive_hlp( &currt, &wait_for, NULL, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_APP, MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_app) );
		if ( ret_code == SAGDP_RET_NEED_NONCE )
		{
			ret_code = handler_sasp_get_packet_id( nonce, &(devices[dev_in_use].sasp_data), devices[dev_in_use].device_id );
			ZEPTO_DEBUG_ASSERT( ret_code == SASP_RET_NONCE );
			sa_get_time( &(currt) );
			ret_code = handler_sagdp_receive_hlp( &currt, &wait_for, nonce, MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_1_SAOUDP_ADDR, MEMORY_HANDLE_SAGDP_LSM_APP, MEMORY_HANDLE_SAGDP_LSM_APP_SAOUDP_ADDR, &(devices[dev_in_use].sagdp_context_app) );
			ZEPTO_DEBUG_ASSERT( ret_code != SAGDP_RET_NEED_NONCE );
		}
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_4( "SAGDP2: ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );

		switch ( ret_code )
		{
			case SAGDP_RET_SYS_CORRUPTED:
			{
				send_error_to_central_unit( MEMORY_HANDLE_MAIN_LOOP_1, devices[dev_in_use].device_id ); // TODO: what's about errors of Comm.Stack itself?
				sagdp_init( &(devices[dev_in_use].sagdp_context_app) );
				ZEPTO_DEBUG_PRINTF_1( "Internal error. System is to be reinitialized\n" );
				goto wait_for_comm_event;
				break;
			}
			case SAGDP_RET_TO_LOWER_NEW:
			{
				// regular processing will be done below in the next block
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

		// SASP
saspsend:
		ret_code = handler_sasp_send( devices[dev_in_use].AES_ENCRYPTION_KEY, nonce, MEMORY_HANDLE_MAIN_LOOP_1, &(devices[dev_in_use].sasp_data) );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );
		ZEPTO_DEBUG_PRINTF_4( "SASP2: ret: %d; rq_size: %d, rsp_size: %d\n", ret_code, ugly_hook_get_request_size( MEMORY_HANDLE_MAIN_LOOP_1 ), ugly_hook_get_response_size( MEMORY_HANDLE_MAIN_LOOP_1 ) );

		switch ( ret_code )
		{
			case SASP_RET_TO_LOWER_REGULAR:
			{
				// regular processing will be done below in the next block
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

		// SAoUDP
saoudp_send:
		ret_code = handler_saoudp_send( MEMORY_HANDLE_MAIN_LOOP_1, MEMORY_HANDLE_MAIN_LOOP_2_SAOUDP_ADDR );
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );

		switch ( ret_code )
		{
			case SAOUDP_RET_OK:
			{
				// regular processing will be done below in the next block
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}

#if SIOT_MESH_IMPLEMENTATION_WORKS
		uint16_t link_id;
		uint16_t target_id = 1;
		ret_code = handler_siot_mesh_send_packet( target_id, MEMORY_HANDLE_MAIN_LOOP_1, &link_id ); // currently we know only about a single client with id=1
		zepto_response_to_request( MEMORY_HANDLE_MAIN_LOOP_1 );

		switch ( ret_code )
		{
			case SIOT_MESH_RET_OK:
			{
				// regular processing will be done below in the next block
				break;
			}
			default:
			{
				// unexpected ret_code
				ZEPTO_DEBUG_PRINTF_2( "Unexpected ret_code %d\n", ret_code );
				ZEPTO_DEBUG_ASSERT( 0 );
				break;
			}
		}
#endif

		// send packet
hal_send:
//		ZEPTO_DEBUG_ASSERT( link_id == 0 ); // TODO: link_id must be a part of send_packet() call; we are now just in the middle of development...
		ret_code = send_message( MEMORY_HANDLE_MAIN_LOOP_1 );
		zepto_parser_free_memory( MEMORY_HANDLE_MAIN_LOOP_1 );
		if (ret_code != COMMLAYER_RET_OK )
		{
			return -1;
		}

	}

	communication_terminate();

	return 0;
}

void set_port_from_command_line(int argc, char *argv[])
{
	uint8_t i;
	for ( i = 0; i<argc; i++ )
	{
		if ( ZEPTO_MEMCMP( argv[i], "--port=", 7 ) == 0 )
		{
			int port = atoi( argv[i]+7);
			ZEPTO_DEBUG_ASSERT( port >= 0 && port < 0x10000 );
			ZEPTO_DEBUG_PRINTF_2( "port to be actually used: %d\n", port );
			self_port_num_with_cl = port;
			return;
		}
	}
}

char* get_persistent_storage_path_from_command_line(int argc, char *argv[])
{
	uint8_t i;
	for ( i = 0; i<argc; i++ )
		if ( ZEPTO_MEMCMP( argv[i], "--psp=", 6 ) == 0 )
		{
			ZEPTO_DEBUG_PRINTF_2( "persistent storage is at: \"%s\"\n", argv[i]+6 );
			return argv[i]+6;
		}
	ZEPTO_DEBUG_PRINTF_1( "default persistent storage location will be used\n" );
	return NULL;
}

int main(int argc, char *argv[])
{
	FAKE_INITIALIZE_DEVICES();

	setbuf(stdout, NULL);

	set_port_from_command_line( argc, argv );

	zepto_mem_man_init_memory_management();

	// TODO: logic of accessing/intializing persistent storage must be totally revised toward more secure version
//	if (!init_eeprom_access())
//		return 0;
	uint8_t rid[DATA_REINCARNATION_ID_SIZE];
//	ZEPTO_MEMCPY( rid, devices[dev_in_use].AES_ENCRYPTION_KEY, DATA_REINCARNATION_ID_SIZE );
	char* persistent_storage_path = get_persistent_storage_path_from_command_line( argc, argv );
	uint8_t ret_code = hal_init_eeprom_access( persistent_storage_path );
	uint16_t i;
	switch ( ret_code )
	{
		case HAL_PS_INIT_FAILED:
		{
			ZEPTO_DEBUG_PRINTF_1( "init_eeprom_access() failed\n" );
			return 0;
		}
		case HAL_PS_INIT_OK:
		{
			ZEPTO_DEBUG_PRINTF_1( "hal_init_eeprom_access() passed\n" );
/*			if ( !eeprom_check_at_start() ) // corrupted data; so far, at least one of slots cannot be recovered
			{
				sasp_init_eeprom_data_at_lifestart();
			}*/
			ret_code = eeprom_check_reincarnation( rid );
			switch ( ret_code )
			{
				case EEPROM_RET_REINCARNATION_ID_OLD:
				{
					for ( i=0;i<MAX_INSTANCES_SUPPORTED; i++ )
						sasp_init_eeprom_data_at_lifestart( &(devices[i].sasp_data), devices[i].device_id );
					eeprom_update_reincarnation_if_necessary( rid );
					break;
				}
				case EEPROM_RET_REINCARNATION_ID_OK_ONE_OK:
				{
					if ( !eeprom_check_at_start() ) // corrupted data; so far, at least one of slots cannot be recovered
					{
						for ( i=0;i<MAX_INSTANCES_SUPPORTED; i++ )
							sasp_init_eeprom_data_at_lifestart( &(devices[i].sasp_data), devices[i].device_id );
					}
					eeprom_update_reincarnation_if_necessary( rid );
					break;
				}
				case EEPROM_RET_REINCARNATION_ID_OK_BOTH_OK:
				{
					if ( !eeprom_check_at_start() ) // corrupted data; so far, at least one of slots cannot be recovered
					{
						for ( i=0;i<MAX_INSTANCES_SUPPORTED; i++ )
							sasp_init_eeprom_data_at_lifestart( &(devices[i].sasp_data), devices[i].device_id );
					}
					break;
				}
				default:
				{
					ZEPTO_DEBUG_ASSERT( 0 == "Unexpected ret code" );
					break;
				}
			}
			break;
		}
		case HAL_PS_INIT_OK_NEEDS_INITIALIZATION:
		{
			for ( i=0;i<MAX_INSTANCES_SUPPORTED; i++ )
				sasp_init_eeprom_data_at_lifestart( &(devices[i].sasp_data), devices[i].device_id );
			eeprom_update_reincarnation_if_necessary( rid );
			ZEPTO_DEBUG_PRINTF_1( "initializing eeprom done\n" );
			break;
		}
	}

	return main_loop();
//	return 0;
}
