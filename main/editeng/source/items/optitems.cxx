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
#include <tools/shl.hxx>
#include <tools/resid.hxx>
#include <tools/stream.hxx>
#include <com/sun/star/linguistic2/XSpellChecker1.hpp>

#include <editeng/optitems.hxx>
#include <editeng/eerdll.hxx>
#include <editeng/editrids.hrc>
#include <editeng/eerdll.hxx>
#include <editeng/editrids.hrc>

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::linguistic2;

// STATIC DATA -----------------------------------------------------------

TYPEINIT1(SfxSpellCheckItem, SfxPoolItem);
TYPEINIT1(SfxHyphenRegionItem, SfxPoolItem);

// class SfxSpellCheckItem -----------------------------------------------

SfxSpellCheckItem::SfxSpellCheckItem
(
	Reference< XSpellChecker1 > &xChecker,
	sal_uInt16 _nWhich
) :

	SfxPoolItem( _nWhich )
{
	xSpellCheck = xChecker;
}

// -----------------------------------------------------------------------

SfxSpellCheckItem::SfxSpellCheckItem( const SfxSpellCheckItem& rItem ) :

	SfxPoolItem( rItem ),
	xSpellCheck( rItem.GetXSpellChecker() )
{
}

//------------------------------------------------------------------------

SfxItemPresentation SfxSpellCheckItem::GetPresentation
(
	SfxItemPresentation	ePres,
	SfxMapUnit			,
	SfxMapUnit			,
	String&				rText,
    const IntlWrapper*
)	const
{
	switch ( ePres )
	{
		case SFX_ITEM_PRESENTATION_NONE:
			rText.Erase();
			return SFX_ITEM_PRESENTATION_NONE;

		case SFX_ITEM_PRESENTATION_NAMELESS:
		case SFX_ITEM_PRESENTATION_COMPLETE:
		{
			return ePres;
		}
		default:
			return SFX_ITEM_PRESENTATION_NONE;
	}
}

// -----------------------------------------------------------------------

SfxPoolItem* SfxSpellCheckItem::Clone( SfxItemPool* ) const
{
	return new SfxSpellCheckItem( *this );
}

// -----------------------------------------------------------------------

int SfxSpellCheckItem::operator==( const SfxPoolItem& rItem ) const
{
	DBG_ASSERT( SfxPoolItem::operator==(rItem), "unequal types" );
	return ( xSpellCheck == ( (const SfxSpellCheckItem& )rItem ).GetXSpellChecker() );
}

// class SfxHyphenRegionItem -----------------------------------------------

SfxHyphenRegionItem::SfxHyphenRegionItem( const sal_uInt16 nId ) :

	SfxPoolItem( nId )
{
	nMinLead = nMinTrail = 0;
}

// -----------------------------------------------------------------------

SfxHyphenRegionItem::SfxHyphenRegionItem( const SfxHyphenRegionItem& rItem ) :

	SfxPoolItem	( rItem ),

	nMinLead	( rItem.GetMinLead() ),
	nMinTrail	( rItem.GetMinTrail() )
{
}

// -----------------------------------------------------------------------

int SfxHyphenRegionItem::operator==( const SfxPoolItem& rAttr ) const
{
	DBG_ASSERT( SfxPoolItem::operator==(rAttr), "unequal types" );

	return ( ( ( (SfxHyphenRegionItem&)rAttr ).nMinLead == nMinLead ) &&
			 ( ( (SfxHyphenRegionItem&)rAttr ).nMinTrail == nMinTrail ) );
}

// -----------------------------------------------------------------------

SfxPoolItem* SfxHyphenRegionItem::Clone( SfxItemPool* ) const
{
	return new SfxHyphenRegionItem( *this );
}

//------------------------------------------------------------------------

SfxItemPresentation SfxHyphenRegionItem::GetPresentation
(
	SfxItemPresentation ePres,
	SfxMapUnit			,
	SfxMapUnit			,
	String&				rText,
    const IntlWrapper*
)	const
{
	switch ( ePres )
	{
		case SFX_ITEM_PRESENTATION_NONE:
			rText.Erase();
			return SFX_ITEM_PRESENTATION_NONE;

		case SFX_ITEM_PRESENTATION_NAMELESS:
		case SFX_ITEM_PRESENTATION_COMPLETE:
		{
			rText += String::CreateFromInt32( nMinLead );
			rText += String( EditResId( RID_SVXITEMS_HYPHEN_MINLEAD ) );
			rText += ',';
			rText += String::CreateFromInt32( nMinTrail );
			rText += String( EditResId( RID_SVXITEMS_HYPHEN_MINTRAIL ) );
			return ePres;
		}
		default:
			return SFX_ITEM_PRESENTATION_NONE;
	}
}

// -----------------------------------------------------------------------

SfxPoolItem* SfxHyphenRegionItem::Create(SvStream& rStrm, sal_uInt16 ) const
{
	sal_uInt8 _nMinLead, _nMinTrail;
	rStrm >> _nMinLead >> _nMinTrail;
	SfxHyphenRegionItem* pAttr = new SfxHyphenRegionItem( Which() );
	pAttr->GetMinLead() = _nMinLead;
	pAttr->GetMinTrail() = _nMinTrail;
	return pAttr;
}

// -----------------------------------------------------------------------

SvStream& SfxHyphenRegionItem::Store( SvStream& rStrm, sal_uInt16 ) const
{
	rStrm << (sal_uInt8) GetMinLead()
		  << (sal_uInt8) GetMinTrail();
	return rStrm;
}


