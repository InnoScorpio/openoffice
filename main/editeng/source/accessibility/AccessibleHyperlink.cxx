/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_editeng.hxx"

#include <com/sun/star/uno/Any.hxx>
#include <com/sun/star/uno/Reference.hxx>
#include <comphelper/accessiblekeybindinghelper.hxx>

#include "AccessibleHyperlink.hxx"
#include "editeng/unoedprx.hxx"
#include <editeng/flditem.hxx>
#include <vcl/keycodes.hxx>

using namespace ::com::sun::star;


//------------------------------------------------------------------------
//
// AccessibleHyperlink implementation
//
//------------------------------------------------------------------------

namespace accessibility
{

    AccessibleHyperlink::AccessibleHyperlink( SvxAccessibleTextAdapter& r, SvxFieldItem* p, sal_uInt16 nP, sal_uInt16 nR, sal_Int32 nStt, sal_Int32 nEnd, const ::rtl::OUString& rD ) 
    : rTA( r )
    { 
        pFld = p; 
        nPara = nP; 
        nRealIdx = nR; 
        nStartIdx = nStt; 
        nEndIdx = nEnd; 
        aDescription = rD;
    }
    
    AccessibleHyperlink::~AccessibleHyperlink()
    {
        delete pFld;
    }

    // XAccessibleAction
    sal_Int32 SAL_CALL AccessibleHyperlink::getAccessibleActionCount() throw (uno::RuntimeException)
    {
    	 return isValid() ? 1 : 0;
    }
    
    sal_Bool SAL_CALL AccessibleHyperlink::doAccessibleAction( sal_Int32 nIndex  ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
    	sal_Bool bRet = sal_False;
    	if ( isValid() && ( nIndex == 0 ) )
    	{
    	    rTA.FieldClicked( *pFld, nPara, nRealIdx );
    	    bRet = sal_True;
    	}
    	return bRet;
    }
    
    ::rtl::OUString  SAL_CALL AccessibleHyperlink::getAccessibleActionDescription( sal_Int32 nIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
    	::rtl::OUString aDesc;

    	if ( isValid() && ( nIndex == 0 ) )
    	    aDesc = aDescription;
    
    	return aDesc;
    }
    
    uno::Reference< ::com::sun::star::accessibility::XAccessibleKeyBinding > SAL_CALL AccessibleHyperlink::getAccessibleActionKeyBinding( sal_Int32 nIndex ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
    	uno::Reference< ::com::sun::star::accessibility::XAccessibleKeyBinding > xKeyBinding;
    
    	if( isValid() && ( nIndex == 0 ) )
    	{
    		::comphelper::OAccessibleKeyBindingHelper* pKeyBindingHelper = new ::comphelper::OAccessibleKeyBindingHelper();
    		xKeyBinding = pKeyBindingHelper;
    
            awt::KeyStroke aKeyStroke;
    		aKeyStroke.Modifiers = 0;
    		aKeyStroke.KeyCode = KEY_RETURN;
    		aKeyStroke.KeyChar = 0;
    		aKeyStroke.KeyFunc = 0;
    		pKeyBindingHelper->AddKeyBinding( aKeyStroke );
    	}
    
    	return xKeyBinding;
    }

    // XAccessibleHyperlink
    uno::Any SAL_CALL AccessibleHyperlink::getAccessibleActionAnchor( sal_Int32 /*nIndex*/ ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
    	return uno::Any();
    }
    
    uno::Any SAL_CALL AccessibleHyperlink::getAccessibleActionObject( sal_Int32 /*nIndex*/ ) throw (lang::IndexOutOfBoundsException, uno::RuntimeException)
    {
    	return uno::Any();
    }
    
    sal_Int32 SAL_CALL AccessibleHyperlink::getStartIndex() throw (uno::RuntimeException)
    {
    	return nStartIdx;
    }
    
    sal_Int32 SAL_CALL AccessibleHyperlink::getEndIndex() throw (uno::RuntimeException)
    {
    	return nEndIdx;
    }
    
    sal_Bool SAL_CALL AccessibleHyperlink::isValid(  ) throw (uno::RuntimeException)
    {
    	return rTA.IsValid();
    }

}  // end of namespace accessibility

//------------------------------------------------------------------------
