# Security manager

> IOP module for security related functions.

Mainly related to memory card authentication and KELF binding.

While the console built-in secrman module can only authenticate memory cards and KELF Files. this special version can also bind KELF Files to a memory card.

The actual encryption/decryption is performed by the MechaCon processor. As such, this module does not include or require any encryption keys.

The source code for this module is based upon the source code from FMCB Installer v0.987.

## Usage
In order to use this module in your program, you must integrate the module into an IOPRP image. and must be named `SECRMAN` (not `secrman.irx` or whatever) before adding it into the IOPRP Image

## Dependencies

This module requires to load [secrsif](../secrsif/) after this module in order to use all the features it offers
