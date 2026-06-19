
#include "Version.h"

#ifndef AETHER_RC_INTERNAL_NAME
#define AETHER_RC_INTERNAL_NAME      "Aether"
#endif
#ifndef AETHER_RC_ORIGINAL_FILENAME
#define AETHER_RC_ORIGINAL_FILENAME  "Aether.exe"
#endif
#ifndef AETHER_RC_FILE_DESCRIPTION
#define AETHER_RC_FILE_DESCRIPTION   "Aether tablet driver"
#endif

VS_VERSION_INFO VERSIONINFO
 FILEVERSION    AETHER_VERSION_MAJOR,AETHER_VERSION_MINOR,AETHER_VERSION_PATCH,AETHER_VERSION_BUILD
 PRODUCTVERSION AETHER_VERSION_MAJOR,AETHER_VERSION_MINOR,AETHER_VERSION_PATCH,AETHER_VERSION_BUILD
 FILEFLAGSMASK  0x3fL
#ifdef _DEBUG
 FILEFLAGS      0x1L
#else
 FILEFLAGS      0x0L
#endif
 FILEOS         0x40004L
 FILETYPE       0x1L
 FILESUBTYPE    0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName",      "Aether"
            VALUE "FileDescription",  AETHER_RC_FILE_DESCRIPTION
            VALUE "FileVersion",      AETHERGUI_VERSION
            VALUE "InternalName",     AETHER_RC_INTERNAL_NAME
            VALUE "LegalCopyright",   "Copyright (C) Aether contributors"
            VALUE "OriginalFilename", AETHER_RC_ORIGINAL_FILENAME
            VALUE "ProductName",      "Aether"
            VALUE "ProductVersion",   AETHERGUI_VERSION
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
