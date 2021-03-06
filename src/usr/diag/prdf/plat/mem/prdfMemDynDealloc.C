/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/diag/prdf/plat/mem/prdfMemDynDealloc.C $              */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2017,2018                        */
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
//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------

#include <iipSystem.h>
#include <prdfGlobal_common.H>
#include <prdfExtensibleChip.H>
#include <prdfMemDynDealloc.H>
#include <prdfTrace.H>
#include <prdfPlatServices.H>
#include <prdfMemAddress.H>

//------------------------------------------------------------------------------
// Function Definitions
//------------------------------------------------------------------------------

using namespace TARGETING;

namespace PRDF
{
using namespace PlatServices;

namespace MemDealloc
{

bool isEnabled()
{
    return ( isHyprRunning() && (isHyprConfigPhyp() || isHyprConfigOpal()) &&
             !isMfgAvpEnabled() && !isMfgHdatAvpEnabled() );
}

int32_t __getAddrConfig( ExtensibleChip * i_mcaChip, uint8_t i_dslct,
                         bool & o_twoDimmConfig, uint8_t & o_mrnkBits,
                         uint8_t & o_srnkBits, uint8_t & o_extraRowBits )
{
    #define PRDF_FUNC "[MemDealloc::__getAddrConfig] "

    int32_t o_rc = SUCCESS;

    SCAN_COMM_REGISTER_CLASS * reg = i_mcaChip->getRegister( "MC_ADDR_TRANS" );
    o_rc = reg->Read();
    if ( SUCCESS != o_rc )
    {
        PRDF_ERR( PRDF_FUNC "Read failed on MC_ADDR_TRANS: i_mcaChip=0x%08x",
                  i_mcaChip->getHuid() );
        return o_rc;
    }

    o_twoDimmConfig = false;
    if ( reg->IsBitSet(0) && reg->IsBitSet(16) )
        o_twoDimmConfig = true;

    o_mrnkBits = 0;
    if ( reg->IsBitSet(i_dslct ? 21: 5) ) o_mrnkBits++;
    if ( reg->IsBitSet(i_dslct ? 22: 6) ) o_mrnkBits++;

    o_srnkBits = 0;
    if ( reg->IsBitSet(i_dslct ? 25: 9) ) o_srnkBits++;
    if ( reg->IsBitSet(i_dslct ? 26:10) ) o_srnkBits++;
    if ( reg->IsBitSet(i_dslct ? 27:11) ) o_srnkBits++;

    // According to the hardware team, B2 is used for DDR4e which went away. If
    // for some reason B2 is valid, there is definitely a bug.
    if ( reg->IsBitSet(i_dslct ? 28:12) )
    {
        PRDF_ERR( PRDF_FUNC "B2 enabled in MC_ADDR_TRANS: i_mcaChip=0x%08x "
                  "i_dslct=%d", i_mcaChip->getHuid(), i_dslct );
        return FAIL;
    }

    o_extraRowBits = 0;
    if ( reg->IsBitSet(i_dslct ? 29:13) ) o_extraRowBits++;
    if ( reg->IsBitSet(i_dslct ? 30:14) ) o_extraRowBits++;
    if ( reg->IsBitSet(i_dslct ? 31:15) ) o_extraRowBits++;

    return o_rc;

    #undef PRDF_FUNC
}

uint64_t __reverseBits( uint64_t i_val, uint64_t i_numBits )
{
    uint64_t o_val = 0;

    for ( uint64_t i = 0; i < i_numBits; i++ )
    {
        o_val <<= 1;
        o_val |= i_val & 0x1;
        i_val >>= 1;
    }

    return o_val;
}

uint64_t __maskBits( uint64_t i_val, uint64_t i_numBits )
{
    uint64_t mask = (0xffffffffffffffffull >> i_numBits) << i_numBits;
    return i_val & ~mask;
}

uint64_t __countBits( uint64_t i_val )
{
    uint64_t o_count = 0;

    while ( 0 != i_val )
    {
        if ( 1 == (i_val & 0x1) )
            o_count++;
        i_val >>= 1;
    }

    return o_count;
}

int32_t __getMcaPortAddr( ExtensibleChip * i_chip, MemAddr i_addr,
                          uint64_t & o_addr )
{
    int32_t o_rc = SUCCESS;

    o_addr = 0;

    // Local vars for address fields
    uint64_t col   = __reverseBits(i_addr.getCol(),  7); // C9 C8 C7 C6 C5 C4 C3
    uint64_t row   = __reverseBits(i_addr.getRow(), 18); // R17 R16 R15 .. R1 R0
    uint64_t bnk   = i_addr.getBank();                   //     BG0 BG1 B0 B1 B2
    uint64_t srnk  = i_addr.getRank().getSlave();        //             S0 S1 S2
    uint64_t mrnk  = i_addr.getRank().getRankSlct();     //                M0 M1
    uint64_t dslct = i_addr.getRank().getDimmSlct();     //                    D

    // Determine if a two DIMM config is used. Also, determine how many
    // mrank (M0-M1), srnk (S0-S2), or extra row (R17-R15) bits are used.
    bool twoDimmConfig;
    uint8_t mrnkBits, srnkBits, extraRowBits;
    o_rc = __getAddrConfig( i_chip, dslct, twoDimmConfig, mrnkBits, srnkBits,
                            extraRowBits );
    if ( SUCCESS != o_rc ) return o_rc;

    // Mask off the non-configured bits. If this address came from hardware,
    // this would not be a problem. However, the get_mrank_range() and
    // get_srank_range() HWPS got lazy just set the entire fields and did not
    // take into account the actual bit ranges.
    mrnk = __maskBits( mrnk, mrnkBits );
    srnk = __maskBits( srnk, srnkBits );
    row  = __maskBits( row,  15 + extraRowBits );

    // Combine master and slave ranks.
    uint64_t rnk     = (mrnk << srnkBits) | srnk;
    uint8_t  rnkBits = mrnkBits + srnkBits;

    // Now split the DIMM select and combined rank into components.
    uint64_t rnk_pt1     = 0, rnk_pt2     = 0, rnk_pt3     = 0;
    uint8_t  rnkBits_pt1 = 0, rnkBits_pt2 = 0, rnkBits_pt3 = 0;

    if ( 0 == rnkBits )
    {
        if ( twoDimmConfig ) // The DIMM select goes into part 3.
        {
            rnk_pt3 = dslct; rnkBits_pt3 = 1;
        }
    }
    else // At least one master or slave.
    {
        // Put the LSB of the combined rank in part 3 and the rest in part 2.
        rnk_pt3 = rnk & 0x1; rnkBits_pt3 = 1;
        rnk_pt2 = rnk >> 1;  rnkBits_pt2 = rnkBits - 1;

        if ( twoDimmConfig ) // The DIMM select goes into part 1.
        {
            rnk_pt1 = dslct; rnkBits_pt1 = 1;
        }
    }

    // Split the row into its components.
    uint64_t r17_r15 = (row & 0x38000) >> 15;
    uint64_t r14     = (row & 0x04000) >> 14;
    uint64_t r13     = (row & 0x02000) >> 13;
    uint64_t r12_r0  = (row & 0x01fff);

    // Split the column into its components.
    uint64_t c9_c4 = (col & 0x7e) >> 1;
    uint64_t c3    = (col & 0x01);

    // Split the bank into its components.
    uint64_t b0      = (bnk & 0x10) >> 4;
    uint64_t b1      = (bnk & 0x08) >> 3;
    // NOTE: B2 is not supported on Nimbus.
    uint64_t bg0_bg1 = (bnk & 0x03);

    // Now start building the flexible part of the address (bits 0-7,23-33).
    o_addr = (o_addr << rnkBits_pt1 ) | rnk_pt1;
    o_addr = (o_addr << extraRowBits) | r17_r15;
    o_addr = (o_addr << rnkBits_pt2 ) | rnk_pt2;
    o_addr = (o_addr << 6           ) | c9_c4;
    o_addr = (o_addr << 1           ) | b0;
    o_addr = (o_addr << rnkBits_pt3 ) | rnk_pt3;
    o_addr = (o_addr << 1           ) | b1;
    o_addr = (o_addr << 2           ) | bg0_bg1;
    o_addr = (o_addr << 1           ) | c3;

    // C2 is in bit 34, but the Nimbus physical address does not contain a C2.
    // It will be set to 0 for now. Also, bits 35-39 are the rest of the cache
    // line address, which we do not need. So, that will be set to 0 as well.
    o_addr <<= 6;

    // Finally, insert R14,R12-R0,R13 into bits 8-22.
    o_addr  = ((o_addr & 0xfffffe0000ull) << 15) | (o_addr & 0x000001ffffull);
    o_addr |= ((r14 << 14) | (r12_r0 << 1) | r13) << 17;

    return o_rc;
}

//------------------------------------------------------------------------------

template<TYPE T>
uint32_t __getGrpInfo( ExtensibleChip * i_chip, uint64_t & o_grpChnls,
                       uint64_t & o_grpId, uint64_t & o_grpSize,
                       uint64_t & o_grpBar );

template<>
uint32_t __getGrpInfo<TYPE_MCA>( ExtensibleChip * i_chip, uint64_t & o_grpChnls,
                                 uint64_t & o_grpId, uint64_t & o_grpSize,
                                 uint64_t & o_grpBar )
{
    #define PRDF_FUNC "[MemDealloc::__getGrpInfo] "

    uint32_t o_rc = SUCCESS;

    do
    {
        // Get the connecte MCS chip and MCA target position.
        ExtensibleChip * mcs_chip = getConnectedParent( i_chip, TYPE_MCS );
        uint8_t mcaPos = i_chip->getPos() % MAX_MCA_PER_MCS;

        SCAN_COMM_REGISTER_CLASS * mcfgp  = mcs_chip->getRegister("MCFGP");
        SCAN_COMM_REGISTER_CLASS * mcfgpm = mcs_chip->getRegister("MCFGPM");
        o_rc = mcfgp->Read();  if ( SUCCESS != o_rc ) break;
        o_rc = mcfgpm->Read(); if ( SUCCESS != o_rc ) break;

        // Get the number of channels in this group.
        uint8_t mcGrpCnfg = mcfgp->GetBitFieldJustified( 1, 4 );
        switch ( mcGrpCnfg )
        {
            case 0: o_grpChnls = 1;                     break; // 11
            case 1: o_grpChnls = (0 == mcaPos) ? 1 : 3; break; // 13
            case 2: o_grpChnls = (0 == mcaPos) ? 3 : 1; break; // 31
            case 3: o_grpChnls = 3;                     break; // 33
            case 4: o_grpChnls = 2;                     break; // 2D
            case 5: o_grpChnls = 2;                     break; // 2S
            case 6: o_grpChnls = 4;                     break; // 4
            case 7: o_grpChnls = 6;                     break; // 6
            case 8: o_grpChnls = 8;                     break; // 8
            default:
                PRDF_ERR( PRDF_FUNC "Invalid MC channels per group value: 0x%x "
                          "on 0x%08x port %d", mcGrpCnfg, mcs_chip->getHuid(),
                          mcaPos );
                o_rc = FAIL;
        }
        if ( SUCCESS != o_rc ) break;

        // Get the group ID and group size.
        o_grpId   = mcfgp->GetBitFieldJustified( (0 == mcaPos) ? 5 : 8, 3 );
        o_grpSize = mcfgp->GetBitFieldJustified( 13, 11 );

        // Get the base address (BAR).
        if ( 0 == mcaPos ) // MCS channel 0
        {
            // Channel 0 is always from the MCFGP.
            o_grpBar = mcfgp->GetBitFieldJustified(24, 24);
        }
        else // MCS channel 1
        {
            switch ( mcGrpCnfg )
            {
                // Each channel is in an different group. Use the MCFGPM.
                case 0: // 11
                case 1: // 13
                case 2: // 31
                case 3: // 33
                case 4: // 2D
                    o_grpBar = mcfgpm->GetBitFieldJustified(24, 24);
                    break;

                // Both channels are in the same group. Use the MCFGP.
                case 5: // 2S
                case 6: // 4
                case 7: // 6
                case 8: // 8
                    o_grpBar = mcfgp->GetBitFieldJustified(24, 24);
                    break;

                default:
                    PRDF_ERR( PRDF_FUNC "Invalid MC channels per group value: "
                              "0x%x on 0x%08x port %d", mcGrpCnfg,
                              mcs_chip->getHuid(), mcaPos );
                    o_rc = FAIL;
            }
        }
        if ( SUCCESS != o_rc ) break;

    } while (0);

    return o_rc;

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

template<TYPE T>
uint32_t __insertGrpId( uint64_t & io_addr, uint64_t i_grpChnls,
                        uint64_t i_grpId );

template<>
uint32_t __insertGrpId<TYPE_MCA>( uint64_t & io_addr, uint64_t i_grpChnls,
                                  uint64_t i_grpId )
{
    #define PRDF_FUNC "[MemDealloc::__insertGrpId] "

    uint32_t o_rc = SUCCESS;

    // Notes on 3 and 6 channel per group configs:
    //      Let's use an example of 3 channels in a group with 4 GB per channel.
    //      The group size will be configured like there are 4 channels (16 GB
    //      total). However, only the first 12 GB of the 16 GB are used because
    //      there are only three channels. Since we need a contiguous address
    //      space and can't have holes every fourth address, the hardware uses
    //      some crafty mod3 logic to evenly distribute the addresses among the
    //      3 channels. The mod3 hashing is based on the address itself so there
    //      isn't a traditional group select like we are used to in the 2, 4,
    //      and 8 channel group configs. For 3 MC/group configs, there is no
    //      shifting (same as 1 MC/group). For 6 MC/group configs, we need to
    //      insert the least significant bit of the group ID into RA[56] (same
    //      as 2 MC/group).

    uint64_t upper33 = io_addr & 0xFFFFFFFF80ull;
    uint64_t lower7  = io_addr & 0x000000007full;

    switch ( i_grpChnls )
    {
        case 1:
        case 3: // no shifting
            break;

        case 2:
        case 6: // insert 1 bit
            io_addr = (upper33 << 1) | ((i_grpId & 0x1) << 7) | lower7;
            break;

        case 4: // insert 2 bits
            io_addr = (upper33 << 2) | ((i_grpId & 0x3) << 7) | lower7;
            break;

        case 8: // insert 3 bits
            io_addr = (upper33 << 3) | ((i_grpId & 0x7) << 7) | lower7;
            break;

        default:
            PRDF_ERR( PRDF_FUNC "Invalid MC channels per group value %d",
                      i_grpChnls );
            o_rc = FAIL;
    }

    return o_rc;

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

// The hardware uses a mod3 hashing algorithm to calculate which memory channel
// an address belongs to. This is calulated with the following:
//      r0 = B3MF(RA[47:48])
//      r1 = rrotate(r0, RA[45:46])
//      r2 = rrotate(r1, RA[43:44])
//      r3 = rrotate(r2, m[0:1])
// RA is the real address, m[0:1] is the two most significant bits of the port
// address, and r3 is the mod3 hash. Since we are translating from the phyiscal
// address to the real address, we don't have m[0:1]. So the goal here is to
// calculate that. Fortunately, we have the 40-bit port address, which is where
// we can get RA[43:48] to calculate r2. We can also do a reverse lookup with
// the group ID and RA[55:56] to find r3. From there, we just need to solve for
// m[0:1] and add it to the beginning of the port address.

// Converts a 2-bit number into the binned (one-hot) 3-modulus format (B3MF).
//    mod(0) = 0b00 = 0b100
//    mod(1) = 0b01 = 0b010
//    mod(2) = 0b10 = 0b001
//    mod(3) = 0b00 = 0b100
uint8_t __b3mf( uint8_t i_val )
{
    return 4 >> (i_val % 3);
}

// Rotates i_b3mf right by i_num bits.
uint8_t __rrotate( uint8_t i_b3mf, uint8_t i_num )
{
    uint8_t o_b3mf = i_b3mf;
    for ( uint8_t n = 0; n < i_num; n++ )
    {
        o_b3mf = ((o_b3mf & 0x6) >> 1) | ((o_b3mf & 0x1) << 2);
    }
    return o_b3mf;
}

// Rotates i_b3mf left by i_num bits.
uint8_t __lrotate( uint8_t i_b3mf, uint8_t i_num )
{
    uint8_t o_b3mf = i_b3mf;
    for ( uint8_t n = 0; n < i_num; n++ )
    {
        o_b3mf = ((o_b3mf & 0x3) << 1) | ((o_b3mf & 0x4) >> 2);
    }
    return o_b3mf;
}

template<TYPE T>
uint64_t __getMsb( uint64_t i_addr, uint64_t i_grpChnls, uint64_t i_grpId );

template<>
uint64_t __getMsb<TYPE_MCA>( uint64_t i_addr, uint64_t i_grpChnls,
                             uint64_t i_grpId )
{
    uint64_t o_msb = 0;

    // Start by calculating r2 (see description above) and extracting RA[55:56].
    uint8_t r0       = __b3mf(        (i_addr >> 15) & 0x3 ); // RA[47:48]
    uint8_t r1       = __rrotate( r0, (i_addr >> 17) & 0x3 ); // RA[45:46]
    uint8_t r2       = __rrotate( r1, (i_addr >> 19) & 0x3 ); // RA[43:44]
    uint8_t ra_55_56 =                (i_addr >>  7) & 0x3;   // RA[55:56]

    // Special case for 6 MC/grp configs.
    if ( 6 == i_grpChnls )
    {
        // Note that the LSB of the group ID has already been inserted into
        // RA[56] (via __insertGrpId()). That bit should not be used to
        // calculate the mod 3 hash.
        i_grpId  = i_grpId  & 0x6; // Top two bits of the group ID.
        ra_55_56 = ra_55_56 & 0x2; // Only bit 55.
    }

    // Get the mod3 hash. There are some tables in sections 2.12.1 and 2.12.2 of
    // the Cumulus MC workbook. Fortunately, those tables can be boiled down to
    // some bit shifting.
    uint8_t r3 = __lrotate( __b3mf(i_grpId), ra_55_56 );

    // Given r2 and r3, calculate the MSBs for the port address by counting the
    // number of lrotates on r3 it takes to match r2.
    while ( r2 != r3 )
    {
        r3 = __lrotate( r3, 1 );
        o_msb++;
    }

    return o_msb;
}

//------------------------------------------------------------------------------

template<TYPE T>
void __insertMsb( uint64_t & io_addr, uint64_t i_grpSize, uint64_t i_msb );

template<>
void __insertMsb<TYPE_MCA>( uint64_t & io_addr, uint64_t i_grpSize,
                            uint64_t i_msb )
{
    // i_grpSize is a mask for the BAR. All we have to do is count the number
    // of bits in that value to determine how many extra bits we need to shift
    // in order to get the MSB in the correct position. Refer to the MC workbook
    // for details of the bit positions based on the group size.
    io_addr |= i_msb << ( 30 + __countBits(i_grpSize) );
}

//------------------------------------------------------------------------------

template<TYPE T>
void __addBar( uint64_t & io_addr, uint64_t i_grpBar );

template<>
void __addBar<TYPE_MCA>( uint64_t & io_addr, uint64_t i_grpBar )
{
    // The BAR field is 24 bits and always starts at bit 8 of the real address.
    io_addr |= (i_grpBar << 32);
}

//------------------------------------------------------------------------------

template<TYPE T>
uint32_t getSystemAddr( ExtensibleChip * i_chip, MemAddr i_addr,
                        uint64_t & o_addr );

template<>
uint32_t getSystemAddr<TYPE_MCA>( ExtensibleChip * i_chip, MemAddr i_addr,
                                  uint64_t & o_addr )
{
    #define PRDF_FUNC "[MemDealloc::getSystemAddr] "

    uint32_t o_rc = SUCCESS;

    do
    {
        // Get the group information.
        uint64_t grpChnls, grpId, grpSize, grpBar;
        o_rc = __getGrpInfo<TYPE_MCA>(i_chip, grpChnls, grpId, grpSize, grpBar);
        if ( SUCCESS != o_rc ) break;

        // Get the 40-bit port address (right justified).
        o_rc = __getMcaPortAddr( i_chip, i_addr, o_addr );
        if ( SUCCESS != o_rc ) break;

        // Insert the group ID.
        o_rc = __insertGrpId<TYPE_MCA>( o_addr, grpChnls, grpId );
        if ( SUCCESS != o_rc ) break;

        // Notes on 3 and 6 channel per group configs:
        //      Now that the group ID has been inserted, if applicable, we need
        //      to add the two most significant (MSB) bits to the beginning of
        //      the port address. These bits are calculated with a special mod3
        //      hashing algorithm.
        if ( 3 == grpChnls || 6 == grpChnls )
        {
            uint64_t msb = __getMsb<TYPE_MCA>( o_addr, grpChnls, grpId );
            __insertMsb<TYPE_MCA>( o_addr, grpSize, msb );
        }

        // Add the BAR to the rest of the address.
        __addBar<TYPE_MCA>( o_addr, grpBar );

    } while (0);

    return o_rc;

    #undef PRDF_FUNC
}

template<>
uint32_t getSystemAddr<TYPE_MBA>( ExtensibleChip * i_chip, MemAddr i_addr,
                                  uint64_t & o_addr )
{
    #define PRDF_FUNC "[MemDealloc::getSystemAddr] "

    // TODO - RTC: 190115
    PRDF_ERR( PRDF_FUNC "not supported on MBA yet: i_chip=0x%08x",
              i_chip->getHuid() );
    return FAIL; // Returning FAIL will prevent us from sending any false
                 // messages to the hypervisor.

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

template<TYPE T>
uint32_t getSystemAddrRange( ExtensibleChip * i_chip,
                             MemAddr    i_saddr, MemAddr    i_eaddr,
                             uint64_t & o_saddr, uint64_t & o_eaddr );

template<>
uint32_t getSystemAddrRange<TYPE_MCA>( ExtensibleChip * i_chip,
                                       MemAddr    i_saddr, MemAddr    i_eaddr,
                                       uint64_t & o_saddr, uint64_t & o_eaddr )
{
    #define PRDF_FUNC "[MemDealloc::getSystemAddrRange] "

    uint32_t o_rc = SUCCESS;

    do
    {
        // Get the group information.
        uint64_t grpChnls, grpId, grpSize, grpBar;
        o_rc = __getGrpInfo<TYPE_MCA>(i_chip, grpChnls, grpId, grpSize, grpBar);
        if ( SUCCESS != o_rc ) break;

        // Get the 40-bit port addresses (right justified).
        o_rc  = __getMcaPortAddr( i_chip, i_saddr, o_saddr );
        o_rc |= __getMcaPortAddr( i_chip, i_eaddr, o_eaddr );
        if ( SUCCESS != o_rc ) break;

        // Insert the group ID.
        o_rc  = __insertGrpId<TYPE_MCA>( o_saddr, grpChnls, grpId );
        o_rc |= __insertGrpId<TYPE_MCA>( o_eaddr, grpChnls, grpId );
        if ( SUCCESS != o_rc ) break;

        // Notes on 3 and 6 channel per group configs:
        //   It turns out that with 3 and 6 MC/group configs every address is
        //   interleaved, meaning that three consecutive physical addresses have
        //   three different MSBs. In addition, that hashing is not so simple.
        //   The given i_saddr and i_eaddr may be on MSB b10 and MSB b00,
        //   respectively. This really mucks things up when the start address is
        //   larger than the end address. To circumvent this issue, we have to
        //   bypass the actual MSBs and force o_saddr and o_eaddr to have
        //   MSB b00 and MSB b10, respectively.
        if ( 3 == grpChnls || 6 == grpChnls )
        {
            __insertMsb<TYPE_MCA>( o_saddr, grpSize, 0 );
            __insertMsb<TYPE_MCA>( o_eaddr, grpSize, 2 );
        }

        // Add the BAR to the rest of the address.
        __addBar<TYPE_MCA>( o_saddr, grpBar );
        __addBar<TYPE_MCA>( o_eaddr, grpBar );

    } while (0);

    return o_rc;

    #undef PRDF_FUNC
}

template<>
uint32_t getSystemAddrRange<TYPE_MBA>( ExtensibleChip * i_chip,
                                       MemAddr    i_saddr, MemAddr    i_eaddr,
                                       uint64_t & o_saddr, uint64_t & o_eaddr )
{
    #define PRDF_FUNC "[MemDealloc::getSystemAddrRange] "

    // TODO - RTC: 190115
    PRDF_ERR( PRDF_FUNC "not supported on MBA yet: i_chip=0x%08x",
              i_chip->getHuid() );
    return FAIL; // Returning FAIL will prevent us from sending any false
                 // messages to the hypervisor.

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

template<TYPE T>
int32_t page( ExtensibleChip * i_chip, MemAddr i_addr )
{
    #define PRDF_FUNC "[MemDealloc::page] "

    uint64_t sysAddr = 0;
    int32_t o_rc = SUCCESS;
    do
    {
        if ( !isEnabled() ) break; // nothing to do

        o_rc = getSystemAddr<T>( i_chip, i_addr, sysAddr);
        if( SUCCESS != o_rc )
        {
            PRDF_ERR( PRDF_FUNC "getSystemAddr() failed. HUID:0x%08X",
                      i_chip->GetId() );
            break;
        }

        sendPageGardRequest( sysAddr );
        PRDF_TRAC( PRDF_FUNC "Page dealloc address: 0x%016llx", sysAddr );

    } while( 0 );

    return o_rc;
    #undef PRDF_FUNC
}
template int32_t page<TYPE_MCA>( ExtensibleChip * i_chip, MemAddr i_addr );
template int32_t page<TYPE_MBA>( ExtensibleChip * i_chip, MemAddr i_addr );

//------------------------------------------------------------------------------

template<TYPE T>
int32_t rank( ExtensibleChip * i_chip, MemRank i_rank )
{
    #define PRDF_FUNC "[MemDealloc::rank] "

    int32_t o_rc = SUCCESS;

    do
    {
        if ( !isEnabled() ) break; // nothing to do

        // Get the address range of i_rank.
        MemAddr startAddr, endAddr;
        o_rc = getMemAddrRange<T>( i_chip, i_rank, startAddr, endAddr,
                                   SLAVE_RANK );
        if ( SUCCESS != o_rc )
        {
            PRDF_ERR( PRDF_FUNC "getMemAddrRange(0x%08x,0x%02x) failed",
                      i_chip->getHuid(), i_rank.getKey() );
            break;
        }

        // Get the system addresses.
        uint64_t ssAddr = 0;
        uint64_t seAddr = 0;
        o_rc = getSystemAddrRange<T>( i_chip, startAddr, endAddr,
                                              ssAddr,    seAddr );
        if ( SUCCESS != o_rc )
        {
            PRDF_ERR( PRDF_FUNC "getSystemAddrRange(0x%08x) failed",
                      i_chip->getHuid() );
            break;
        }

        // Send the address range to the hypervisor.
        sendDynMemDeallocRequest( ssAddr, seAddr );
        PRDF_TRAC( PRDF_FUNC "Rank dealloc for Start Addr: 0x%016llx "
                   "End Addr: 0x%016llx", ssAddr, seAddr );

    } while (0);

    return o_rc;

    #undef PRDF_FUNC
}
template int32_t rank<TYPE_MCA>( ExtensibleChip * i_chip, MemRank i_rank );
template int32_t rank<TYPE_MBA>( ExtensibleChip * i_chip, MemRank i_rank );

//------------------------------------------------------------------------------

template<TYPE T>
int32_t port( ExtensibleChip * i_chip )
{
    #define PRDF_FUNC "[MemDealloc::port] "

    int32_t o_rc = SUCCESS;

    do
    {
        if ( !isEnabled() ) break; // nothing to do

        // Get the address range of i_chip.
        MemAddr startAddr, endAddr;
        o_rc = getMemAddrRange<T>( i_chip, startAddr, endAddr );
        if ( SUCCESS != o_rc )
        {
            PRDF_ERR( PRDF_FUNC "getMemAddrRange(0x%08x) failed",
                      i_chip->getHuid() );
            break;
        }

        // Get the system addresses.
        uint64_t ssAddr = 0;
        uint64_t seAddr = 0;
        o_rc = getSystemAddrRange<T>( i_chip, startAddr, endAddr,
                                              ssAddr,    seAddr );
        if ( SUCCESS != o_rc )
        {
            PRDF_ERR( PRDF_FUNC "getSystemAddrRange(0x%08x) failed",
                      i_chip->getHuid() );
            break;
        }

        // Send the address range to the hypervisor.
        sendDynMemDeallocRequest( ssAddr, seAddr );
        PRDF_TRAC( PRDF_FUNC "Port dealloc for Start Addr: 0x%016llx "
                   "End Addr: 0x%016llx", ssAddr, seAddr );

    } while (0);

    return o_rc;

    #undef PRDF_FUNC
}
template int32_t port<TYPE_MCA>( ExtensibleChip * i_chip );
template int32_t port<TYPE_MBA>( ExtensibleChip * i_chip );

//------------------------------------------------------------------------------

template <TYPE T>
int32_t __getDimmRange( TargetHandle_t i_dimm,
                        uint64_t & o_ssAddr, uint64_t & o_seAddr )
{
    #define PRDF_FUNC "[MemDealloc::__getDimmRange] "

    int32_t o_rc = SUCCESS;

    o_ssAddr = o_seAddr = 0;

    do
    {
        // Get the MCA, MBA, etc. connected to this DIMM.
        TargetHandle_t trgt = getConnectedParent( i_dimm, T );
        ExtensibleChip * chip = (ExtensibleChip *)systemPtr->GetChip( trgt );
        if ( nullptr == chip )
        {
            PRDF_ERR( PRDF_FUNC "No chip connected to DIMM" );
            o_rc = FAIL; break;
        }

        // Get the DIMM select.
        uint8_t dimmSlct = getDimmSlct<T>( i_dimm );

        // Get the address range of i_dimm.
        MemAddr startAddr, endAddr;
        o_rc = getMemAddrRange<T>( chip, startAddr, endAddr, dimmSlct );
        if ( SUCCESS != o_rc )
        {
            PRDF_ERR( PRDF_FUNC "getMemAddrRange(0x%08x,%d) failed",
                      chip->getHuid(), dimmSlct );
            break;
        }

        // Get the system addresses.
        o_rc = getSystemAddrRange<T>( chip, startAddr, endAddr,
                                            o_ssAddr,  o_seAddr );
        if ( SUCCESS != o_rc )
        {
            PRDF_ERR( PRDF_FUNC "getSystemAddrRange(0x%08x) failed",
                      chip->getHuid() );
            break;
        }

    } while (0);

    return o_rc;

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

template <TYPE T>
int32_t dimmSlct( TargetHandle_t i_dimm )
{
    #define PRDF_FUNC "[MemDealloc::dimmSlct] "

    int32_t o_rc = SUCCESS;

    do
    {
        if ( !isEnabled() ) break; // nothing to do

        // Get the system addresses.
        uint64_t ssAddr = 0, seAddr = 0;
        o_rc = __getDimmRange<T>( i_dimm, ssAddr, seAddr );
        if ( SUCCESS != o_rc )
        {
            PRDF_ERR( PRDF_FUNC "__getDimmRange(0x%08x) failed",
                      getHuid(i_dimm) );
            break;
        }

        // Send the address range to the hypervisor.
        sendDynMemDeallocRequest( ssAddr, seAddr );
        PRDF_TRAC( PRDF_FUNC "DIMM Slct dealloc for Start Addr: 0x%016llx "
                   "End Addr: 0x%016llx", ssAddr, seAddr );

    } while (0);

    return o_rc;

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

template <TYPE T>
bool isDimmPair( TargetHandle_t i_dimm1, TargetHandle_t i_dimm2 )
{
    #define PRDF_FUNC "[MemDealloc::isDimmPair] "
    bool isDimmPair = false;
    do
    {
        uint8_t dimm1Slct = getDimmSlct<T>( i_dimm1 );
        uint8_t dimm2Slct = getDimmSlct<T>( i_dimm2 );

        isDimmPair = ( ( dimm1Slct == dimm2Slct ) &&
                       ( getConnectedParent( i_dimm1, T ) ==
                                 getConnectedParent( i_dimm2, T )));
    } while(0);
    return isDimmPair;
    #undef PRDF_FUNC
}

// This function is used for sorting dimms in a list.
template <TYPE T>
bool compareDimms( TargetHandle_t i_dimm1, TargetHandle_t i_dimm2 )
{
    #define PRDF_FUNC "[MemDealloc::compareDimms] "
    bool isSmall = false;
    do
    {
        uint8_t dimm1Slct = getDimmSlct<T>( i_dimm1 );
        uint8_t dimm2Slct = getDimmSlct<T>( i_dimm2 );

        TargetHandle_t tgt1 = getConnectedParent( i_dimm1, T );
        TargetHandle_t tgt2 = getConnectedParent( i_dimm2, T );

        isSmall = ( ( tgt1 < tgt2 ) ||
                    ( ( tgt1 == tgt2) && ( dimm1Slct < dimm2Slct )));

    } while(0);

    return isSmall;
    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

template <TYPE T>
int32_t dimmList( TargetHandleList  & i_dimmList )
{
    #define PRDF_FUNC "[MemDealloc::dimmList] "

    int32_t o_rc = SUCCESS;

    // Find unique dimm slct.
    std::sort( i_dimmList.begin(), i_dimmList.end(), compareDimms<T> );
    TargetHandleList::iterator uniqueDimmEndIt =
                std::unique( i_dimmList.begin(), i_dimmList.end(),
                             isDimmPair<T> );

    for( TargetHandleList::iterator it = i_dimmList.begin();
         it != uniqueDimmEndIt; it++ )
    {
        // Get the system addresses.
        uint64_t ssAddr = 0, seAddr = 0;
        if ( SUCCESS != __getDimmRange<T>(*it, ssAddr, seAddr) )
        {
            PRDF_ERR( PRDF_FUNC "__getDimmRange(0x%08x) failed", getHuid(*it) );
            o_rc = FAIL; continue; // Continue to the next DIMM.
        }

        // Send the address range to the hypervisor.
        sendPredDeallocRequest( ssAddr, seAddr );
        PRDF_TRAC( PRDF_FUNC "Predictive dealloc for start addr: 0x%016llx "
                   "end addr: 0x%016llx", ssAddr, seAddr );
    }

    return o_rc;

    #undef PRDF_FUNC
}

//------------------------------------------------------------------------------

int32_t dimmList( TargetHandleList  & i_dimmList )
{
    #define PRDF_FUNC "[MemDealloc::dimmList] "

    int32_t o_rc = SUCCESS;

    do
    {
        if ( i_dimmList.empty() ) break;

        // Determine what target these DIMMs are connected to.
        // Note that we cannot use getConnectedParent() because it will assert
        // if there is no parent of that type.

        TargetHandle_t dimmTrgt = i_dimmList.front();
        TargetHandleList list;

        // First, check for MCAs.
        list = getConnected( dimmTrgt, TYPE_MCA );
        if ( !list.empty() )
        {
            o_rc = dimmList<TYPE_MCA>( i_dimmList );
            break;
        }

        // Second, check for MBAs.
        list = getConnected( dimmTrgt, TYPE_MBA );
        if ( !list.empty() )
        {
            o_rc = dimmList<TYPE_MBA>( i_dimmList );
            break;
        }

        // If we get here we did not find a supported target.
        PRDF_ERR( PRDF_FUNC "Unsupported connected parent to dimm 0x%08x",
                  getHuid(dimmTrgt) );
        PRDF_ASSERT(false); // code bug

    } while (0);

    return o_rc;

    #undef PRDF_FUNC
}

} //namespace MemDealloc
} // namespace PRDF

