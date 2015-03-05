This directory contains the POC SEP service.

 sepdrmjni - this directory contains the implementation of the the
             Java Native Interface library for the libsepdrm APIs.

 apimanagers - this directory contains the implementation of the SEP
             Manager.  The SEP Manager provides the Binder IPC proxy
             to the clients.

 sepserviceapp - this directory conatins implementation of the SEP
             service.  The SEP service is a system service that is
             started at boot.  It implements the SEP Manager
             (Stub) to access chaabi APIs provide by libsepdrm.
