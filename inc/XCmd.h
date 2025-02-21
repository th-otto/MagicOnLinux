/*
 * Copyright (C) 1990-2025 Andreas Kromke, andreas.kromke@gmail.com
 *
 * This program is free software; you can redistribute it or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
*
* Management of plugins
*
*/

#ifndef _XCMD_H_INCLUDED
#define _XCMD_H_INCLUDED

// system headers
// program headers
#include "Globals.h"
#include "osd_cpu.h"
#include "XCmdDefs.h"
#include "MagicMacXPluginTypes.h"

// switches

#if TARGET_RT_MAC_MACHO
struct GlueCode
{
	GlueCode *pNext;
//	CFragConnectionID id;		// zugeh�rige Bibliothek
	void *p;				// Zeiger auf MachO-Glue-Code
};

enum enRunTimeFormat
{
	eUnused = 0,
	eMachO = 1,
	ePEF = 2
};

#define MAX_PLUGINS	128		// maximum of plugins
#endif

class CXCmd
{
   public:
	// Konstruktor
	CXCmd();
	// Destruktor
	~CXCmd();
	// initialisieren
	int Init(void);
	// XCmd laden
	OSErr LoadFile(const char *path, uint64_t *pConnectionId);
	OSErr LoadLibrary(const char *libName, uint64_t *pConnectionId);
	OSErr LoadPlugin(
			const char *pPath,
			const char *SearchPath,
			uint64_t *pPlugInRef,
			MagicMacXPluginInterfaceStruct **ppInterface);
	// die zentrale Kommandofunktion
	INT32 Command(UINT32 params, uint8_t *AdrOffset68k);

   private:
#if TARGET_RT_MAC_MACHO
	struct tsLoadedPlugin
	{
		enRunTimeFormat RunTimeFormat;
		// used for CFM/PEF
		CFragConnectionID ConnID;
		GlueCode *pGlueList;
		// used for MachO
		CFPlugInRef PluginRef;
		MagicMacXPluginInterfaceStruct *pInterface;
	};
	static tsLoadedPlugin s_Plugins[MAX_PLUGINS];
#endif
	OSErr OnCommandLoadLibrary(
				const char *szLibName,
				bool bIsPath,
				UINT32 *pDescriptor,
				INT32 *pNumOfSymbols);
	OSErr OnCommandUnloadLibrary(uint32_t XCmdDescriptor);
	OSErr OnCommandFindSymbol(
				uint32_t XCmdDescriptor,
				char *pSymName,
				uint32_t SymNumber,
				unsigned char *pSymClass,
				uint32_t *pSymbolAddress
				);
   	OSErr Preload(void);
   	void InitXCMD(uint64_t connectionId);
   	static XCmdCallbackFunctionProcType Callback;

   	strXCmdInfo m_XCmdInfo;		// Info-Struktur geht an PEF/CFM PlugIns
   	struct XCmdInfo m_XCmdPlugInInfo;	// Info-Struktur geht an MachO PlugIns
	const char *m_XCMDFolderSpec;
#if TARGET_RT_MAC_MACHO
	void *NewGlue(void *pCFragPtr, uint32_t XCmdDescriptor, CFragSymbolClass symclass);
#endif
};
#endif
