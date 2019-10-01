// Copyright (c) 2011 Vince Durham
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#include "auxpow.h"

void CBlockHeader::SetAuxPow(CAuxPow* pow)
{
    if (pow != NULL)
        nVersion |= BLOCK_VERSION_AUXPOW;
    else
        nVersion &= ~BLOCK_VERSION_AUXPOW;
    auxpow.reset(pow);
}
