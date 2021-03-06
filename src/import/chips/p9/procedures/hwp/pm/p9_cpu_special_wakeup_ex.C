/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/import/chips/p9/procedures/hwp/pm/p9_cpu_special_wakeup_ex.C $ */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2016,2018                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */

///
///  @file          :    p9_cpu_special_wakeup_ex.C
///  @brief         :    HWP to perform special wakeup of an EX "chiplet"

// *HWP HW Owner    :    Greg Still <stillgs@us.ibm.com>
// *HWP FW Owner    :    Prem S Jha <premjha2@in.ibm.com>
// *HWP Team        :    PM
// *HWP Level       :    3
// *HWP Consumed by :    OCC:FSP:HOST:CRO

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------
#include <p9_cpu_special_wakeup.H>
#include <p9_cpu_special_wakeup_lib.H>
#include <p9_ppe_defs.H>
#include <p9_ppe_utils.H>

fapi2::ReturnCode collectExTimeoutFailInfo( const fapi2::Target < fapi2::TARGET_TYPE_EX>& i_target,
        ProcessingValues_t i_processing_info );
/// ----------------------------------------------------------------------------
/// @brief      Sets a normal eq chiplet into special wakeup state.
/// @param[in]  i_target     eq target
/// @param[in]  i_operation  Special Wakeup Operation i.e. assert or deassert
/// @param[in]  i_entity     entity to be considered for special wakeup.
/// @return     fapi2 return code.
///
fapi2::ReturnCode p9_cpu_special_wakeup_ex(
    const fapi2::Target < fapi2::TARGET_TYPE_EX>& i_target,
    const p9specialWakeup::PROC_SPCWKUP_OPS i_operation,
    const p9specialWakeup::PROC_SPCWKUP_ENTITY i_entity )
{
    FAPI_INF(">> p9_cpu_special_wakeup_ex");

    fapi2::ReturnCode l_rc;
    ProcessingValues_t l_processing_info;
    uint8_t l_spWakeUpInProg = 0;
    auto l_eqTarget = i_target.getParent<fapi2::TARGET_TYPE_EQ>();

    FAPI_ATTR_GET( fapi2::ATTR_EX_INSIDE_SPECIAL_WAKEUP,
                   i_target,
                   l_spWakeUpInProg );

    //A special wakeup is already in progress. In all likelyhood, a special
    // wakeup has timed out and we are in FFDC collection path. During this
    // FFDC collection, we SCOMed a register which itself needs a special
    // wakeup.

    if( l_spWakeUpInProg )
    {
        FAPI_INF("exiting ex recurssion");
        return fapi2::FAPI2_RC_SUCCESS;
    }

    p9specialWakeup::blockWakeupRecurssion( l_eqTarget, p9specialWakeup::BLOCK );

    l_rc = _special_wakeup( i_target,
                            i_operation,
                            i_entity,
                            l_processing_info );

    if ( l_rc == (uint32_t)fapi2::RC_INTERNAL_SPCWKUP_TIMEOUT )
    {
        collectExTimeoutFailInfo( i_target, l_processing_info );
    }

    p9specialWakeup::blockWakeupRecurssion( l_eqTarget, p9specialWakeup::UNBLOCK );

    FAPI_INF("<< p9_cpu_special_wakeup_ex" );
    return fapi2::current_err;
}

/// ----------------------------------------------------------------------------

///
/// @brief      Collect FFDC for EQ Special Wakeup timeout
/// @param[in]  i_target     ex target
/// @param[in]  i_operation  info pertaining to special wakeup
/// @return     fapi2 return code.
///
fapi2::ReturnCode collectExTimeoutFailInfo( const fapi2::Target < fapi2::TARGET_TYPE_EX>& i_target,
        ProcessingValues_t i_processing_info )
{

    FAPI_INF(">> collectExTimeoutFailInfo" );

    fapi2::buffer<uint64_t> l_CPMMR[CORES_PER_EX];
    fapi2::buffer<uint64_t> l_GPMMR[CORES_PER_EX];
    fapi2::buffer<uint64_t> l_spWakeupRegVal[CORES_PER_EX];
    fapi2::buffer<uint64_t> l_histRegVal[CORES_PER_EX];
    fapi2::buffer<uint64_t> l_netCtrlVal[CORES_PER_EX];
    uint32_t l_coreId = 0;

    auto l_core_vector =
        i_target.getChildren<fapi2::TARGET_TYPE_CORE>( fapi2::TARGET_STATE_PRESENT );

    for( uint8_t i = 0; i < CORES_PER_EX; i++ )
    {
        l_CPMMR[i].insert( INIT_REG_PATT, 0, 64 );
        l_GPMMR[i].insert( INIT_REG_PATT, 0, 64 );
        l_spWakeupRegVal[i].insert( INIT_REG_PATT, 0, 64 );
        l_histRegVal[i].insert( INIT_REG_PATT, 0, 64 );
    }

    for ( auto it : l_core_vector )
    {
        if( it.isFunctional() )
        {
            fapi2::getScom( it, C_CPPM_CPMMR, l_CPMMR[l_coreId] );
            fapi2::getScom( it, C_PPM_GPMMR_SCOM, l_GPMMR[l_coreId] );
            fapi2::getScom( it, i_processing_info.spwkup_address[l_coreId], l_spWakeupRegVal[l_coreId] );
            fapi2::getScom( it, i_processing_info.history_address[l_coreId], l_histRegVal[l_coreId] );
            fapi2::getScom( it, i_processing_info.netctrl_address[l_coreId], l_netCtrlVal[l_coreId] );
        }

        l_coreId++;
    }

    uint8_t l_exPos = 0;
    FAPI_ATTR_GET( fapi2::ATTR_CHIP_UNIT_POS, i_target, l_exPos );

    // Collect CME, SGPE and PGPE FFDC, currently we only collect XIRs
    std::vector<uint64_t> l_ppeBaseAddresses;
    l_ppeBaseAddresses.push_back (getCmeBaseAddress (l_exPos));
    l_ppeBaseAddresses.push_back (SGPE_BASE_ADDRESS);
    l_ppeBaseAddresses.push_back (PGPE_BASE_ADDRESS);

    //From this point onwards, any usage of FAPI TRY in physical or
    //logical path can be a serious problem. Hence, should not be used.

    FAPI_ASSERT( false ,
                 fapi2::SPCWKUP_EX_TIMEOUT().
                 set_POLLCOUNT( i_processing_info.poll_count ).
                 set_C0_NETCTRL( l_netCtrlVal[0] ).
                 set_C1_NETCTRL( l_netCtrlVal[1] ).
                 set_C0_SP_WKUP_REG_VALUE( l_spWakeupRegVal[0] ).
                 set_C1_SP_WKUP_REG_VALUE( l_spWakeupRegVal[1] ).
                 set_C0_HISTORY_VALUE( l_histRegVal[0] ).
                 set_C1_HISTORY_VALUE( l_histRegVal[1] ).
                 set_ENTITY( i_processing_info.entity ).
                 set_C0_CPMMR( l_CPMMR[0] ).
                 set_C1_CPMMR( l_CPMMR[1] ).
                 set_C0_GPMMR( l_GPMMR[0] ).
                 set_C1_GPMMR( l_GPMMR[1] ).
                 set_EQ_TARGET( i_target.getParent<fapi2::TARGET_TYPE_EQ>() ).
                 set_EX_TARGET( i_target ).
                 set_PROC_CHIP_TARGET( i_processing_info.procTgt ).
                 set_PPE_BASE_ADDRESSES( l_ppeBaseAddresses ).
                 set_PPE_STATE_MODE( XIRS ),
                 "Timed Out In Setting The EX Special Wakeup" );

fapi_try_exit:
    FAPI_INF("<< collectExTimeoutFailInfo" );
    return fapi2::current_err;
}

/// ----------------------------------------------------------------------------

/// @param[in]  i_chipletTarget     ex target
/// @param[in]  i_processing_info   struct storing processing info
/// @param[in]  i_msgId             Id pertaining to debug message string.
/// @return     fapi2 return code.
fapi2::ReturnCode spwkup_deassert(  const fapi2::Target<fapi2::TARGET_TYPE_EX>& i_chipletTarget,
                                    const ProcessingValues_t i_processing_info,
                                    p9specialWakeup::SpecialWakeUpMsg i_msgId )
{
    FAPI_INF("> spwkup_deassert core EX" );

    uint64_t l_address;

    for (uint32_t i = 0; i < i_processing_info.num_addresses; ++i)
    {
        l_address = i_processing_info.spwkup_address[i];
        FAPI_TRY(_spwkup_deassert(i_chipletTarget, l_address, i_msgId));
    }

fapi_try_exit:
    FAPI_INF("< spwkup_deassert EX" );
    return fapi2::current_err;
}

/// ----------------------------------------------------------------------------

/// @param[in]  i_chipletTarget     ex target
/// @param[in]  i_processing_info   struct storing processing info
/// @param[in]  i_entity            .
/// @return     fapi2 return code.
template<>
fapi2::ReturnCode set_addresses(const fapi2::Target<fapi2::TARGET_TYPE_EX>& i_target,
                                ProcessingValues_t& i_structure,
                                const uint32_t i_entity )
{
    FAPI_INF("> set_addresses for EX");

    FAPI_DBG("i_processing: setaddr start"
             "entity = %d  "
             "b_xstop_flag = %d "
             "b_ignore_xstop_flag = %d "
             "b_wakeup_on_entry_flag = %d "
             "b_ex_flag = %d \n",
             i_structure.entity,
             i_structure.b_xstop_flag,
             i_structure.b_ignore_xstop_flag,
             i_structure.b_wakeup_on_entry_flag,
             i_structure.b_ex_flag);

    uint8_t l_ex_num = 0;
    uint8_t l_core_num = 0;
    uint32_t i = 0;

    // Determine the good cores in the EX
    auto l_core_functional_vector =
        i_target.getChildren<fapi2::TARGET_TYPE_CORE>(fapi2::TARGET_STATE_FUNCTIONAL);

    FAPI_ASSERT(l_core_functional_vector.size() > 0,
                fapi2::SPCWKUP_NOEXCORES(),
                "No good cores to special wake-up in targeted EX");

    for (auto it : l_core_functional_vector)
    {
        FAPI_TRY(FAPI_ATTR_GET( fapi2::ATTR_CHIP_UNIT_POS, it, l_core_num),
                 "fapiGetAttribute of ATTR_CHIP_UNIT_POS");

        FAPI_DBG("EX %d being procesed. Setting up for Core %d with index %d",
                 l_ex_num, l_core_num, i);

        i_structure.spwkup_address[i]  = SPCWKUP_ADDR[i_entity][p9specialWakeup::SPW_CORE]
                                         + 0x01000000 * l_core_num;
        i_structure.history_address[i] = SPCWKUP_HIST_ADDR[i_entity][p9specialWakeup::SPW_CORE]
                                         + 0x01000000 * l_core_num;
        i_structure.netctrl_address[i] = SPCWKUP_NETCTRL0_ADDR[p9specialWakeup::SPW_CORE]
                                         + 0x01000000 * l_core_num;
        i_structure.gpmmr_address[i] = SPCWKUP_GPMMR_ADDR[p9specialWakeup::SPW_CORE]
                                       + 0x01000000 * l_core_num;

        FAPI_DBG("i_structure.spwkup_address[%d]  = 0x%08llX \n "
                 "i_structure.history_address[%d] = 0x%08llX \n "
                 "i_structure.netctrl_address[%d] = 0x%08llX \n "
                 "i_structure.gpmmr_addresss[%d]  = 0x%08llX \n " ,
                 i, i_structure.spwkup_address[i],
                 i, i_structure.history_address[i],
                 i, i_structure.netctrl_address[i],
                 i, i_structure.gpmmr_address[i]);
        ++i;
    }

    i_structure.num_addresses = i;


fapi_try_exit:
    FAPI_INF("< set_addresses for EX");
    return fapi2::current_err;
}
