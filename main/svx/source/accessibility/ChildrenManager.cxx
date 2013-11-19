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
#include "precompiled_svx.hxx"
#include <svx/ChildrenManager.hxx>
#ifndef _SVX_ACCESSIBILITY_CHILDREN_MANAGER_IMPL_HXX
#include "ChildrenManagerImpl.hxx"
#endif
#include <svx/AccessibleShape.hxx>

using namespace ::com::sun::star;
using namespace	::com::sun::star::accessibility;
using ::com::sun::star::uno::Reference;

namespace accessibility {


//=====  AccessibleChildrenManager  ===========================================

ChildrenManager::ChildrenManager (
    const ::com::sun::star::uno::Reference<XAccessible>& rxParent,
    const ::com::sun::star::uno::Reference<drawing::XShapes>& rxShapeList,
    const AccessibleShapeTreeInfo& rShapeTreeInfo,
    AccessibleContextBase& rContext)
    : mpImpl (NULL)
{
    mpImpl = new ChildrenManagerImpl (rxParent, rxShapeList, rShapeTreeInfo, rContext);
    if (mpImpl != NULL)
        mpImpl->Init ();
    else
        throw uno::RuntimeException(
            ::rtl::OUString::createFromAscii(
				"ChildrenManager::ChildrenManager can't create implementation object"), NULL);
}




ChildrenManager::~ChildrenManager (void)
{
    if (mpImpl != NULL)
        mpImpl->dispose();

    // emtpy
    OSL_TRACE ("~ChildrenManager");
}




long ChildrenManager::GetChildCount (void) const throw ()
{
    OSL_ASSERT (mpImpl != NULL);
    return mpImpl->GetChildCount();
}




::com::sun::star::uno::Reference<XAccessible> ChildrenManager::GetChild (long nIndex)
    throw (::com::sun::star::uno::RuntimeException,
           ::com::sun::star::lang::IndexOutOfBoundsException)
{
    OSL_ASSERT (mpImpl != NULL);
    return mpImpl->GetChild (nIndex);
}

//IAccessibility2 Implementation 2009-----
Reference<XAccessible> ChildrenManager::GetChild (const Reference<drawing::XShape>& xShape)
    throw (::com::sun::star::uno::RuntimeException)
{
    OSL_ASSERT (mpImpl != NULL);
    return mpImpl->GetChild (xShape);
}

::com::sun::star::uno::Reference<
        ::com::sun::star::drawing::XShape> ChildrenManager::GetChildShape(long nIndex)
    throw (::com::sun::star::uno::RuntimeException)
{
	OSL_ASSERT (mpImpl != NULL);
    return mpImpl->GetChildShape(nIndex);
}
//-----IAccessibility2 Implementation 2009


void ChildrenManager::Update (bool bCreateNewObjectsOnDemand)
{
    OSL_ASSERT (mpImpl != NULL);
    mpImpl->Update (bCreateNewObjectsOnDemand);
}




void ChildrenManager::SetShapeList (const ::com::sun::star::uno::Reference<
    ::com::sun::star::drawing::XShapes>& xShapeList)
{
    OSL_ASSERT (mpImpl != NULL);
    mpImpl->SetShapeList (xShapeList);
}




void ChildrenManager::AddAccessibleShape (std::auto_ptr<AccessibleShape> pShape)
{
    OSL_ASSERT (mpImpl != NULL);
    mpImpl->AddAccessibleShape (pShape);
}




void ChildrenManager::ClearAccessibleShapeList (void)
{
    OSL_ASSERT (mpImpl != NULL);
    mpImpl->ClearAccessibleShapeList ();
}




void ChildrenManager::SetInfo (AccessibleShapeTreeInfo& rShapeTreeInfo)
{
    OSL_ASSERT (mpImpl != NULL);
    mpImpl->SetInfo (rShapeTreeInfo);
}




void ChildrenManager::UpdateSelection (void)
{
    OSL_ASSERT (mpImpl != NULL);
    mpImpl->UpdateSelection ();
}




bool ChildrenManager::HasFocus (void)
{
    OSL_ASSERT (mpImpl != NULL);
    return mpImpl->HasFocus ();
}




void ChildrenManager::RemoveFocus (void)
{
    OSL_ASSERT (mpImpl != NULL);
    mpImpl->RemoveFocus ();
}




//=====  IAccessibleViewForwarderListener  ====================================
void ChildrenManager::ViewForwarderChanged (ChangeType aChangeType,
        const IAccessibleViewForwarder* pViewForwarder)
{
    OSL_ASSERT (mpImpl != NULL);
    mpImpl->ViewForwarderChanged (aChangeType, pViewForwarder);
}



} // end of namespace accessibility

