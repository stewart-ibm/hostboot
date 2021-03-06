/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/include/usr/hwas/common/hwas.H $                          */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2012,2017                        */
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
#ifndef  __HWAS_HWAS_H
#define __HWAS_HWAS_H
/**
 *  @file hwas.H
 *
 *  HardWare Availability Service prototypes.
 *  In trying to keep with C++ tradition, doxygen documentation for functions
 *  are here in the .H file.
 *
 *  All of the following routines are "named isteps" - they are invoked as
 *  tasks by the @ref IStepDispatcher.
 *
 */


/******************************************************************************/
// Includes
/******************************************************************************/
#include <stdint.h>
#include <targeting/common/commontargeting.H>
#include <hwas/common/deconfigGard.H>
#include <hwas/common/hwasError.H>

namespace HWAS
{

/**
 * @brief   initHardware   Common HWAS function to setup the hardware
 *
 *  It will call into the hwas platform-specific platInitHardware()
 *  function to init the FSI hardware.
 *
 * @param  none
 *
 * @return errlHndl_t       valid errlHndl_t handle if there was an error
 *                          NULL if no errors;
 */
errlHndl_t initHardware();

/**
 * @brief   discoverTagets   Common HWAS function to build targeting
 *
 *  This routine will walk through all the targets and initialize HWAS STATE
 *  to a known default value (powered off, etc.)
 *
 *  Then call into the hwas platform-specific platPresenceDetect() function
 *  to read the hardware information, and apply it to the target states,
 *  and call into the hwas platform-specific functions:
 *      platReadIDEC() to get and set the ChipID and EC values.
 *      platReadPartialGood() to get and set the partial good vector.
 *
 * @param  none
 *
 * @return errlHndl_t       valid errlHndl_t handle if there was an error
 *                          NULL if no errors;
 */
errlHndl_t discoverTargets();

/**
 * @brief   restrictECunits   Internal HWAS function to restrict the ECs
 *
 *  This routine will walk through the procs in the list, and turn EC
 *  units to not functional if the number of units is larger than the max
 *  given. This is used for PR keyword and Field Core Override (FCO)
 *  processing.
 *
 * @param[in] i_procRestrict   vector of procRestrict entries
 * @param[in] i_present    boolean for 'present' HWAS value for restricted
 *                          EC units
 * @param[in] i_deconfigReason   DECONFIGURED_BY_ enum or 0
 *
 * @return errlHndl_t       valid errlHndl_t handle if there was an error
 *                          NULL if no errors;
 */

// Maximum number of EXs per proc
#define NUM_EX_PER_EQ     2
#define NUM_EQ_PER_CHIP   6
#define NUM_EX_PER_CHIP   (NUM_EX_PER_EQ * NUM_EQ_PER_CHIP)

// structure used to store proc information for EC restrict processing
typedef struct {
    TARGETING::TargetHandle_t target;      // proc target
    uint32_t group;             // uniq id for this group - ie: FRUID, node
    uint32_t maxECs;            // max EC units for this group
    uint8_t procs;              // number of procs in the group
} procRestrict_t;

errlHndl_t restrictECunits(
    std::vector <procRestrict_t> &i_procRestrict,
    const bool i_present,
    const uint32_t i_deconfigReason);

/**
 * @brief Verifies that the system has enough hardware to proceed through
 *  the IPL.
 *  This function contains checks that are COMMON between HWSV and HB.
 *  platform-specific checks will be called by platCheckMinimimHardware(),
 *  see hwasCommon.H .
 *  If it cannot find minimum hardware, an error will be created and returned.
 *  Error logs will also be created for each hardware module that is not
 *  running.
 *
 * @param[in] i_nodeOrSys Level of HW check node or system
 * @param[out] o_bootable Indicate whether the system is
 *             is bootable with current configuration.
 *             if o_bootable is not NULL an error for
 *             system unavailability will not be logged
 *
 * @return error log handle
 */
errlHndl_t checkMinimumHardware(
    const TARGETING::ConstTargetHandle_t i_nodeOrSys = NULL,
    bool *o_bootable = NULL);

/**
*  @brief  Loop through processors, make sure all have the same EC level
*          create an error log for any slave processor that does not match
*          the master's EC level
*
*
*  @return  errlHndl_t   Error returned will be a summary of all errors that
*                        occurred during the procedure, all PLIDs should match
*/
errlHndl_t validateProcessorEcLevels();

/**
 * @brief  Struct representing a particular target.  Used by
 * invokePresentByAssoc to populate a vector of TargetInfo's for subsequent
 * use by deconfigPresentByAssoc
 */
struct TargetInfo
{
    TARGETING::ATTR_AFFINITY_PATH_type affinityPath;
    TARGETING::Target * pThisTarget;
    TARGETING::ATTR_TYPE_type type;
    HWAS::DeconfigGard::DeconfiguredByReason reason;
};

// Structure populated in invokePresentByAssoc() for use in presentByAssoc()
typedef std::vector<TargetInfo> TargetInfoVector;

/**
 * @brief Invokes presentByAssoc
 *
 * Called by discoverTargets(). This function queries the system and populates
 * a vector of structs representing functional MCS, MEMBUFS, MBAs, MCAs,DIMMS.
 * This vector is then passed to presentByAssoc() which systematically adds
 * targets to another vector to be deconfigured based on their related targets
 * Upon completion of presentByAssoc(), this function iterates
 * through the returned vector and deconfigures any targets marked
 * for deconfiguration.
 */
void invokePresentByAssoc();

/**
 * @brief Algorithm to validate the memory target structure.
 *
 * For non direct memory, check if a MCS has a MEMBUF, a MEMBUF has a MBA,
 * and the MBA has a DIMM.
 *
 * For direct memory, check if a MCS has a MCA, and a MCA has a DIMM.
 *
 * The vector o_funcTargets is sorted by affinity path to allow a single pass
 * with some backtracking to check every scenario more efficiently.
 * These checks are needed because of scenarios where targets are non-present
 * and their related targets are not marked as deconfigured
 *
 * @param[in/out] io_funcTargets    A vector of functional memory targets
 * @param[out]    o_targToDeconfig  A vector of targets to deconfigure, done
 *                                  this way to allow unit tests
 */
void presentByAssoc(TargetInfoVector& io_funcTargets,
                    TargetInfoVector& o_targToDeconfig);

/**
* @brief Algorithm to set up the EQ_GARD and EC_GARD attributes on the proc
*
* @param[in] i_procTarget    Proccesor target to set attributes on
*/
void setChipletGardsOnProc(TARGETING::Target * i_procTarget);


/**
 * @brief Find the ATTR_EC for all processors and calculate the EFFECTIVE_EC
 *        (the lowest EC found)
 *
 */
void calculateEffectiveEC();


/**
 * @brief Mark any MCA units that are present but have a disabled port as
 *        non-functional
 *
 * @return error log handle
 */
errlHndl_t markDisabledMcas();

};   // end namespace

#endif
