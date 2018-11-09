@REM off
setx GPU_FORCE_64BIT_PTR 0
setx GPU_MAX_HEAP_SIZE 100
setx GPU_USE_SYNC_OBJECTS 1
setx GPU_MAX_ALLOC_PERCENT 100
setx GPU_SINGLE_ALLOC_PERCENT 100

@rem Please ensure the following configuration in energi.conf
@rem
@rem  server=1
@rem  rpcbind=127.0.0.1
@rem  rpcallowip=127.0.0.1
@rem  rcpuser=YourUser
@rem  rpcpassword=YouPassword

energiminer.exe

@rem Add the following parameters:
@rem 
@rem For solo mining:
@rem   "http://YourRPCUser:YourRPCPassword@127.0.0.1:9796" --coinbase-addr "YourReceivingWalletAddress"
@rem 
@rem For pool mining:
@rem   "stratum://YourReceivingWalletAddress.RigName@mining.energi.network:3033"
@rem 
@rem For CUDA:
@rem   -U
@rem For OpenCL:
@rem   -G
@rem 
