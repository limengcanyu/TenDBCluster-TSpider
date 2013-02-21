/* Copyright (C) Olivier Bertrand 2004 - 2013

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*************** Mycat CC Program Source Code File (.CC) ***************/
/* PROGRAM NAME: MYCAT                                                 */
/* -------------                                                       */
/*  Version 1.4                                                        */
/*                                                                     */
/*  Author: Olivier Bertrand                       2012 - 2013         */
/*                                                                     */
/* WHAT THIS PROGRAM DOES:                                             */
/* -----------------------                                             */
/*  This program are the DB description related routines.              */
/***********************************************************************/

/***********************************************************************/
/*  Include relevant MariaDB header file.                              */
/***********************************************************************/
#if defined(WIN32)
//#include <windows.h>
//#include <sqlext.h>
#elif defined(UNIX)
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#endif
#define DONT_DEFINE_VOID
//#include <mysql/plugin.h>
#include "handler.h"
#undef  OFFSET

/***********************************************************************/
/*  Include application header files                                   */
/*                                                                     */
/*  global.h     is header containing all global declarations.         */
/*  plgdbsem.h   is header containing DB application declarations.     */
/*  tabdos.h     is header containing TDBDOS classes declarations.     */
/*  MYCAT.h      is header containing DB description declarations.     */
/***********************************************************************/
#if defined(UNIX)
#include "osutil.h"
#endif   // UNIX
#include "global.h"
#include "plgdbsem.h"
#include "reldef.h"
#include "tabcol.h"
#include "xtable.h"
#include "filamtxt.h"
#include "tabdos.h"
#include "tabfmt.h"
#include "tabvct.h"
#include "tabsys.h"
#if defined(WIN32)
#include "tabmac.h"
#include "tabwmi.h"
#endif   // WIN32
#include "tabtbl.h"
#if defined(XML_SUPPORT)
#include "tabxml.h"
#endif   // XML_SUPPORT
#include "tabmul.h"
#if defined(MYSQL_SUPPORT)
#include "tabmysql.h"
#endif   // MYSQL_SUPPORT
#if defined(ODBC_SUPPORT)
#define NODBC
#include "tabodbc.h"
#endif   // ODBC_SUPPORT
#if defined(PIVOT_SUPPORT)
#include "tabpivot.h"
#endif   // PIVOT_SUPPORT
#include "ha_connect.h"
#include "mycat.h"

/***********************************************************************/
/*  Extern static variables.                                           */
/***********************************************************************/
#if defined(WIN32)
extern "C" HINSTANCE s_hModule;           // Saved module handle
#endif  // !WIN32

extern int xtrace;

/***********************************************************************/
/*  Get a unique enum table type ID.                                   */
/***********************************************************************/
TABTYPE GetTypeID(const char *type)
  {
  return (!type) ? TAB_UNDEF                      
                 : (!stricmp(type, "DOS"))   ? TAB_DOS
                 : (!stricmp(type, "FIX"))   ? TAB_FIX
                 : (!stricmp(type, "BIN"))   ? TAB_BIN
	               : (!stricmp(type, "CSV"))   ? TAB_CSV
                 : (!stricmp(type, "FMT"))   ? TAB_FMT
                 : (!stricmp(type, "DBF"))   ? TAB_DBF
                 : (!stricmp(type, "XML"))   ? TAB_XML
                 : (!stricmp(type, "INI"))   ? TAB_INI
                 : (!stricmp(type, "VEC"))   ? TAB_VEC
                 : (!stricmp(type, "ODBC"))  ? TAB_ODBC
                 : (!stricmp(type, "MYSQL")) ? TAB_MYSQL
                 : (!stricmp(type, "DIR"))   ? TAB_DIR
	               : (!stricmp(type, "MAC"))   ? TAB_MAC
	               : (!stricmp(type, "WMI"))   ? TAB_WMI
	               : (!stricmp(type, "TBL"))   ? TAB_TBL
                 : (!stricmp(type, "OEM"))   ? TAB_OEM : TAB_NIY;
  } // end of GetTypeID

/***********************************************************************/
/*  Get a unique enum catalog function ID.                             */
/***********************************************************************/
uint GetFuncID(const char *func)
  {
  uint fnc;

  if (!func)
    fnc= FNC_NO;
  else if (!strnicmp(func, "col", 3))
    fnc= FNC_COL;
  else if (!strnicmp(func, "tab", 3))
    fnc= FNC_TABLE;
  else if (!stricmp(func, "dsn") ||
           !strnicmp(func, "datasource", 10) ||
           !strnicmp(func, "source", 6) ||
           !strnicmp(func, "sqldatasource", 13))
    fnc= FNC_DSN;
  else if (!strnicmp(func, "driver", 6) ||
           !strnicmp(func, "sqldriver", 9))
    fnc= FNC_DRIVER;
  else
    fnc= FNC_NIY;

  return fnc;
  } // end of GetFuncID

/* ------------------------- Class CATALOG --------------------------- */

/***********************************************************************/
/*  CATALOG Constructor.                                               */
/***********************************************************************/
CATALOG::CATALOG(void)
  {
  To_Desc= NULL;
//*DescFile= '\0';
#if defined(WIN32)
  DataPath= ".\\";
#else   // !WIN32
  DataPath= "./";
#endif  // !WIN32
  Descp= NULL;
//memset(&DescArea, 0, sizeof(AREADEF));
  memset(&Ctb, 0, sizeof(CURTAB));
  Cbuf= NULL;
  Cblen= 0;
	DefHuge= false;
  } // end of CATALOG constructor

/* -------------------------- Class MYCAT ---------------------------- */

/***********************************************************************/
/*  MYCAT Constructor.                                                 */
/***********************************************************************/
MYCAT::MYCAT(PHC hc) : CATALOG()
  {
	Hc= hc;
  To_Desc= NULL;
  DefHuge= false;
	SepIndex= true;			// Temporay until we can store offet and size
  } // end of MYCAT constructor

/***********************************************************************/
/*  When using volatile storage, reset values pointing to Sarea.       */
/***********************************************************************/
void MYCAT::Reset(void)
  {
  To_Desc= NULL;
  } // end of Reset

/***********************************************************************/
/*  This function sets the current database path.                      */
/***********************************************************************/
void MYCAT::SetPath(PGLOBAL g, LPCSTR *datapath, const char *path)
	{
	if (path) {
		size_t len= strlen(path) + (*path != '.' ? 4 : 1);
		char  *buf= (char*)PlugSubAlloc(g, NULL, len);

		if (*path != '.') {
#if defined(WIN32)
			char *s= "\\";
#else   // !WIN32
			char *s= "/";
#endif  // !WIN32
			strcat(strcat(strcat(strcpy(buf, "."), s), path), s);
		} else
			strcpy(buf, path);

		*datapath= buf;
		} // endif path

	} // end of SetDataPath

/***********************************************************************/
/*  This function sets an integer MYCAT information.                   */
/***********************************************************************/
bool MYCAT::SetIntCatInfo(LPCSTR name, PSZ what, int n)
	{
	return Hc->SetIntegerOption(what, n);
	} // end of SetIntCatInfo

/***********************************************************************/
/*  This function returns integer MYCAT information.                   */
/***********************************************************************/
int MYCAT::GetIntCatInfo(LPCSTR name, PSZ what, int idef)
	{
	int n= Hc->GetIntegerOption(what);

	return (n == NO_IVAL) ? idef : n;
	} // end of GetIntCatInfo

/***********************************************************************/
/*  This function returns Boolean MYCAT information.                   */
/***********************************************************************/
bool MYCAT::GetBoolCatInfo(LPCSTR name, PSZ what, bool bdef)
	{
	bool b= Hc->GetBooleanOption(what, bdef);

	return b;
	} // end of GetBoolCatInfo

/***********************************************************************/
/*  This function returns size catalog information.                    */
/***********************************************************************/
int MYCAT::GetSizeCatInfo(LPCSTR name, PSZ what, PSZ sdef)
	{
	char * s, c;
  int  i, n= 0;

	if (!(s= Hc->GetStringOption(what)))
		s= sdef;

	if ((i= sscanf(s, " %d %c ", &n, &c)) == 2)
    switch (toupper(c)) {
      case 'M':
        n *= 1024;
      case 'K':
        n *= 1024;
      } // endswitch c

  return n;
} // end of GetSizeCatInfo

/***********************************************************************/
/*  This function sets char MYCAT information in buf.                  */
/***********************************************************************/
int MYCAT::GetCharCatInfo(LPCSTR name, PSZ what, 
																			 PSZ sdef, char *buf, int size)
	{
	char *s= Hc->GetStringOption(what);

	strncpy(buf, ((s) ? s : sdef), size);
	return size;
	} // end of GetCharCatInfo

/***********************************************************************/
/*  This function returns string MYCAT information.                    */
/*  Default parameter is "*" to get the handler default.               */
/***********************************************************************/
char *MYCAT::GetStringCatInfo(PGLOBAL g, PSZ name, PSZ what, PSZ sdef)
	{
	char *sval, *s= Hc->GetStringOption(what, sdef);
	
	if (s) {
		sval= (char*)PlugSubAlloc(g, NULL, strlen(s) + 1);
		strcpy(sval, s);
	} else
		sval = NULL;

	return sval;
	}	// end of GetStringCatInfo

/***********************************************************************/
/*  This function returns column MYCAT information.                    */
/***********************************************************************/
int MYCAT::GetColCatInfo(PGLOBAL g, PTABDEF defp)
	{
	char		*type= GetStringCatInfo(g, NULL, "Type", "DOS");
	int      i, loff, poff, nof, nlg;
	void    *field= NULL;
  TABTYPE  tc;
  PCOLDEF  cdp, lcdp= NULL, tocols= NULL;
	PCOLINFO pcf= (PCOLINFO)PlugSubAlloc(g, NULL, sizeof(COLINFO));

  // Get a unique char identifier for type
  tc= (defp->Catfunc == FNC_NO) ? GetTypeID(type) : TAB_CATLG;

  // Take care of the column definitions
	i= poff= nof= nlg= 0;

	// Offsets of HTML and DIR tables start from 0, DBF at 1
	loff= (tc == TAB_DBF) ? 1 : (tc == TAB_XML || tc == TAB_DIR) ? -1 : 0; 

  while (true) {
		// Default Offset depends on table type
		switch (tc) {
      case TAB_DOS:
      case TAB_FIX:
      case TAB_BIN:
      case TAB_VEC:
      case TAB_DBF:
        poff= loff + nof;				 // Default next offset
				nlg= max(nlg, poff);		 // Default lrecl
        break;
      case TAB_CSV:
      case TAB_FMT:
				nlg+= nof;
      case TAB_DIR:
      case TAB_XML:
        poff= loff + 1;
        break;
      case TAB_INI:
      case TAB_MAC:
      case TAB_TBL:
      case TAB_OEM:
        poff = 0;      // Offset represents an independant flag
        break;
      default:         // VCT PLG ODBC MYSQL WMI...
        poff = 0;			 // NA
        break;
			} // endswitch tc

		do {
			field= Hc->GetColumnOption(field, pcf);
			} while (field && (*pcf->Name =='*' /*|| pcf->Flags & U_VIRTUAL*/));

		if (tc == TAB_DBF && pcf->Type == TYPE_DATE && !pcf->Datefmt) {
			// DBF date format defaults to 'YYYMMDD'
			pcf->Datefmt= "YYYYMMDD";
			pcf->Length= 8;
			} // endif tc

		if (!field)
			break;

    // Allocate the column description block
    cdp= new(g) COLDEF;

    if ((nof= cdp->Define(g, NULL, pcf, poff)) < 0)
      return -1;						 // Error, probably unhandled type
		else if (nof)
			loff= cdp->GetOffset();

		switch (tc) {
			case TAB_VEC:
				cdp->SetOffset(0);		 // Not to have shift
			case TAB_BIN:
				// BIN/VEC are packed by default
				if (nof)
					// Field width is the internal representation width
					// that can also depend on the column format
					switch (cdp->Fmt ? *cdp->Fmt : 'X') {
						case 'C':         break;
						case 'R':
						case 'F':
						case 'L':
						case 'I':	nof= 4; break;
						case 'D':	nof= 8; break;
						case 'S':	nof= 2; break;
						case 'T':	nof= 1; break;
						default:  nof= cdp->Clen;
						} // endswitch Fmt

      default:
				break;
			} // endswitch tc

		if (lcdp)
	    lcdp->SetNext(cdp);
		else
			tocols= cdp;

		lcdp= cdp;
    i++;
    } // endwhile

  // Degree is the the number of defined columns (informational)
  if (i != defp->GetDegree())
    defp->SetDegree(i);

	if (defp->GetDefType() == TYPE_AM_DOS) {
		int			ending, recln= 0;
		PDOSDEF ddp= (PDOSDEF)defp;

		// Was commented because sometimes ending is 0 even when
		// not specified (for instance if quoted is specified)
//	if ((ending= Hc->GetIntegerOption("Ending")) < 0) {
		if ((ending= Hc->GetIntegerOption("Ending")) <= 0) {
#if defined(WIN32)
			ending= 2;
#else
			ending= 1;
#endif
			Hc->SetIntegerOption("Ending", ending);
			} // endif ending

		// Calculate the default record size
		switch (tc) {
      case TAB_FIX:
        recln= nlg + ending;     // + length of line ending
        break;
      case TAB_BIN:
      case TAB_VEC:
        recln= nlg;
	
//      if ((k= (pak < 0) ? 8 : pak) > 1)
          // See above for detailed comment
          // Round up lrecl to multiple of 8 or pak
//        recln= ((recln + k - 1) / k) * k;
	
        break;
      case TAB_DOS:
      case TAB_DBF:
        recln= nlg;
        break;
      case TAB_CSV:
      case TAB_FMT:
        // The number of separators (assuming an extra one can exist)
//      recln= poff * ((qotd) ? 3 : 1);	 to be investigated
				recln= nlg + poff * 3;     // To be safe
      default:
        break;
      } // endswitch tc

		// lrecl must be at least recln to avoid buffer overflow
		recln= max(recln, Hc->GetIntegerOption("Lrecl"));
		Hc->SetIntegerOption("Lrecl", recln);
		ddp->SetLrecl(recln);
		} // endif Lrecl

	// Attach the column definition to the tabdef
	defp->SetCols(tocols);
	return poff;
	} // end of GetColCatInfo

/***********************************************************************/
/*  GetIndexInfo: retrieve index description from the table structure. */
/***********************************************************************/
bool MYCAT::GetIndexInfo(PGLOBAL g, PTABDEF defp)
  {
  PIXDEF xdp, pxd= NULL, toidx= NULL;

  // Now take care of the index definitions
	for (int n= 0; ; n++) {
		if (xtrace)
			printf("Getting index %d info\n", n + 1);

		if (!(xdp= Hc->GetIndexInfo(n)))
			break;

    if (pxd)
			pxd->SetNext(xdp);
		else
			toidx= xdp;

    pxd= xdp;
    } // endfor n

  // All is correct, attach new index(es)
  defp->SetIndx(toidx);
	return false;
  } // end of GetIndexInfo

/***********************************************************************/
/*  GetTableDesc: retrieve a table descriptor.                         */
/*  Look for a table descriptor matching the name and type. If found   */
/*  in storage, return a pointer to it, else look in the XDB file. If  */
/*  found, make and add the descriptor and return a pointer to it.     */
/***********************************************************************/
PRELDEF MYCAT::GetTableDesc(PGLOBAL g, LPCSTR name,
                                       LPCSTR am, PRELDEF *prp)
  {
	LPCSTR  type;

	if (xtrace)
		printf("GetTableDesc: name=%s am=%s\n", name, SVP(am));

  // Firstly check whether this table descriptor is in memory
  if (To_Desc)
		return  To_Desc;

	// Here get the type of this table
	if (!(type= Hc->GetStringOption("Type")))
		type= "DOS";

  return MakeTableDesc(g, name, type);
  } // end of GetTableDesc

/***********************************************************************/
/*  MakeTableDesc: make a table/view description.                      */
/*  Note: caller must check if name already exists before calling it.  */
/***********************************************************************/
PRELDEF MYCAT::MakeTableDesc(PGLOBAL g, LPCSTR name, LPCSTR am)
  {
  TABTYPE tc;
  PRELDEF tdp= NULL;

	if (xtrace)
		printf("MakeTableDesc: name=%s am=%s\n", name, SVP(am));

  /*********************************************************************/
  /*  Get a unique enum identifier for types.                          */
  /*********************************************************************/
  tc= GetTypeID(am);

  switch (tc) {
    case TAB_FIX:
    case TAB_BIN:
    case TAB_DBF:
    case TAB_DOS: tdp= new(g) DOSDEF;   break;
    case TAB_CSV:
    case TAB_FMT: tdp= new(g) CSVDEF;   break;
    case TAB_INI: tdp= new(g) INIDEF;   break;
    case TAB_DIR: tdp= new(g) DIRDEF;   break;
#if defined(XML_SUPPORT)
    case TAB_XML: tdp= new(g) XMLDEF;   break;
#endif   // XML_SUPPORT
    case TAB_VEC: tdp= new(g) VCTDEF;   break;
#if defined(ODBC_SUPPORT)
    case TAB_ODBC: tdp= new(g) ODBCDEF; break;
#endif   // ODBC_SUPPORT
#if defined(WIN32)
    case TAB_MAC: tdp= new(g) MACDEF;   break;
    case TAB_WMI: tdp= new(g) WMIDEF;   break;
#endif   // WIN32
    case TAB_OEM: tdp= new(g) OEMDEF;   break;
	  case TAB_TBL: tdp= new(g) TBLDEF;   break;
#if defined(MYSQL_SUPPORT)
		case TAB_MYSQL: tdp= new(g) MYSQLDEF;	break;
#endif   // MYSQL_SUPPORT
//#if defined(PIVOT_SUPPORT)
//  case TAB_PIVOT: tdp= new(g) PIVOTDEF; break;
//#endif   // PIVOT_SUPPORT
    default:
      sprintf(g->Message, MSG(BAD_TABLE_TYPE), am, name);
    } // endswitch

  // Do make the table/view definition from XDB file information
  if (tdp && tdp->Define(g, this, name, am))
    tdp= NULL;

  return tdp;
  } // end of MakeTableDesc

/***********************************************************************/
/*  Initialize a Table Description Block construction.                 */
/***********************************************************************/
PTDB MYCAT::GetTable(PGLOBAL g, PTABLE tablep, MODE mode)
  {
  PRELDEF tdp;
  PTDB    tdbp= NULL;
  LPCSTR  name= tablep->GetName();

	if (xtrace)
		printf("GetTableDB: name=%s\n", name);

  // Look for the description of the requested table
  tdp= GetTableDesc(g, name, NULL);

  if (tdp) {
		if (xtrace)
			printf("tdb=%p type=%s\n", tdp, tdp->GetType());

		if (tablep->GetQualifier())
			SetPath(g, &tdp->Database, tablep->GetQualifier());
		
    tdbp= tdp->GetTable(g, mode);
		} // endif tdp

  if (tdbp) {
		if (xtrace)
			printf("tdbp=%p name=%s amtype=%d\n", tdbp, tdbp->GetName(),
																						tdbp->GetAmType());
    tablep->SetTo_Tdb(tdbp);
    tdbp->SetTable(tablep);
    } // endif tdbp

  return (tdbp);
  } // end of GetTable

/***********************************************************************/
/*  ClearDB: Terminates Database usage.                                */
/***********************************************************************/
void MYCAT::ClearDB(PGLOBAL g)
  {
  To_Desc= NULL;
  } // end of ClearDB

/* ------------------------ End of MYCAT --------------------------- */