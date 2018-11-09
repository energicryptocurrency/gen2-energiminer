#!/bin/bash

#
# Please ensure the following configuration in ~/.energicore/energi.conf
#
# server=1
# rpcbind=127.0.0.1
# rpcallowip=127.0.0.1
# rcpuser=YourUser
# rpcpassword=YouPassword

./energiminer 

# Add the following parameters:

# For solo mining:
#  "http://YourRPCUser:YourRPCPassword@127.0.0.1:9796" --coinbase-addr "YourReceivingWalletAddress"
# For pool mining:
#  "stratum://YourReceivingWalletAddress.RigName@mining.energi.network:3033"

# For CUDA:
#  -U
# For OpenCL:
#  -G
