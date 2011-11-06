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

// include ---------------------------------------------------------------
#include <tools/list.hxx>
#include <vcl/outdev.hxx>
#include <editeng/editrids.hrc>
#include <unotools/intlwrapper.hxx>
#include <unotools/localedatawrapper.hxx>
#include <editeng/itemtype.hxx>
#include <editeng/eerdll.hxx>

// -----------------------------------------------------------------------

XubString GetMetricText( long nVal, SfxMapUnit eSrcUnit, SfxMapUnit eDestUnit, const IntlWrapper* pIntl    )
{
	sal_Bool bNeg = sal_False;
	long nRet = 0;
	XubString sRet;

	if ( nVal < 0 )
	{
		bNeg = sal_True;
		nVal *= -1;
	}

	switch ( eDestUnit )
	{
		case SFX_MAPUNIT_100TH_MM:
		case SFX_MAPUNIT_10TH_MM:
		case SFX_MAPUNIT_MM:
		case SFX_MAPUNIT_CM:
		{
			nRet = (long)OutputDevice::LogicToLogic(
				nVal, (MapUnit)eSrcUnit, (MapUnit)SFX_MAPUNIT_100TH_MM );

			switch ( eDestUnit )
			{
				case SFX_MAPUNIT_100TH_MM:  nRet *= 1000; break;
				case SFX_MAPUNIT_10TH_MM:   nRet *= 100; break;
				case SFX_MAPUNIT_MM:        nRet *= 10; break;
                default: ;//prevent warning
            }
			break;
		}

		case SFX_MAPUNIT_1000TH_INCH:
		case SFX_MAPUNIT_100TH_INCH:
		case SFX_MAPUNIT_10TH_INCH:
		case SFX_MAPUNIT_INCH:
		{
			nRet = (long)OutputDevice::LogicToLogic(
				nVal, (MapUnit)eSrcUnit, (MapUnit)SFX_MAPUNIT_1000TH_INCH );

			switch ( eDestUnit )
			{
				case SFX_MAPUNIT_1000TH_INCH:   nRet *= 1000; break;
				case SFX_MAPUNIT_100TH_INCH:    nRet *= 100; break;
				case SFX_MAPUNIT_10TH_INCH:     nRet *= 10; break;
                default: ;//prevent warning
            }
			break;
		}

		case SFX_MAPUNIT_POINT:
		case SFX_MAPUNIT_TWIP:
		case SFX_MAPUNIT_PIXEL:
			return String::CreateFromInt32( (long)OutputDevice::LogicToLogic(
						nVal, (MapUnit)eSrcUnit, (MapUnit)eDestUnit ));

		default:
			DBG_ERROR( "not supported mapunit" );
			return sRet;
	}

	if ( SFX_MAPUNIT_CM == eDestUnit || SFX_MAPUNIT_INCH == eDestUnit )
	{
		long nMod = nRet % 10;

		if ( nMod > 4 )
			nRet += 10 - nMod;
		else if ( nMod > 0 )
			nRet -= nMod;
	}

	if ( bNeg )
		sRet += sal_Unicode('-');

	long nDiff = 1000;
	for( int nDigits = 4; nDigits; --nDigits, nDiff /= 10 )
	{
		if ( nRet < nDiff )
			sRet += sal_Unicode('0');
		else
			sRet += String::CreateFromInt32( nRet / nDiff );
		nRet %= nDiff;
		if( 4 == nDigits )
		{
//            DBG_ASSERT(pIntl, "no IntlWrapper* set")
            if(pIntl)
                sRet += pIntl->getLocaleData()->getNumDecimalSep();
            else
                sRet += ',';
			if( !nRet )
			{
				sRet += sal_Unicode('0');
				break;
			}
		}
		else if( !nRet )
			break;
	}
	return sRet;
}

// -----------------------------------------------------------------------

XubString GetSvxString( sal_uInt16 nId )
{
	return EE_RESSTR( nId );
}

#ifndef SVX_LIGHT

// -----------------------------------------------------------------------

XubString GetColorString( const Color& rCol )
{
	XubString sStr;

	FASTBOOL bFound = sal_False;
	ColorData nColData =
		RGB_COLORDATA( rCol.GetRed(), rCol.GetGreen(), rCol.GetBlue() );
	sal_uInt16 nColor = 0, nColCount = 16;

	static ColorData aColAry[] = {
		COL_BLACK, COL_BLUE, COL_GREEN, COL_CYAN,
		COL_RED, COL_MAGENTA, COL_BROWN, COL_GRAY,
		COL_LIGHTGRAY, COL_LIGHTBLUE, COL_LIGHTGREEN, COL_LIGHTCYAN,
		COL_LIGHTRED, COL_LIGHTMAGENTA, COL_YELLOW, COL_WHITE };

	while ( !bFound && nColor < nColCount )
	{
		if ( aColAry[nColor] == nColData )
			bFound = sal_True;
		else
			nColor++;
	}

	if ( nColor < nColCount )
		sStr = EE_RESSTR( RID_SVXITEMS_COLOR_BEGIN + nColor + 1 );

	if ( !sStr.Len() )
	{
		sStr.AppendAscii(RTL_CONSTASCII_STRINGPARAM( "RGB" ));
		sStr += sal_Unicode('(');
		sStr += String::CreateFromInt32( rCol.GetRed() );
		sStr += cpDelim;
		sStr += String::CreateFromInt32( rCol.GetGreen() );
		sStr += cpDelim;
		sStr += String::CreateFromInt32( rCol.GetBlue() );
		sStr += sal_Unicode(')');
	}
	return sStr;
}

#endif

// -----------------------------------------------------------------------

sal_uInt16 GetMetricId( SfxMapUnit eUnit )
{
	sal_uInt16 nId = RID_SVXITEMS_METRIC_MM;

	switch ( eUnit )
	{
		case SFX_MAPUNIT_100TH_MM:
		case SFX_MAPUNIT_10TH_MM:
		case SFX_MAPUNIT_MM:
			nId = RID_SVXITEMS_METRIC_MM;
			break;

		case SFX_MAPUNIT_CM:
			nId = RID_SVXITEMS_METRIC_CM;
			break;

		case SFX_MAPUNIT_1000TH_INCH:
		case SFX_MAPUNIT_100TH_INCH:
		case SFX_MAPUNIT_10TH_INCH:
		case SFX_MAPUNIT_INCH:
			nId = RID_SVXITEMS_METRIC_INCH;
			break;

		case SFX_MAPUNIT_POINT:
			nId = RID_SVXITEMS_METRIC_POINT;
			break;

		case SFX_MAPUNIT_TWIP:
			nId = RID_SVXITEMS_METRIC_TWIP;
			break;

		case SFX_MAPUNIT_PIXEL:
			nId = RID_SVXITEMS_METRIC_PIXEL;
			break;

		default:
			DBG_ERROR( "not supported mapunit" );
	}
	return nId;
}


