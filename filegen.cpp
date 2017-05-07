//
// Build: cl.exe /Zi filegen.cpp
//

#pragma comment(lib, "advapi32.lib")

#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <wchar.h>
#include <wincrypt.h>

// Executable file name
#define EXE_FILE_NAME      L"Filegen.exe"

// Data block size
#define DATA_BLOCK_SIZE    16 * 1024 * 1024

// How to fill in the file
typedef enum
{
    UNKNOWN,     // Unknown (Undecided)
    ZERO,      // Use 0 for file padding
    RANDOM       // Use random data for file padding
} PADDING_MODE;

// Parameters structure
typedef struct
{
    LPWSTR        FilePath;
    LARGE_INTEGER FileSizeInBytes;
    PADDING_MODE  PaddingMode;
    SIZE_T        DataBlockSizeInBytes;
    SIZE_T        NumDataBlock;
    SIZE_T        RemainderDataSizeInBytes;
} Parameters;


BOOL ParseCommandLineParameter(INT argc, WCHAR *argv[], Parameters *params);
VOID PrintParameters(Parameters *params);
VOID CreateZeroDataFile(HANDLE fileHandle, LARGE_INTEGER fileSizeInBytes);
VOID CreateRandomDataFile(HANDLE fileHandle, SIZE_T dataBlockSizeInBytes,
                          SIZE_T numDataBlock, SIZE_T remainderDataSizeInBytes);
VOID GenerateRandomDataBlock(BYTE *dataBlock, SIZE_T dataBlockSizeInBytes);
BOOL WriteDataBlockToFile(HANDLE fileHandle, SIZE_T datalockSizeInBytes, SIZE_T numDataBlock);
VOID PrintFormatMessage(LPWSTR prefix, DWORD errorNo);
VOID PrintUsage();


INT
wmain(INT argc, WCHAR *argv[])
{
    Parameters params;
    if (!ParseCommandLineParameter(argc, argv, &params))
    {
        PrintUsage();
        return 0;
    }

    PrintParameters(&params);

    //
    //

    HANDLE fileHandle = CreateFile(params.FilePath, GENERIC_WRITE, 0, NULL,
                                   CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        PrintFormatMessage(L"FAILED: CreateFile()", GetLastError());
        goto Cleanup;
    }

    DWORD startTick = GetTickCount();

    switch (params.PaddingMode)
    {
        case ZERO:
            CreateZeroDataFile(fileHandle, params.FileSizeInBytes);
            break;

        case RANDOM:
            CreateRandomDataFile(fileHandle, params.DataBlockSizeInBytes,
                    params.NumDataBlock, params.RemainderDataSizeInBytes);
            break;

        default:
            wprintf(L"Invalid padding mode: %d\n", params.PaddingMode);
            break;
    }

    wprintf(L"  Elapsed: %d ms\n", GetTickCount() - startTick);

Cleanup:

    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(fileHandle);
    }

    wprintf(L"  Done.\n");

    return 0;
}


BOOL
ParseCommandLineParameter(INT argc, WCHAR *argv[], Parameters *params)
{
    if (argc < 4)
    {
        return FALSE;
    }

    // Padding mode
    if (lstrcmpi(argv[3], L"zero") == 0)  // TODO: Rewrite by CompareString function.
    {
        params->PaddingMode = ZERO;
    }
    else if (lstrcmpi(argv[3], L"random") == 0)  // TODO: Rewrite by CompareString function.
    {
        params->PaddingMode = RANDOM;
    }
    else
    {
        params->PaddingMode = UNKNOWN;
        return FALSE;
    }

    // File size
    params->FileSizeInBytes.QuadPart = _wtoi64(argv[2]);

    // Not allow minus file size
    if (params->FileSizeInBytes.QuadPart < 0)
    {
        return FALSE;
    }

    // File path
    params->FilePath = argv[1];

    // Data block size
    params->DataBlockSizeInBytes = DATA_BLOCK_SIZE;

    // Number of data blocks
    if (params->FileSizeInBytes.QuadPart > 0)
    {
        params->NumDataBlock =
                ((SIZE_T)(params->FileSizeInBytes.QuadPart)) / params->DataBlockSizeInBytes;
    }
    else
    {
        params->NumDataBlock = 0;
    }

    // Remainder data size
    params->RemainderDataSizeInBytes =
            (SIZE_T)(params->FileSizeInBytes.QuadPart) % params->DataBlockSizeInBytes;

    return TRUE;
}


VOID
PrintParameters(Parameters *params)
{
    wprintf(L"\n");
    wprintf(L"  File path     : %s\n", params->FilePath);
    wprintf(L"  File size     : %I64d bytes\n", params->FileSizeInBytes);
    wprintf(L"  Padding mode  : %s\n",
            params->PaddingMode == ZERO ? L"Zero" : L"Random");
    wprintf(L"  Block size    : %I64d bytes\n", params->DataBlockSizeInBytes);
    wprintf(L"  Blocks        : %I64d\n", params->NumDataBlock);
    wprintf(L"  Remainder size: %I64d bytes\n", params->RemainderDataSizeInBytes);
    wprintf(L"\n");
}


VOID
CreateZeroDataFile(HANDLE fileHandle, LARGE_INTEGER fileSizeInBytes)
{
    LARGE_INTEGER newFilePointerPos;
    if (!SetFilePointerEx(fileHandle, fileSizeInBytes,
                          &newFilePointerPos, FILE_BEGIN))
    {
        PrintFormatMessage(L"FAILED: SetFilePointerEx()", GetLastError());
        return;
    }

    if (!SetEndOfFile(fileHandle))
    {
        PrintFormatMessage(L"FAILED: SetEndOfFile()", GetLastError());
        return;
    }
}


VOID
CreateRandomDataFile(HANDLE fileHandle, SIZE_T dataBlockSizeInBytes,
        SIZE_T numDataBlock, SIZE_T remainderDataSizeInBytes)
{
    if (numDataBlock == 0 && remainderDataSizeInBytes == 0)
    {
        return;  // No need write data.
    }

    // Writing per data block
    if (!WriteDataBlockToFile(fileHandle, dataBlockSizeInBytes, numDataBlock))
    {
        return;
    }

    // Writing remainder data
    if (remainderDataSizeInBytes > 0)
    {
        if (!WriteDataBlockToFile(fileHandle, remainderDataSizeInBytes, 1))
        {
            return;
        }
    }
}


VOID
GenerateRandomDataBlock(BYTE *dataBlock, SIZE_T dataBlockSizeInBytes)
{

    HCRYPTPROV cryptProviderHandle;
    if (!CryptAcquireContext(&cryptProviderHandle, NULL, NULL, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
    {
        PrintFormatMessage(L"FAILED: CryptAcquireContext()", GetLastError());
        return;
    }

    if (!CryptGenRandom(cryptProviderHandle, dataBlockSizeInBytes, dataBlock))
    {
        PrintFormatMessage(L"FAILED: CryptGenRandom()", GetLastError());
        return;
    }
}


BOOL
WriteDataBlockToFile(HANDLE fileHandle, SIZE_T dataBlockSizeInBytes, SIZE_T numDataBlock)
{
    BOOL result = TRUE;

    BYTE *dataBlock = (BYTE *)VirtualAlloc(NULL, dataBlockSizeInBytes, MEM_COMMIT, PAGE_READWRITE);
    if (dataBlock == NULL)
    {
        PrintFormatMessage(L"FAILED: VirtualAlloc()", GetLastError());
        result = FALSE;
        goto Cleanup;
    }

    for (SIZE_T i = 0; i < numDataBlock; i++)
    {
        GenerateRandomDataBlock(dataBlock, dataBlockSizeInBytes);

        DWORD writtenBytes;
        if (!WriteFile(fileHandle, dataBlock, dataBlockSizeInBytes, &writtenBytes, NULL))
        {
            PrintFormatMessage(L"FAILED: WriteFile()", GetLastError());
            result = FALSE;
            goto Cleanup;
        }
    }

Cleanup:

    if (dataBlock != NULL)
    {
        if (!VirtualFree(dataBlock, dataBlockSizeInBytes, MEM_DECOMMIT))
        {
            PrintFormatMessage(L"FAILED: VirtualFree()", GetLastError());
        }
    }

    if (!result)
    {
        wprintf(L"  Generated file is invalid.\n");
    }

    return result;
}


VOID
PrintFormatMessage(LPWSTR prefix, DWORD errorNo)
{
    LPWSTR messageBuffer;
    DWORD minMessageBufferSizeInChars = 2048;
    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL, errorNo, 0, (LPWSTR)&messageBuffer, minMessageBufferSizeInChars, NULL) == 0)
    {
        wprintf(L"FAILED: FormatMessage(): %d\n", GetLastError());
        return;
    }

    wprintf(L"    %s: %d: %s\n", prefix, errorNo, messageBuffer);

    if (LocalFree(messageBuffer) != NULL)
    {
        wprintf(L"FAILED: LocalFree(): %d\n", GetLastError());
    }
}


VOID
PrintUsage()
{
    wprintf(L"\n");
    wprintf(L"  Usage: %s <FilePath> <FileSize> <FillMode>\n", EXE_FILE_NAME);
    wprintf(L"\n");
    wprintf(L"    FilePath ..... File path for generate file.\n");
    wprintf(L"    FileSize ..... File size in bytes for generate file.\n");
    wprintf(L"    FillMode\n");
    wprintf(L"        Zero   ... Generating zero padding file.\n");
    wprintf(L"        Random ... Generating random value padding file.\n");
}
