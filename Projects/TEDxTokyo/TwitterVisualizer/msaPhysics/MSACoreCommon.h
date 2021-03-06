#pragma once

#include "MSACoreMath.h"

namespace msa {
	#define DelPointer(p)	if(p) { delete p; p = NULL; }
	#define DelArray(p)		if(p) { delete []p; p = NULL; }
}

namespace MSA = msa;    // for backwards compatibility

