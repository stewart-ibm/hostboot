/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/import/chips/p9/procedures/hwp/io/p9_io_erepairAccessorHwpFuncs.H $ */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2015,2017                        */
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
/// @file p9_io_erepairAccessorHwpFuncs.H
/// @brief FW Team utility functions that access fabric and memory eRepair data.
///
//----------------------------------------------------------------------------


#ifndef P9_IO_EREPAIRACCESSORHWPFUNCS_H_
#define P9_IO_EREPAIRACCESSORHWPFUNCS_H_

#include <fapi2.H>
#include <algorithm>

const uint8_t EREPAIR_MAX_CENTAUR_PER_MCS = 1;

template<fapi2::TargetType K>
using getLanes_t = fapi2::ReturnCode (*)(
                       const fapi2::Target < K >& i_tgtHandle,
                       const uint8_t i_clkGroup,
                       std::vector<uint8_t>& o_txFailLanes,
                       std::vector<uint8_t>& o_rxFailLanes);

template<fapi2::TargetType K>
using setLanes_t = fapi2::ReturnCode (*)(
                       const fapi2::Target < K >& i_tgtHandle,
                       const uint8_t i_clkGroup,
                       const std::vector<uint8_t>& o_txFailLanes,
                       const std::vector<uint8_t>& o_rxFailLanes);


/**
 * @brief FW Team Utility function that gets eRepair data from the VPD
 *        This function gets the eRepair data from both the Field VPD
 *        and the Manufacturing VPD.
 *
 * @param[in]  i_endp_target Reference to X-Bus or O-Bus or MCS or memBuf Target
 * @param[in]  i_clkGroup    Specifies clock group 0:[XOA, X1A,..] 1:[X0B, X1B,..]
 * @param[out] o_txFailLanes Reference to a Vector that will contain the fail
 *                           lanes read from the VPD for Drive side
 * @param[out] o_rxFailLanes Reference to a Vector that will contain the fail
 *                           lanes read from the VPD for Receive side
 * @return ReturnCode
 *
 */
template<fapi2::TargetType K>
fapi2::ReturnCode erepairGetFailedLanes(
    const fapi2::Target < K >& i_endp_target,
    const uint8_t i_clkGroup,
    std::vector<uint8_t>& o_txFailLanes,
    std::vector<uint8_t>& o_rxFailLanes);

/**
 * @brief FW Team Utility function that gets eRepair data
 *
 * This is a wrapper function for the Accessor HWP which reads failed lane
 * numbers from the Field VPD
 *
 * @param[in]  i_endp_target Reference to X-Bus or O-Bus or MCS or memBuf Target
 * @param[in]  i_clkGroup    Specifies clock group 0:[XOA, X1A,..] 1:[X0B, X1B,..]
 * @param[out] o_txFailLanes Reference to a Vector that will contain the fail
 *                           lanes read from the Field VPD for Drive side
 * @param[out] o_rxFailLanes Reference to a Vector that will contain the fail
 *                           lanes read from the Field VPD for Receive side
 * @return ReturnCode
 *
 */
template<fapi2::TargetType K>
fapi2::ReturnCode erepairGetFieldFailedLanes(
    const fapi2::Target < K >&          i_endp_target,
    const uint8_t                     i_clkGroup,
    std::vector<uint8_t>& o_txFailLanes,
    std::vector<uint8_t>& o_rxFailLanes);

/**
 * @brief FW Team Utility function that gets eRepair data
 *
 * This is a wrapper function for the Accessor HWP which reads failed lane
 * numbers from the Manufacturing VPD
 *
 * @param[in]  i_endp_target Reference to X-Bus or O-Bus or MCS or memBuf Target
 * @param[in]  i_clkGroup    Specifies clock group 0:[XOA, X1A,..] 1:[X0B, X1B,..]
 * @param[out] o_txFailLanes Reference to a Vector that will contain the fail
 *                           lanes read from the Mnfg VPD for Drive side
 * @param[out] o_rxFailLanes Reference to a Vector that will contain the fail
 *                           lanes read from the Mnfg VPD for Receive side
 * @return ReturnCode
 *
 */
template<fapi2::TargetType K>
fapi2::ReturnCode erepairGetMnfgFailedLanes(
    const fapi2::Target < K >&          i_endp_target,
    const uint8_t                     i_clkGroup,
    std::vector<uint8_t>& o_txFailLanes,
    std::vector<uint8_t>& o_rxFailLanes);

/**
 * @brief FW Team Utility function that sets eRepair data in the VPD.
 *        This functions sets the eRepair data to either the Field VPD
 *        or the Manufacturing VPD depending on whether the IPL was done
 *        in normal mode or Manufacturing mode.
 *        It writes eRepair data to the VPD of both the endpoint targets
 *        passed as arguments.
 *
 * @param[in]  i_txEndp_target   Reference to X-Bus or O-Bus or MCS or memBuf
 *                               Target. This is the peer target of
 *                               i_rxEndp_target
 * @param[in]  i_rxEndp_target   Reference to X-Bus or O-Bus or MCS or memBuf
 *                               Target. This is the target on which the
 *                               badlanes were found
 * @param[in]  i_rxFailLanes     Vector that will contain the fail lanes
 *                               to be written to VPD for Receive side
 * @param[in]  i_clkGroup        Specifies clock group 0:[XOA, X1A,..] 1:[X0B, X1B,..]
 * @param[out] o_thresholdExceed If TRUE, indicates that the eRepair threshold
 *                               has exceeded, FALSE otherwise.
 *
 * @return ReturnCode
 */
template<fapi2::TargetType K, fapi2::TargetType J>
fapi2::ReturnCode erepairSetFailedLanes(
    const fapi2::Target < K >&          i_txEndp_target,
    const fapi2::Target < J >&          i_rxEndp_target,
    const uint8_t                     i_clkGroup,
    const std::vector<uint8_t>& i_rxFailLanes,
    bool&                       o_thresholdExceed);

/**
 * @brief FW Team Utility function that sets eRepair data in Field VPD
 *
 * This is a wrapper function for the Accessor HWP which writes failed lane
 * numbers to the Field VPD
 *
 * @param[in]  i_endp_target Reference to X-Bus or O-Bus or MCS or memBuf Target
 * @param[in]  i_clkGroup    Specifies clock group 0:[XOA, X1A,..] 1:[X0B, X1B,..]
 * @param[in]  i_txFailLanes Vector that will contain the fail lane
 *                           to be written to Field VPD for Drive side
 * @param[in]  i_rxFailLanes Vector that will contain the fail lanes
 *                           to be written to Field VPD for Receive side
 *
 * @return ReturnCode
 */
template<fapi2::TargetType K>
fapi2::ReturnCode erepairSetFieldFailedLanes(
    const fapi2::Target < K >&          i_endp_target,
    const uint8_t                     i_clkGroup,
    const std::vector<uint8_t>& i_txFailLanes,
    const std::vector<uint8_t>& i_rxFailLanes);

/**
 * @brief FW Team Utility function that sets eRepair data in Manufacturing VPD
 *
 * This is a wrapper function for the Accessor HWP which writes failed lane
 * numbers to the Manufacturing VPD
 *
 * @param[in]  i_endp_target Reference to X-Bus or O-Bus or MCS or memBuf Target
 * @param[in]  i_clkGroup    Specifies clock group 0:[XOA, X1A,..] 1:[X0B, X1B,..]
 * @param[in]  i_txFailLanes Vector that will contain the fail lane
 *                           to be written to Mnfg VPD for Drive side
 * @param[in]  i_rxFailLanes Vector that will contain the fail lanes
 *                           to be written to Mnfg VPD for Receive side
 *
 * @return ReturnCode
 */
template<fapi2::TargetType K>
fapi2::ReturnCode erepairSetMnfgFailedLanes(
    const fapi2::Target < K >&          i_endp_target,
    const uint8_t                     i_clkGroup,
    const std::vector<uint8_t>& i_txFailLanes,
    const std::vector<uint8_t>& i_rxFailLanes);

/**
 * @brief Function which retrieves the lanes that need to be restored for the
 *        given end point targets
 *
 * This function is called by the iStep dispatcher during the Restore Repair
 * iStep for Fabric buses and DMI buses. The caller need to make sure that the
 * first and fourth arguments are the endpoint targets of a Fabric bus or
 * DMI bus.
 * It calls the wrapper functions of Accessor HWP to read the fail lane data
 * recorded in the VPD on both the ends and verifies that there are matching
 * records on both the ends. If matching fail lanes are not found, the
 * corresponding fail lane data is invalidated using the wrapper Accessor HWP
 * that writes data to the VPD.
 *
 * @param [in]   i_endp1_target      Reference to X-Bus or O-Bus or MCS Target
 * @param [in]   i_endp2_target      Reference to X-Bus or O-Bus or MCS Target
 * @param[in]    i_clkGroup          Specifies clock group 0:[XOA, X1A,..] 1:[X0B, X1B,..]
 * @param [out]  o_endp1_txFaillanes Reference to vector that will have the
 *                                   fail lane numbers that need to be restored
 *                                   for the Tx side of the target passed
 *                                   as first param
 * @param [out]  o_endp1_rxFaillanes Reference to vector that will have the
 *                                   fail lane numbers that need to be restored
 *                                   for the Rx side of the target passed
 *                                   as first param
 * @param [out]  o_endp2_txFaillanes Reference to vector that will have the
 *                                   fail lane numbers that need to be restored
 *                                   for the Tx side of the target passed
 *                                   as fourth param
 * @param [out]  o_endp2_rxFaillanes Reference to vector that will have the
 *                                   fail lane numbers that need to be restored
 *                                   for the Rx side of the target passed
 *                                   as fourth param
 *
 * @return ReturnCode
 *
 */
template<fapi2::TargetType K, fapi2::TargetType J>
fapi2::ReturnCode erepairGetRestoreLanes(
    const fapi2::Target < K >&          i_endp1_target,
    const fapi2::Target < J >&          i_endp2_target,
    const uint8_t                     i_clkGroup,
    std::vector<uint8_t>& o_endp1_txFaillanes,
    std::vector<uint8_t>& o_endp1_rxFaillanes,
    std::vector<uint8_t>& o_endp2_txFaillanes,
    std::vector<uint8_t>& o_endp2_rxFaillanes);

#endif
