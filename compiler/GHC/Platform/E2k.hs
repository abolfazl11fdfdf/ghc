{-# LANGUAGE CPP #-}

module GHC.Platform.E2k where

import GhcPrelude

#define MACHREGS_NO_REGS 1
#define MACHREGS_e2k 0
#include "../../../includes/CodeGen.Platform.hs"

