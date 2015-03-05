/*
********************************************************************************
**    Intel Architecture Group
**    Copyright (C) 2009-2010 Intel Corporation
********************************************************************************
**
**    INTEL CONFIDENTIAL
**    This file, software, or program is supplied under the terms of a
**    license agreement and/or nondisclosure agreement with Intel Corporation
**    and may not be copied or disclosed except in accordance with the
**    terms of that agreement.  This file, software, or program contains
**    copyrighted material and/or trade secret information of Intel
**    Corporation, and must be treated as such.  Intel reserves all rights
**    in this material, except as the license agreement or nondisclosure
**    agreement specifically indicate.
**
**    All rights reserved.  No part of this program or publication
**    may be reproduced, transmitted, transcribed, stored in a
**    retrieval system, or translated into any language or computer
**    language, in any form or by any means, electronic, mechanical,
**    magnetic, optical, chemical, manual, or otherwise, without
**    the prior written permission of Intel Corporation.
**
**    Intel makes no warranty of any kind regarding this code.  This code
**    is provided on an "As Is" basis and Intel will not provide any support,
**    assistance, installation, training or other services.  Intel does not
**    provide any updates, enhancements or extensions.  Intel specifically
**    disclaims any warranty of merchantability, noninfringement, fitness
**    for any particular purpose, or any other warranty.
**
**    Intel disclaims all liability, including liability for infringement
**    of any proprietary rights, relating to use of the code.  No license,
**    express or implied, by estoppel or otherwise, to any intellectual
**    property rights is granted herein.
**/

package com.intel.host.ipt.iha;

/**
********************************************************************************
**
** @file IhaJniException.java
**
** @brief Exception class to return IHA error codes for the IHA JNI API.
** 
** Inherit from RuntimeException (unchecked) so that callers are not
** required to handle the exception.
**
**    @author Ranjit Narjala
**
********************************************************************************
*/
public class IhaException extends java.lang.RuntimeException {

    private int errorCode;

    /**************************************************************************
     * Constructor
     *
     * @param	message: String that defines the error to be thrown.
     *
     * @return	void
     *****************************************************************************/
    public IhaException(String message) {
	super(message);
	errorCode = 0;
    }


    /**************************************************************************
     * Constructor
     *
     * @param	error: Error code that is thrown.
     *
     * @return	void
     *****************************************************************************/
    public IhaException(int error) {
	errorCode = error;
    }

    /**************************************************************************
     * Constructor
     *
     * @param	error: Error code that is thrown.
     *
     * @return	void
     *****************************************************************************/
    public IhaException(String msg, int error) {
	super(msg);
	errorCode = error;
    }


    /**************************************************************************
     * Retrieve the error code when an IHA exception is thrown
     *
     * @param	none
     *
     * @return	IHA error code
     *****************************************************************************/
    public int GetError() {
	return errorCode;
    }
} // class IhaException
